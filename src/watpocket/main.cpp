#include <CLI/CLI.hpp>

#include <internal_use_only/config.hpp>
#include <watpocket/watpocket.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::string lowercase_copy(std::string text)
{
  std::transform(
    text.begin(), text.end(), text.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

bool is_netcdf_trajectory_path(const std::filesystem::path &path)
{
  return lowercase_copy(path.extension().string()) == ".nc";
}

bool is_parm7_topology_path(const std::filesystem::path &path)
{
  const auto ext = lowercase_copy(path.extension().string());
  return ext == ".parm7" || ext == ".prmtop";
}

bool is_pymol_draw_output_path(const std::filesystem::path &path)
{
  return lowercase_copy(path.extension().string()) == ".py";
}

bool is_pdb_draw_output_path(const std::filesystem::path &path)
{
  return lowercase_copy(path.extension().string()) == ".pdb";
}

bool is_drawable_structure_path(const std::filesystem::path &path)
{
  const auto ext = lowercase_copy(path.extension().string());
  return ext == ".pdb" || ext == ".cif" || ext == ".mmcif";
}

std::string join_residue_ids(const std::vector<std::int64_t> &ids, const char sep)
{
  std::ostringstream out;
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i != 0U) { out << sep; }
    out << ids[i];
  }
  return out.str();
}

std::string make_pymol_resi_selector(const std::vector<std::int64_t> &ids)
{
  if (ids.empty()) { return "none"; }

  std::ostringstream out;
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i != 0U) { out << '+'; }
    out << ids[i];
  }
  return "resi " + out.str();
}

void write_trajectory_statistics(std::ostream &out, const watpocket::TrajectorySummary &summary)
{
  if (summary.frames_successful == 0U) {
    out << "Trajectory water-pocket statistics: no frames were processed.\n";
    return;
  }

  out << "Trajectory water-pocket statistics:\n";
  out << "  frames: " << summary.frames_successful << '\n';
  out << "  waters/frame min: " << summary.min_waters << '\n';
  out << "  waters/frame max: " << summary.max_waters << '\n';
  out << std::fixed << std::setprecision(3);
  out << "  waters/frame mean: " << summary.mean_waters << '\n';
  out << "  waters/frame median: " << summary.median_waters << '\n';
  out << "  top water resnums by frame presence:\n";

  if (summary.top_waters.empty()) {
    out << "    (none)\n";
    return;
  }

  out << std::setprecision(4);
  for (std::size_t index = 0; index < summary.top_waters.size(); ++index) {
    const auto &item = summary.top_waters[index];
    out << "    " << (index + 1U) << ". " << item.resid << " -> " << item.fraction << '\n';
  }
}

void write_trajectory_warnings(std::ostream &out, const watpocket::TrajectorySummary &summary)
{
  if (!summary.has_skipped_frames || summary.skipped_frame_warnings.empty()) { return; }

  std::vector<std::pair<std::size_t, std::string>> ordered_warnings;
  ordered_warnings.reserve(summary.skipped_frame_warnings.size());
  for (const auto &entry : summary.skipped_frame_warnings) { ordered_warnings.push_back(entry); }

  std::sort(ordered_warnings.begin(), ordered_warnings.end(), [](const auto &lhs, const auto &rhs) {
    return lhs.first < rhs.first;
  });

  for (const auto &entry : ordered_warnings) { out << entry.second << '\n'; }
}

}// namespace

