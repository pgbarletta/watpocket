#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Kernel/global_functions_3.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/convex_hull_3.h>

#include <CLI/CLI.hpp>
#include <chemfiles.hpp>

#include <internal_use_only/config.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using Point3 = Kernel::Point_3;
using Plane3 = Kernel::Plane_3;
using Mesh = CGAL::Surface_mesh<Point3>;

struct ResidueSelector {
  std::optional<std::string> chain;
  std::int64_t resid = 0;
  std::string raw;
};

struct ChainResidKey {
  std::string chain;
  std::int64_t resid = 0;

  bool operator==(const ChainResidKey& other) const noexcept
  {
    return chain == other.chain && resid == other.resid;
  }
};

struct ChainResidKeyHash {
  std::size_t operator()(const ChainResidKey& key) const noexcept
  {
    const auto h1 = std::hash<std::string>{}(key.chain);
    const auto h2 = std::hash<std::int64_t>{}(key.resid);
    return h1 ^ (h2 + 0x9e3779b9U + (h1 << 6U) + (h1 >> 2U));
  }
};

struct ResidueLookup {
  std::unordered_map<ChainResidKey, std::vector<std::size_t>, ChainResidKeyHash> by_chain_and_id;
  std::unordered_map<std::int64_t, std::vector<std::size_t>> by_id;
  std::set<std::string> chains;
};

struct HullData {
  std::vector<std::array<double, 6>> edges;
  std::vector<Plane3> halfspaces;
};

struct WaterOxygen {
  std::int64_t resid = 0;
  Point3 position;
};

std::string trim_copy(const std::string& input)
{
  const auto begin = input.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }

  const auto end = input.find_last_not_of(" \t\r\n");
  return input.substr(begin, end - begin + 1);
}

std::string lowercase_copy(std::string text)
{
  std::transform(text.begin(),
                 text.end(),
                 text.begin(),
                 [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

std::string uppercase_copy(std::string text)
{
  std::transform(text.begin(),
                 text.end(),
                 text.begin(),
                 [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return text;
}

std::int64_t parse_int64(const std::string& text)
{
  std::size_t parsed = 0;
  try {
    const auto value = std::stoll(text, &parsed, 10);
    if (parsed != text.size()) {
      throw std::runtime_error("invalid integer: '" + text + "'");
    }
    return static_cast<std::int64_t>(value);
  } catch (const std::exception&) {
    throw std::runtime_error("invalid integer: '" + text + "'");
  }
}

ResidueSelector parse_selector_token(const std::string& token)
{
  const auto raw = trim_copy(token);
  if (raw.empty()) {
    throw std::runtime_error("empty residue selector in --resnums");
  }

  const auto first_colon = raw.find(':');
  if (first_colon == std::string::npos) {
    return ResidueSelector{ std::nullopt, parse_int64(raw), raw };
  }

  if (raw.find(':', first_colon + 1) != std::string::npos) {
    throw std::runtime_error("invalid residue selector '" + raw + "' (expected CHAIN:RESID)");
  }

  const auto chain = trim_copy(raw.substr(0, first_colon));
  const auto resid = trim_copy(raw.substr(first_colon + 1));
  if (chain.empty() || resid.empty()) {
    throw std::runtime_error("invalid residue selector '" + raw + "' (expected CHAIN:RESID)");
  }

  return ResidueSelector{ chain, parse_int64(resid), raw };
}

std::vector<ResidueSelector> parse_residue_selectors(const std::string& csv)
{
  std::vector<ResidueSelector> selectors;
  std::stringstream stream(csv);
  std::string token;

  while (std::getline(stream, token, ',')) {
    selectors.push_back(parse_selector_token(token));
  }

  if (selectors.empty()) {
    throw std::runtime_error("--resnums must contain at least one residue selector");
  }

  return selectors;
}

std::string selector_display(const ResidueSelector& selector)
{
  if (selector.chain) {
    return *selector.chain + ":" + std::to_string(selector.resid);
  }
  return std::to_string(selector.resid);
}

std::string residue_chain_id(const chemfiles::Residue& residue)
{
  if (const auto chainid = residue.get<chemfiles::Property::STRING>("chainid")) {
    return *chainid;
  }

  if (const auto chainname = residue.get<chemfiles::Property::STRING>("chainname")) {
    return *chainname;
  }

  return "";
}

ResidueLookup build_residue_lookup(const chemfiles::Topology& topology)
{
  ResidueLookup lookup;
  const auto& residues = topology.residues();

  for (std::size_t residue_index = 0; residue_index < residues.size(); ++residue_index) {
    const auto& residue = residues[residue_index];
    if (!residue.id()) {
      continue;
    }

    const auto chain = residue_chain_id(residue);
    const auto resid = *residue.id();

    lookup.chains.insert(chain);
    lookup.by_chain_and_id[ChainResidKey{ chain, resid }].push_back(residue_index);
    lookup.by_id[resid].push_back(residue_index);
  }

  return lookup;
}

std::size_t find_ca_atom_index(const chemfiles::Frame& frame, const chemfiles::Residue& residue)
{
  std::optional<std::size_t> ca_atom_index;

  for (const auto atom_index : residue) {
    if (frame[atom_index].name() == "CA") {
      if (ca_atom_index) {
        throw std::runtime_error("residue has multiple 'CA' atoms");
      }
      ca_atom_index = atom_index;
    }
  }

  if (!ca_atom_index) {
    throw std::runtime_error("residue does not contain atom named 'CA'");
  }

  return *ca_atom_index;
}

std::vector<Point3> resolve_ca_points(const chemfiles::Frame& frame, const std::vector<ResidueSelector>& selectors)
{
  const auto& topology = frame.topology();
  const auto lookup = build_residue_lookup(topology);
  const auto multiple_chains = lookup.chains.size() > 1;

  if (multiple_chains) {
    for (const auto& selector : selectors) {
      if (!selector.chain) {
        throw std::runtime_error(
          "multiple chains detected; selectors must use CHAIN:RESID syntax (for example A:12)");
      }
    }
  }

  const auto& positions = frame.positions();
  std::vector<Point3> points;
  points.reserve(selectors.size());

  for (const auto& selector : selectors) {
    std::vector<std::size_t> matches;

    if (selector.chain) {
      const auto iterator = lookup.by_chain_and_id.find(ChainResidKey{ *selector.chain, selector.resid });
      if (iterator != lookup.by_chain_and_id.end()) {
        matches = iterator->second;
      }
    } else {
      const auto iterator = lookup.by_id.find(selector.resid);
      if (iterator != lookup.by_id.end()) {
        matches = iterator->second;
      }
    }

    if (matches.empty()) {
      throw std::runtime_error("could not find residue selector '" + selector_display(selector) + "'");
    }

    if (matches.size() != 1) {
      throw std::runtime_error("residue selector '" + selector_display(selector)
                               + "' matched multiple residues; add more specific selectors");
    }

    const auto residue_index = matches.front();
    const auto& residue = topology.residue(residue_index);

    std::size_t ca_atom_index = 0;
    try {
      ca_atom_index = find_ca_atom_index(frame, residue);
    } catch (const std::exception& e) {
      throw std::runtime_error("selector '" + selector_display(selector) + "': " + e.what());
    }

    const auto& position = positions.at(ca_atom_index);
    points.emplace_back(position[0], position[1], position[2]);
  }

  return points;
}

bool are_all_collinear(const std::vector<Point3>& points)
{
  if (points.size() < 3) {
    return true;
  }

  const auto& p0 = points.front();
  std::size_t second = 1;
  while (second < points.size() && points[second] == p0) {
    ++second;
  }

  if (second == points.size()) {
    return true;
  }

  const auto& p1 = points[second];
  for (std::size_t i = second + 1; i < points.size(); ++i) {
    if (!CGAL::collinear(p0, p1, points[i])) {
      return false;
    }
  }

  return true;
}

bool are_all_coplanar(const std::vector<Point3>& points)
{
  if (points.size() < 4) {
    return true;
  }

  const auto& p0 = points.front();
  std::size_t second = 1;
  while (second < points.size() && points[second] == p0) {
    ++second;
  }

  if (second == points.size()) {
    return true;
  }

  const auto& p1 = points[second];
  std::size_t third = second + 1;
  while (third < points.size() && CGAL::collinear(p0, p1, points[third])) {
    ++third;
  }

  if (third == points.size()) {
    return true;
  }

  const auto& p2 = points[third];
  for (std::size_t i = third + 1; i < points.size(); ++i) {
    if (!CGAL::coplanar(p0, p1, p2, points[i])) {
      return false;
    }
  }

  return true;
}

HullData compute_hull_data(const std::vector<Point3>& points)
{
  if (points.size() < 4) {
    throw std::runtime_error("convex hull requires at least 4 selected residues");
  }

  if (are_all_collinear(points)) {
    throw std::runtime_error("selected C-alpha points are collinear; convex hull is not 3D");
  }

  if (are_all_coplanar(points)) {
    throw std::runtime_error("selected C-alpha points are coplanar; convex hull is not 3D");
  }

  Mesh hull;
  CGAL::convex_hull_3(points.begin(), points.end(), hull);

  if (hull.number_of_edges() == 0U || hull.number_of_faces() == 0U || hull.number_of_vertices() == 0U) {
    throw std::runtime_error("failed to build a valid convex hull");
  }

  HullData hull_data;
  std::vector<std::array<double, 6>> edges;
  edges.reserve(hull.number_of_edges());

  for (const auto edge : hull.edges()) {
    const auto halfedge = hull.halfedge(edge);
    const auto source_vertex = hull.source(halfedge);
    const auto target_vertex = hull.target(halfedge);

    const auto& source_point = hull.point(source_vertex);
    const auto& target_point = hull.point(target_vertex);

    std::array<double, 3> a{ CGAL::to_double(source_point.x()),
                             CGAL::to_double(source_point.y()),
                             CGAL::to_double(source_point.z()) };
    std::array<double, 3> b{ CGAL::to_double(target_point.x()),
                             CGAL::to_double(target_point.y()),
                             CGAL::to_double(target_point.z()) };

    if (std::lexicographical_compare(b.begin(), b.end(), a.begin(), a.end())) {
      std::swap(a, b);
    }

    edges.push_back({ a[0], a[1], a[2], b[0], b[1], b[2] });
  }

  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_z = 0.0;
  std::size_t vertex_count = 0;
  for (const auto vertex : hull.vertices()) {
    const auto& point = hull.point(vertex);
    sum_x += CGAL::to_double(point.x());
    sum_y += CGAL::to_double(point.y());
    sum_z += CGAL::to_double(point.z());
    ++vertex_count;
  }
  const Point3 interior_point(sum_x / static_cast<double>(vertex_count),
                              sum_y / static_cast<double>(vertex_count),
                              sum_z / static_cast<double>(vertex_count));

  auto& halfspaces = hull_data.halfspaces;
  halfspaces.reserve(hull.number_of_faces());
  for (const auto face : hull.faces()) {
    const auto halfedge = hull.halfedge(face);
    const auto next_halfedge = hull.next(halfedge);

    const auto& p0 = hull.point(hull.source(halfedge));
    const auto& p1 = hull.point(hull.target(halfedge));
    const auto& p2 = hull.point(hull.target(next_halfedge));

    Plane3 plane(p0, p1, p2);
    if (plane.oriented_side(interior_point) == CGAL::ON_POSITIVE_SIDE) {
      plane = Plane3(p0, p2, p1);
    }

    halfspaces.push_back(plane);
  }

  hull_data.edges = std::move(edges);
  return hull_data;
}

bool point_inside_or_on_hull(const Point3& point, const std::vector<Plane3>& halfspaces)
{
  for (const auto& plane : halfspaces) {
    if (plane.oriented_side(point) == CGAL::ON_POSITIVE_SIDE) {
      return false;
    }
  }
  return true;
}

std::vector<WaterOxygen> collect_water_oxygens(const chemfiles::Frame& frame)
{
  static const std::set<std::string> water_resnames{ "HOH", "WAT", "TIP3", "TIP3P", "SPC", "SPCE" };
  static const std::set<std::string> water_oxygen_names{ "O", "OW" };

  std::vector<WaterOxygen> waters;
  const auto& topology = frame.topology();
  const auto& residues = topology.residues();
  const auto& positions = frame.positions();

  for (const auto& residue : residues) {
    if (!residue.id()) {
      continue;
    }

    if (water_resnames.count(uppercase_copy(residue.name())) == 0U) {
      continue;
    }

    for (const auto atom_index : residue) {
      if (water_oxygen_names.count(uppercase_copy(frame[atom_index].name())) == 0U) {
        continue;
      }

      const auto& position = positions.at(atom_index);
      waters.push_back(
        WaterOxygen{ *residue.id(), Point3(position[0], position[1], position[2]) });
    }
  }

  return waters;
}

std::vector<std::int64_t> find_waters_inside_hull(const chemfiles::Frame& frame, const HullData& hull_data)
{
  std::set<std::int64_t> residue_ids;
  const auto waters = collect_water_oxygens(frame);

  for (const auto& water : waters) {
    if (point_inside_or_on_hull(water.position, hull_data.halfspaces)) {
      residue_ids.insert(water.resid);
    }
  }

  return std::vector<std::int64_t>(residue_ids.begin(), residue_ids.end());
}

std::string join_residue_ids(const std::vector<std::int64_t>& residue_ids, const char separator)
{
  std::ostringstream buffer;
  for (std::size_t i = 0; i < residue_ids.size(); ++i) {
    if (i != 0U) {
      buffer << separator;
    }
    buffer << residue_ids[i];
  }
  return buffer.str();
}

std::string make_pymol_resi_selector(const std::vector<std::int64_t>& residue_ids)
{
  if (residue_ids.empty()) {
    return "none";
  }
  return "resi " + join_residue_ids(residue_ids, '+');
}

bool is_drawable_structure_path(const std::filesystem::path& path)
{
  const auto extension = lowercase_copy(path.extension().string());
  return extension == ".pdb" || extension == ".cif" || extension == ".mmcif";
}

std::string python_quoted(const std::string& text)
{
  std::string escaped;
  escaped.reserve(text.size() + 8);

  for (const char c : text) {
    if (c == '\\' || c == '"') {
      escaped.push_back('\\');
    }
    escaped.push_back(c);
  }

  return '"' + escaped + '"';
}

void write_pymol_script(const std::filesystem::path& input_path,
                        const std::filesystem::path& output_path,
                        const std::vector<std::array<double, 6>>& edges,
                        const std::vector<std::int64_t>& water_residue_ids)
{
  std::ofstream out(output_path);
  if (!out) {
    throw std::runtime_error("could not open draw output file '" + output_path.string() + "'");
  }

  out << "from pymol import cmd, cgo\n\n";
  out << "cmd.load(" << python_quoted(input_path.string()) << ", \"watpocket_input\")\n\n";
  out << "hull_obj = [\n";
  out << "    cgo.LINEWIDTH, 4.0,\n";
  out << "    cgo.BEGIN, cgo.LINES,\n";
  out << "    cgo.COLOR, 0.20, 0.80, 1.00,\n";

  out << std::fixed;
  out.precision(4);
  for (const auto& edge : edges) {
    out << "    cgo.VERTEX, " << edge[0] << ", " << edge[1] << ", " << edge[2] << ",\n";
    out << "    cgo.VERTEX, " << edge[3] << ", " << edge[4] << ", " << edge[5] << ",\n";
  }

  out << "    cgo.END,\n";
  out << "]\n\n";
  out << "cmd.load_cgo(hull_obj, \"watpocket_hull\")\n";
  out << "cmd.set(\"cgo_line_width\", 4.0, \"watpocket_hull\")\n";
  out << "cmd.show(\"cartoon\", \"watpocket_input\")\n";
  out << "cmd.select(\"watpocket\", \"watpocket_input and solvent and "
      << make_pymol_resi_selector(water_residue_ids) << "\")\n";
  out << "cmd.hide(\"everything\", \"solvent\")\n";
  out << "cmd.show(\"spheres\", \"watpocket\")\n";
  out << "cmd.set(\"sphere_scale\", 0.4, \"watpocket\")\n";
  out << "cmd.orient(\"watpocket_hull\")\n";
  out << "print(\"watpocket: loaded hull with " << edges.size() << " edges\")\n";
}

} // namespace

int main(int argc, char** argv)
{
  CLI::App app("watpocket: build a convex hull from selected residue C-alpha atoms");

  std::vector<std::string> inputs;
  std::string resnums;
  std::string draw_output;

  app.set_version_flag("--version", std::string(myproject::cmake::project_version));
  app.add_option("inputs", inputs, "Input files: <structure> or <topology> <trajectory>")
    ->required()
    ->expected(1, 2);
  app.add_option("--resnums", resnums, "Comma-separated residue selectors, e.g. 12,15 or A:12,B:18")
    ->required();
  app.add_option("-d,--draw", draw_output, "Write a PyMOL script that draws convex hull edges as CGO lines");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return app.exit(e);
  }

  try {
    if (inputs.size() == 2U) {
      throw std::runtime_error("trajectory mode (<topology> <trajectory>) is not implemented yet");
    }

    const std::filesystem::path input_path = inputs.front();
    if (!std::filesystem::exists(input_path)) {
      throw std::runtime_error("input file does not exist: '" + input_path.string() + "'");
    }

    const auto selectors = parse_residue_selectors(resnums);

    if (!draw_output.empty() && !is_drawable_structure_path(input_path)) {
      throw std::runtime_error("--draw requires a single PDB/CIF input (.pdb, .cif, .mmcif)");
    }

    chemfiles::Trajectory trajectory(input_path.string(), 'r');
    const auto frame = trajectory.read();

    const auto points = resolve_ca_points(frame, selectors);
    const auto hull_data = compute_hull_data(points);
    const auto water_residue_ids = find_waters_inside_hull(frame, hull_data);

    if (water_residue_ids.empty()) {
      std::cout << "No water oxygen atoms were found inside the convex hull.\n";
    } else {
      std::cout << "Water residues inside hull: " << join_residue_ids(water_residue_ids, ',') << '\n';
      std::cout << "select watpocket, " << make_pymol_resi_selector(water_residue_ids) << '\n';
    }

    if (!draw_output.empty()) {
      write_pymol_script(
        input_path, std::filesystem::path(draw_output), hull_data.edges, water_residue_ids);
      std::cout << "Wrote PyMOL draw script: " << draw_output << '\n';
    }

    std::cout << "Convex hull built from " << points.size() << " residues with " << hull_data.edges.size()
              << " edges.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}