int main(int argc, char **argv)
{
  CLI::App app("watpocket: build a convex hull from selected residue C-alpha atoms");

  std::vector<std::string> inputs;
  std::string resnums;
  std::string draw_output;
  std::string csv_output;

  app.set_version_flag("--version", std::string(myproject::cmake::project_version));
  app.add_option("inputs", inputs, "Input files: <structure> or <topology> <trajectory>")->required()->expected(1, 2);
  app.add_option("--resnums", resnums, "Comma-separated residue selectors, e.g. 12,15 or A:12,B:18")->required();
  app.add_option("-d,--draw",
    draw_output,
    "Write draw output. Single-structure mode: .py (PyMOL script) or .pdb (hull PDB). "
    "Trajectory mode: .pdb only (hull per frame using MODEL/ENDMDL).");
  app.add_option("-o,--output", csv_output, "Write trajectory CSV output to this path (required in trajectory mode).");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  try {
    const auto selectors = watpocket::parse_residue_selectors(resnums);

    if (inputs.size() == 2U) {
      const std::filesystem::path topology_path = inputs[0];
      const std::filesystem::path trajectory_path = inputs[1];

      if (!std::filesystem::exists(topology_path)) {
        throw watpocket::Error("topology file does not exist: '" + topology_path.string() + "'");
      }

      if (!std::filesystem::exists(trajectory_path)) {
        throw watpocket::Error("trajectory file does not exist: '" + trajectory_path.string() + "'");
      }

      if (!is_netcdf_trajectory_path(trajectory_path)) {
        throw watpocket::Error("trajectory mode currently supports NetCDF trajectories (.nc) only");
      }

      if (csv_output.empty()) {
        throw watpocket::Error("-o/--output is required in trajectory mode (<topology> <trajectory>)");
      }

      const auto has_draw_output = !draw_output.empty();
      const auto draw_path = std::filesystem::path(draw_output);
      if (has_draw_output && !is_pdb_draw_output_path(draw_path)) {
        throw watpocket::Error("trajectory mode --draw output path must end with .pdb");
      }

      const auto summary = watpocket::analyze_trajectory_files(topology_path,
        trajectory_path,
        selectors,
        std::filesystem::path(csv_output),
        has_draw_output ? std::optional(draw_path) : std::nullopt);

      write_trajectory_warnings(std::cerr, summary);

      if (has_draw_output) { std::cout << "Wrote trajectory hull PDB: " << draw_output << '\n'; }

      std::cout << "Wrote trajectory CSV: " << csv_output << '\n';
      write_trajectory_statistics(std::cout, summary);
      std::cout << "Processed " << summary.frames_processed << " trajectory frames (" << summary.frames_successful
                << " successful, " << summary.frames_skipped << " skipped).\n";
      return 0;
    }

    const std::filesystem::path input_path = inputs.front();
    if (!std::filesystem::exists(input_path)) {
      throw watpocket::Error("input file does not exist: '" + input_path.string() + "'");
    }

    if (is_parm7_topology_path(input_path)) {
      throw watpocket::Error(
        "parm7/prmtop topology files are only supported in trajectory mode: "
        "watpocket <topology.parm7> <trajectory.nc> --resnums ...");
    }

    if (!draw_output.empty()) {
      const auto draw_path = std::filesystem::path(draw_output);
      const auto draw_is_py = is_pymol_draw_output_path(draw_path);
      const auto draw_is_pdb = is_pdb_draw_output_path(draw_path);
      if (!draw_is_py && !draw_is_pdb) { throw watpocket::Error("--draw output path must end with .py or .pdb"); }

      if (draw_is_py && !is_drawable_structure_path(input_path)) {
        throw watpocket::Error("--draw .py output requires a single PDB/CIF input (.pdb, .cif, .mmcif)");
      }
    }

    if (!csv_output.empty()) {
      throw watpocket::Error("-o/--output is only supported in trajectory mode (<topology> <trajectory>)");
    }

    const auto result = watpocket::analyze_structure_file(input_path, selectors);

    if (result.water_residue_ids.empty()) {
      std::cout << "No water oxygen atoms were found inside the convex hull.\n";
    } else {
      std::cout << "Water residues inside hull: " << join_residue_ids(result.water_residue_ids, ',') << '\n';
      std::cout << "select watpocket, " << make_pymol_resi_selector(result.water_residue_ids) << '\n';
    }

    if (!draw_output.empty()) {
      const auto draw_path = std::filesystem::path(draw_output);
      if (is_pymol_draw_output_path(draw_path)) {
        watpocket::write_pymol_draw_script(input_path, draw_path, result.hull.edges, result.water_residue_ids);
        std::cout << "Wrote PyMOL draw script: " << draw_output << '\n';
      } else if (is_pdb_draw_output_path(draw_path)) {
        watpocket::write_hull_pdb(draw_path, result.hull, result.water_atoms_for_pdb);
        std::cout << "Wrote hull PDB: " << draw_output << '\n';
      }
    }

    std::cout << "Convex hull built from " << selectors.size() << " residues with " << result.hull.edges.size()
              << " edges.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}
