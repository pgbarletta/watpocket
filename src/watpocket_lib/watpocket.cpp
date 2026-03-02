#include <chemfiles.hpp>

#include <internal_use_only/config.hpp>
#include <watpocket/watpocket.hpp>

#include "point_soa_cgal_adapter.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace watpocket {

namespace {

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

struct WaterOxygenRef {
  std::int64_t resid = 0;
  std::size_t atom_index = 0;
};

struct Parm7FormatSpec {
  char type = '\0';
  std::size_t width = 0;
};

struct Parm7Section {
  Parm7FormatSpec format;
  std::vector<std::string> lines;
};

struct Parm7Topology {
  std::vector<std::string> atom_names;
  std::vector<std::string> residue_labels;
  std::vector<std::size_t> residue_pointers;
  std::vector<std::size_t> residue_end_offsets;
  std::vector<std::size_t> atom_to_residue_index;
  std::vector<std::string> residue_chain_ids;
  bool has_residue_chain_ids = false;
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

Parm7FormatSpec parse_parm7_format_line(const std::string& line)
{
  static const std::regex pattern(R"(\(\s*\d+\s*([A-Za-z])\s*(\d+)(?:\.\d+)?\s*\))");
  std::smatch match;

  if (!std::regex_search(line, match, pattern)) {
    throw std::runtime_error("invalid parm7 %FORMAT line: '" + line + "'");
  }

  const auto type = static_cast<char>(std::tolower(static_cast<unsigned char>(match[1].str().front())));
  const auto width = std::stoul(match[2].str());
  if (width == 0U) {
    throw std::runtime_error("invalid parm7 %FORMAT width in line: '" + line + "'");
  }

  return Parm7FormatSpec{ type, width };
}

std::vector<std::string> decode_parm7_string_fields(const Parm7Section& section)
{
  std::vector<std::string> values;
  values.reserve(section.lines.size() * 4U);

  const auto width = section.format.width;
  for (const auto& line : section.lines) {
    for (std::size_t offset = 0; offset < line.size(); offset += width) {
      const auto count = std::min(width, line.size() - offset);
      values.push_back(trim_copy(line.substr(offset, count)));
    }
  }

  return values;
}

std::vector<std::int64_t> decode_parm7_integer_fields(const Parm7Section& section)
{
  std::vector<std::int64_t> values;
  values.reserve(section.lines.size() * 4U);

  const auto width = section.format.width;
  for (const auto& line : section.lines) {
    for (std::size_t offset = 0; offset < line.size(); offset += width) {
      const auto count = std::min(width, line.size() - offset);
      const auto token = trim_copy(line.substr(offset, count));
      if (token.empty()) {
        continue;
      }
      values.push_back(parse_int64(token));
    }
  }

  return values;
}

const Parm7Section& require_parm7_section(const std::unordered_map<std::string, Parm7Section>& sections,
                                          const std::string& section_name)
{
  const auto iterator = sections.find(section_name);
  if (iterator == sections.end()) {
    throw std::runtime_error("parm7 section '" + section_name + "' is missing");
  }
  return iterator->second;
}

std::vector<std::string> parse_parm7_string_section(const std::unordered_map<std::string, Parm7Section>& sections,
                                                    const std::string& section_name,
                                                    const std::size_t expected_count)
{
  const auto& section = require_parm7_section(sections, section_name);
  if (section.format.type != 'a') {
    throw std::runtime_error("parm7 section '" + section_name
                             + "' has unexpected format type; expected character ('a')");
  }

  auto values = decode_parm7_string_fields(section);
  if (values.size() < expected_count) {
    throw std::runtime_error("parm7 section '" + section_name + "' has " + std::to_string(values.size())
                             + " values, expected at least " + std::to_string(expected_count));
  }

  if (values.size() > expected_count) {
    for (std::size_t i = expected_count; i < values.size(); ++i) {
      if (!values[i].empty()) {
        throw std::runtime_error("parm7 section '" + section_name + "' has unexpected extra values");
      }
    }
    values.resize(expected_count);
  }

  return values;
}

std::vector<std::int64_t> parse_parm7_integer_section(const std::unordered_map<std::string, Parm7Section>& sections,
                                                      const std::string& section_name,
                                                      const std::size_t expected_min_count)
{
  const auto& section = require_parm7_section(sections, section_name);
  if (section.format.type != 'i') {
    throw std::runtime_error("parm7 section '" + section_name
                             + "' has unexpected format type; expected integer ('i')");
  }

  auto values = decode_parm7_integer_fields(section);
  if (values.size() < expected_min_count) {
    throw std::runtime_error("parm7 section '" + section_name + "' has " + std::to_string(values.size())
                             + " integer values, expected at least " + std::to_string(expected_min_count));
  }

  return values;
}

std::unordered_map<std::string, Parm7Section> parse_parm7_sections(const std::filesystem::path& parm7_path)
{
  std::ifstream input(parm7_path);
  if (!input) {
    throw std::runtime_error("could not open parm7 topology file '" + parm7_path.string() + "'");
  }

  std::unordered_map<std::string, Parm7Section> sections;
  std::optional<std::string> current_flag;

  std::string line;
  while (std::getline(input, line)) {
    const auto trimmed = trim_copy(line);
    if (trimmed.rfind("%FLAG", 0) == 0U) {
      current_flag = trim_copy(trimmed.substr(5));
      sections[*current_flag] = Parm7Section{};
      continue;
    }

    if (trimmed.rfind("%FORMAT", 0) == 0U) {
      if (!current_flag) {
        throw std::runtime_error("parm7 file has %FORMAT before %FLAG");
      }
      sections[*current_flag].format = parse_parm7_format_line(trimmed);
      continue;
    }

    if (current_flag) {
      sections[*current_flag].lines.push_back(line);
    }
  }

  return sections;
}

Parm7Topology parse_parm7_topology(const std::filesystem::path& parm7_path)
{
  auto sections = parse_parm7_sections(parm7_path);

  const auto pointers = parse_parm7_integer_section(sections, "POINTERS", 1U);
  if (pointers.size() < 12U) {
    throw std::runtime_error("parm7 POINTERS section has too few values");
  }

  const auto natom = static_cast<std::size_t>(pointers.at(0));
  const auto nres = static_cast<std::size_t>(pointers.at(11));
  if (natom == 0U) {
    throw std::runtime_error("parm7 POINTERS indicates NATOM is zero");
  }
  if (nres == 0U) {
    throw std::runtime_error("parm7 POINTERS indicates NRES is zero");
  }

  Parm7Topology topology;
  topology.atom_names = parse_parm7_string_section(sections, "ATOM_NAME", natom);
  topology.residue_labels = parse_parm7_string_section(sections, "RESIDUE_LABEL", nres);

  const auto residue_pointers_i = parse_parm7_integer_section(sections, "RESIDUE_POINTER", nres);
  topology.residue_pointers.reserve(nres);
  for (const auto v : residue_pointers_i) {
    if (v <= 0) {
      throw std::runtime_error("parm7 RESIDUE_POINTER contains non-positive value");
    }
    topology.residue_pointers.push_back(static_cast<std::size_t>(v - 1));
  }

  topology.residue_end_offsets.resize(nres);
  for (std::size_t residue_index = 0; residue_index < nres; ++residue_index) {
    const auto begin = topology.residue_pointers.at(residue_index);
    const auto end = (residue_index + 1U < nres) ? topology.residue_pointers.at(residue_index + 1U) : natom;
    if (begin > end || end > natom) {
      throw std::runtime_error("parm7 RESIDUE_POINTER defines invalid residue range");
    }
    topology.residue_end_offsets[residue_index] = end;
  }

  topology.atom_to_residue_index.resize(natom);
  for (std::size_t residue_index = 0; residue_index < nres; ++residue_index) {
    const auto begin = topology.residue_pointers.at(residue_index);
    const auto end = topology.residue_end_offsets.at(residue_index);
    for (std::size_t atom_index = begin; atom_index < end; ++atom_index) {
      topology.atom_to_residue_index[atom_index] = residue_index;
    }
  }

  if (sections.count("RESIDUE_CHAINID") != 0U) {
    topology.residue_chain_ids = parse_parm7_string_section(sections, "RESIDUE_CHAINID", nres);
    topology.has_residue_chain_ids = true;
  } else {
    topology.residue_chain_ids.assign(nres, "");
    topology.has_residue_chain_ids = false;
  }

  return topology;
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

std::vector<std::size_t> resolve_ca_atom_indices(const chemfiles::Frame& frame,
                                                 const std::vector<ResidueSelector>& selectors)
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

  std::vector<std::size_t> atom_indices;
  atom_indices.reserve(selectors.size());

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

    atom_indices.push_back(ca_atom_index);
  }

  return atom_indices;
}

std::size_t find_ca_atom_index(const Parm7Topology& topology, const std::size_t residue_index)
{
  std::optional<std::size_t> ca_atom_index;

  const auto begin = topology.residue_pointers.at(residue_index);
  const auto end = topology.residue_end_offsets.at(residue_index);

  for (std::size_t atom_index = begin; atom_index < end; ++atom_index) {
    if (uppercase_copy(topology.atom_names.at(atom_index)) == "CA") {
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

std::vector<std::size_t> resolve_ca_atom_indices(const Parm7Topology& topology,
                                                 const std::vector<ResidueSelector>& selectors)
{
  std::vector<std::size_t> atom_indices;
  atom_indices.reserve(selectors.size());

  for (const auto& selector : selectors) {
    if (selector.chain && !topology.has_residue_chain_ids) {
      throw std::runtime_error("selector '" + selector_display(selector)
                               + "': parm7 topology has no RESIDUE_CHAINID section; chain-qualified selectors are not supported");
    }

    if (selector.resid <= 0) {
      throw std::runtime_error("selector '" + selector_display(selector) + "': residue id must be positive for parm7 topology");
    }

    const auto residue_index = static_cast<std::size_t>(selector.resid - 1);
    if (residue_index >= topology.residue_labels.size()) {
      throw std::runtime_error("could not find residue selector '" + selector_display(selector) + "'");
    }

    if (selector.chain) {
      const auto chain_id = uppercase_copy(*selector.chain);
      if (uppercase_copy(topology.residue_chain_ids.at(residue_index)) != chain_id) {
        throw std::runtime_error("could not find residue selector '" + selector_display(selector) + "'");
      }
    }

    std::size_t ca_atom_index = 0;
    try {
      ca_atom_index = find_ca_atom_index(topology, residue_index);
    } catch (const std::exception& e) {
      throw std::runtime_error("selector '" + selector_display(selector) + "': " + e.what());
    }

    atom_indices.push_back(ca_atom_index);
  }

  return atom_indices;
}

void fill_points_from_atom_indices(const chemfiles::Frame& frame,
                                   const std::vector<std::size_t>& atom_indices,
                                   const std::string& label,
                                   PointSoA& points)
{
  points.resize(atom_indices.size());

  const auto& positions = frame.positions();
  auto point_view = points.mutable_view();
  for (std::size_t i = 0; i < atom_indices.size(); ++i) {
    const auto atom_index = atom_indices[i];
    if (atom_index >= positions.size()) {
      throw std::runtime_error(label + " atom index " + std::to_string(atom_index)
                               + " is out of bounds for frame with "
                               + std::to_string(positions.size()) + " atoms");
    }

    const auto& position = positions.at(atom_index);
    point_view.set_point(i, position[0], position[1], position[2]);
  }
}

PointSoA resolve_ca_points(const chemfiles::Frame& frame, const std::vector<ResidueSelector>& selectors)
{
  const auto atom_indices = resolve_ca_atom_indices(frame, selectors);
  PointSoA points;
  fill_points_from_atom_indices(frame, atom_indices, "selected C-alpha", points);
  return points;
}

std::vector<WaterOxygenRef> collect_water_oxygen_refs(const chemfiles::Frame& frame)
{
  static const std::set<std::string> water_resnames{ "HOH", "WAT", "TIP3", "TIP3P", "SPC", "SPCE" };
  static const std::set<std::string> water_oxygen_names{ "O", "OW" };

  std::vector<WaterOxygenRef> water_refs;
  const auto& residues = frame.topology().residues();

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

      water_refs.push_back(WaterOxygenRef{ *residue.id(), atom_index });
    }
  }

  return water_refs;
}

std::vector<WaterOxygenRef> collect_water_oxygen_refs(const Parm7Topology& topology)
{
  static const std::set<std::string> water_resnames{ "HOH", "WAT", "TIP3", "TIP3P", "SPC", "SPCE" };
  static const std::set<std::string> water_oxygen_names{ "O", "OW" };

  std::vector<WaterOxygenRef> water_refs;
  const auto residue_count = topology.residue_labels.size();

  for (std::size_t residue_index = 0; residue_index < residue_count; ++residue_index) {
    if (water_resnames.count(uppercase_copy(topology.residue_labels[residue_index])) == 0U) {
      continue;
    }

    const auto residue_id = static_cast<std::int64_t>(residue_index + 1U);
    const auto begin = topology.residue_pointers[residue_index];
    const auto end = topology.residue_end_offsets[residue_index];
    for (std::size_t atom_index = begin; atom_index < end; ++atom_index) {
      if (water_oxygen_names.count(uppercase_copy(topology.atom_names[atom_index])) == 0U) {
        continue;
      }

      water_refs.push_back(WaterOxygenRef{ residue_id, atom_index });
    }
  }

  return water_refs;
}

std::vector<std::int64_t> find_waters_inside_hull(const chemfiles::Frame& frame,
                                                  const detail::HullData& hull_data,
                                                  const std::vector<WaterOxygenRef>& water_refs)
{
  std::set<std::int64_t> residue_ids;
  const auto& positions = frame.positions();

  for (const auto& water_ref : water_refs) {
    if (water_ref.atom_index >= positions.size()) {
      throw std::runtime_error("water oxygen atom index " + std::to_string(water_ref.atom_index)
                               + " is out of bounds for frame with "
                               + std::to_string(positions.size()) + " atoms");
    }
    const auto& position = positions.at(water_ref.atom_index);
    if (detail::point_inside_or_on_hull(position[0], position[1], position[2], hull_data.halfspaces)) {
      residue_ids.insert(water_ref.resid);
    }
  }

  return std::vector<std::int64_t>(residue_ids.begin(), residue_ids.end());
}

std::string normalize_pdb_field(const std::string& text, const std::size_t width)
{
  auto normalized = trim_copy(text);
  if (normalized.size() > width) {
    normalized.resize(width);
  }
  return normalized;
}

std::string guess_element_symbol(const std::string& atom_name)
{
  for (const char c : atom_name) {
    if (std::isalpha(static_cast<unsigned char>(c))) {
      return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
  }
  return "X";
}

void write_pdb_atom_record(std::ostream& out, const std::size_t serial, const PdbAtomRecord& atom)
{
  const auto atom_name = normalize_pdb_field(atom.atom_name, 4U);
  const auto residue_name = normalize_pdb_field(atom.residue_name, 3U);
  const auto chain_id = atom.chain_id.empty() ? " " : normalize_pdb_field(atom.chain_id, 1U);
  const auto element = normalize_pdb_field(atom.element.empty() ? guess_element_symbol(atom.atom_name) : atom.element, 2U);

  char atom_line[128];
  std::snprintf(atom_line,
                sizeof(atom_line),
                "ATOM  %5zu %4s %3s %1s%4lld    %8.3f%8.3f%8.3f%6.2f%6.2f          %2s",
                serial,
                atom_name.c_str(),
                residue_name.c_str(),
                chain_id.c_str(),
                static_cast<long long>(atom.residue_id),
                atom.position[0],
                atom.position[1],
                atom.position[2],
                1.00,
                0.00,
                element.c_str());
  out << atom_line << '\n';
}

std::vector<PdbAtomRecord> collect_water_atoms_for_pdb(const chemfiles::Frame& frame,
                                                       const std::vector<std::int64_t>& water_residue_ids)
{
  static const std::set<std::string> water_resnames{ "HOH", "WAT", "TIP3", "TIP3P", "SPC", "SPCE" };

  std::vector<PdbAtomRecord> water_atoms;
  if (water_residue_ids.empty()) {
    return water_atoms;
  }

  const auto& residues = frame.topology().residues();
  const auto& positions = frame.positions();
  for (const auto& residue : residues) {
    if (!residue.id()) {
      continue;
    }

    const auto residue_id = *residue.id();
    if (!std::binary_search(water_residue_ids.begin(), water_residue_ids.end(), residue_id)) {
      continue;
    }

    const auto residue_name = residue.name();
    if (water_resnames.count(uppercase_copy(residue_name)) == 0U) {
      continue;
    }

    const auto chain_id = residue_chain_id(residue);
    for (const auto atom_index : residue) {
      if (atom_index >= positions.size()) {
        continue;
      }

      const auto& pos = positions.at(atom_index);
      water_atoms.push_back(PdbAtomRecord{ frame[atom_index].name(),
                                           residue_name,
                                           chain_id,
                                           residue_id,
                                           { pos[0], pos[1], pos[2] },
                                           "" });
    }
  }

  return water_atoms;
}

std::vector<PdbAtomRecord> collect_water_atoms_for_pdb(const chemfiles::Frame& frame,
                                                       const Parm7Topology& topology,
                                                       const std::vector<std::int64_t>& water_residue_ids)
{
  static const std::set<std::string> water_resnames{ "HOH", "WAT", "TIP3", "TIP3P", "SPC", "SPCE" };

  std::vector<PdbAtomRecord> water_atoms;
  if (water_residue_ids.empty()) {
    return water_atoms;
  }

  const auto& positions = frame.positions();

  for (const auto resid : water_residue_ids) {
    const auto residue_index = static_cast<std::size_t>(resid - 1);
    if (residue_index >= topology.residue_labels.size()) {
      continue;
    }

    const auto residue_name = topology.residue_labels[residue_index];
    if (water_resnames.count(uppercase_copy(residue_name)) == 0U) {
      continue;
    }

    const auto chain_id = topology.residue_chain_ids[residue_index];
    const auto begin = topology.residue_pointers[residue_index];
    const auto end = topology.residue_end_offsets[residue_index];

    for (std::size_t atom_index = begin; atom_index < end; ++atom_index) {
      if (atom_index >= positions.size()) {
        continue;
      }
      const auto& pos = positions.at(atom_index);
      water_atoms.push_back(PdbAtomRecord{ topology.atom_names[atom_index],
                                           residue_name,
                                           chain_id,
                                           resid,
                                           { pos[0], pos[1], pos[2] },
                                           "" });
    }
  }

  return water_atoms;
}

std::string python_quoted(const std::string& text)
{
  std::string out;
  out.reserve(text.size() + 2U);
  out.push_back('"');
  for (const auto c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

std::string make_pymol_resi_selector(const std::vector<std::int64_t>& ids)
{
  if (ids.empty()) {
    return "none";
  }
  std::ostringstream out;
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i != 0U) {
      out << '+';
    }
    out << ids[i];
  }
  return "resi " + out.str();
}

bool is_netcdf_trajectory_path(const std::filesystem::path& path)
{
  return lowercase_copy(path.extension().string()) == ".nc";
}

bool is_parm7_topology_path(const std::filesystem::path& path)
{
  const auto ext = lowercase_copy(path.extension().string());
  return ext == ".parm7" || ext == ".prmtop";
}

bool is_pdb_draw_output_path(const std::filesystem::path& path)
{
  return lowercase_copy(path.extension().string()) == ".pdb";
}

void write_csv_header(std::ostream& out) { out << "frame,resnums,total_count\n"; }

void write_csv_row(std::ostream& out, const std::size_t frame_number, const std::vector<std::int64_t>& water_residue_ids)
{
  out << frame_number << ",\"";
  for (std::size_t i = 0; i < water_residue_ids.size(); ++i) {
    if (i != 0U) {
      out << ' ';
    }
    out << water_residue_ids[i];
  }
  out << "\"," << water_residue_ids.size() << "\n";
}

double median_of(std::vector<std::size_t> values)
{
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const auto mid = values.size() / 2U;
  if ((values.size() % 2U) == 1U) {
    return static_cast<double>(values[mid]);
  }
  return 0.5 * (static_cast<double>(values[mid - 1U]) + static_cast<double>(values[mid]));
}

TrajectorySummary summarize_trajectory(const std::vector<std::size_t>& waters_per_frame,
                                      const std::unordered_map<std::int64_t, std::size_t>& water_presence_counts)
{
  TrajectorySummary summary;

  if (!waters_per_frame.empty()) {
    summary.min_waters = *std::min_element(waters_per_frame.begin(), waters_per_frame.end());
    summary.max_waters = *std::max_element(waters_per_frame.begin(), waters_per_frame.end());
    summary.mean_waters = static_cast<double>(std::accumulate(waters_per_frame.begin(), waters_per_frame.end(), std::size_t{ 0 }))
                          / static_cast<double>(waters_per_frame.size());
    summary.median_waters = median_of(waters_per_frame);
  }

  std::vector<TrajectoryWaterPresence> presences;
  presences.reserve(water_presence_counts.size());
  for (const auto& [resid, count] : water_presence_counts) {
    presences.push_back(TrajectoryWaterPresence{ resid,
                                                 count,
                                                 waters_per_frame.empty()
                                                   ? 0.0
                                                   : static_cast<double>(count) / static_cast<double>(waters_per_frame.size()) });
  }

  std::sort(presences.begin(),
            presences.end(),
            [](const auto& a, const auto& b) {
              if (a.frames_present != b.frames_present) {
                return a.frames_present > b.frames_present;
              }
              return a.resid < b.resid;
            });

  const auto keep = std::min<std::size_t>(5U, presences.size());
  presences.resize(keep);
  summary.top_waters = std::move(presences);

  return summary;
}

void write_hull_pdb_records(std::ostream& out,
                            const HullGeometry& hull,
                            const std::vector<PdbAtomRecord>& water_atoms)
{
  std::size_t serial = 1;

  for (const auto& vertex : hull.vertices) {
    write_pdb_atom_record(out,
                          serial++,
                          PdbAtomRecord{ "C",
                                         "ANA",
                                         "",
                                         1,
                                         vertex,
                                         "C" });
  }

  // CONECT records for hull bonds. PDB is 1-based serial indexing.
  for (const auto& bond : hull.bonds) {
    out << "CONECT" << std::setw(5) << (bond.first + 1U) << std::setw(5) << (bond.second + 1U) << "\n";
  }

  for (const auto& atom : water_atoms) {
    write_pdb_atom_record(out, serial++, atom);
  }
}

void write_hull_pdb_model(std::ostream& out,
                          const HullGeometry& hull,
                          const std::size_t model_number,
                          const std::vector<PdbAtomRecord>& water_atoms)
{
  out << "MODEL     " << model_number << "\n";
  write_hull_pdb_records(out, hull, water_atoms);
  out << "ENDMDL\n";
}

} // namespace

std::string_view build_version() noexcept
{
  return myproject::cmake::project_version;
}

std::vector<ResidueSelector> parse_residue_selectors(std::string_view csv)
{
  try {
    std::vector<ResidueSelector> selectors;
    std::stringstream stream{ std::string(csv) };
    std::string token;

    while (std::getline(stream, token, ',')) {
      selectors.push_back(parse_selector_token(token));
    }

    if (selectors.empty()) {
      throw std::runtime_error("--resnums must contain at least one residue selector");
    }

    return selectors;
  } catch (const Error&) {
    throw;
  } catch (const std::exception& e) {
    throw Error(e.what());
  }
}

PointSoA read_structure_points_by_atom_indices(const std::filesystem::path& input_path,
                                               const std::vector<std::size_t>& atom_indices,
                                               std::string_view label)
{
  try {
    if (!std::filesystem::exists(input_path)) {
      throw Error("input file does not exist: '" + input_path.string() + "'");
    }

    if (is_parm7_topology_path(input_path)) {
      throw Error(
        "parm7/prmtop topology files are only supported in trajectory mode: "
        "watpocket <topology.parm7> <trajectory.nc> --resnums ...");
    }

    chemfiles::Trajectory trajectory(input_path.string(), 'r');
    const auto frame = trajectory.read();

    PointSoA points;
    fill_points_from_atom_indices(frame, atom_indices, std::string(label), points);
    return points;
  } catch (const Error&) {
    throw;
  } catch (const std::exception& e) {
    throw Error(e.what());
  }
}

PointSoA read_trajectory_points_by_atom_indices(const std::filesystem::path& topology_path,
                                                const std::filesystem::path& trajectory_path,
                                                const std::size_t frame_number,
                                                const std::vector<std::size_t>& atom_indices,
                                                std::string_view label)
{
  try {
    if (!std::filesystem::exists(topology_path)) {
      throw Error("topology file does not exist: '" + topology_path.string() + "'");
    }

    if (!std::filesystem::exists(trajectory_path)) {
      throw Error("trajectory file does not exist: '" + trajectory_path.string() + "'");
    }

    if (!is_netcdf_trajectory_path(trajectory_path)) {
      throw Error("trajectory mode currently supports NetCDF trajectories (.nc) only");
    }

    if (frame_number == 0U) {
      throw Error("frame number must be >= 1");
    }

    const auto topology_is_parm7 = is_parm7_topology_path(topology_path);
    std::size_t topology_atom_count = 0;
    if (topology_is_parm7) {
      const auto parm7_topology = parse_parm7_topology(topology_path);
      topology_atom_count = parm7_topology.atom_names.size();
    } else {
      chemfiles::Trajectory topology_trajectory(topology_path.string(), 'r');
      const auto topology_frame = topology_trajectory.read();
      topology_atom_count = topology_frame.size();
    }

    chemfiles::Trajectory trajectory(trajectory_path.string(), 'r');
    if (!topology_is_parm7) {
      trajectory.set_topology(topology_path.string());
    }

    const auto frame_count = trajectory.size();
    if (frame_count == 0U) {
      throw Error("trajectory contains no frames: '" + trajectory_path.string() + "'");
    }

    if (frame_number > frame_count) {
      throw Error("requested frame " + std::to_string(frame_number) + " is out of range for trajectory with "
                  + std::to_string(frame_count) + " frames");
    }

    const auto frame = trajectory.read_at(frame_number - 1U);
    if (frame.size() != topology_atom_count) {
      throw Error("atom-count mismatch between topology and trajectory: topology has "
                  + std::to_string(topology_atom_count) + " atoms, trajectory frame "
                  + std::to_string(frame_number) + " has " + std::to_string(frame.size()) + " atoms");
    }

    PointSoA points;
    fill_points_from_atom_indices(frame, atom_indices, std::string(label), points);
    return points;
  } catch (const Error&) {
    throw;
  } catch (const std::exception& e) {
    throw Error(e.what());
  }
}

StructureAnalysisResult analyze_structure_file(const std::filesystem::path& input_path,
                                              const std::vector<ResidueSelector>& selectors)
{
  try {
    if (!std::filesystem::exists(input_path)) {
      throw Error("input file does not exist: '" + input_path.string() + "'");
    }

    if (is_parm7_topology_path(input_path)) {
      throw Error(
        "parm7/prmtop topology files are only supported in trajectory mode: "
        "watpocket <topology.parm7> <trajectory.nc> --resnums ...");
    }

    chemfiles::Trajectory trajectory(input_path.string(), 'r');
    const auto frame = trajectory.read();

    StructureAnalysisResult result;
    const auto points = resolve_ca_points(frame, selectors);
    const auto hull_data = detail::compute_hull_data(points.view());
    result.hull = hull_data.geometry;
    const auto water_refs = collect_water_oxygen_refs(frame);
    result.water_residue_ids = find_waters_inside_hull(frame, hull_data, water_refs);
    result.water_atoms_for_pdb = collect_water_atoms_for_pdb(frame, result.water_residue_ids);
    return result;
  } catch (const Error&) {
    throw;
  } catch (const std::exception& e) {
    throw Error(e.what());
  }
}

TrajectorySummary analyze_trajectory_files(const std::filesystem::path& topology_path,
                                          const std::filesystem::path& trajectory_path,
                                          const std::vector<ResidueSelector>& selectors,
                                          const std::optional<std::filesystem::path>& csv_output,
                                          const std::optional<std::filesystem::path>& draw_output_pdb,
                                          const TrajectoryCallbacks& callbacks)
{
  try {
    if (!std::filesystem::exists(topology_path)) {
      throw Error("topology file does not exist: '" + topology_path.string() + "'");
    }

    if (!std::filesystem::exists(trajectory_path)) {
      throw Error("trajectory file does not exist: '" + trajectory_path.string() + "'");
    }

    if (!is_netcdf_trajectory_path(trajectory_path)) {
      throw Error("trajectory mode currently supports NetCDF trajectories (.nc) only");
    }

    const auto topology_is_parm7 = is_parm7_topology_path(topology_path);
    std::optional<Parm7Topology> parm7_topology;
    std::size_t topology_atom_count = 0;
    std::vector<std::size_t> ca_atom_indices;
    std::vector<WaterOxygenRef> water_refs;

    if (topology_is_parm7) {
      parm7_topology = parse_parm7_topology(topology_path);
      topology_atom_count = parm7_topology->atom_names.size();
      ca_atom_indices = resolve_ca_atom_indices(*parm7_topology, selectors);
      water_refs = collect_water_oxygen_refs(*parm7_topology);
    } else {
      chemfiles::Trajectory topology_trajectory(topology_path.string(), 'r');
      const auto topology_frame = topology_trajectory.read();
      topology_atom_count = topology_frame.size();
      ca_atom_indices = resolve_ca_atom_indices(topology_frame, selectors);
      water_refs = collect_water_oxygen_refs(topology_frame);
    }

    chemfiles::Trajectory sanity_trajectory(trajectory_path.string(), 'r');
    if (sanity_trajectory.size() == 0U) {
      throw Error("trajectory contains no frames: '" + trajectory_path.string() + "'");
    }

    const auto sanity_frame = sanity_trajectory.read();
    if (sanity_frame.size() != topology_atom_count) {
      throw Error("atom-count mismatch between topology and trajectory: topology has "
                  + std::to_string(topology_atom_count) + " atoms, trajectory has "
                  + std::to_string(sanity_frame.size()) + " atoms");
    }

    chemfiles::Trajectory trajectory(trajectory_path.string(), 'r');
    if (!topology_is_parm7) {
      trajectory.set_topology(topology_path.string());
    }

    std::ofstream csv_file;
    if (csv_output) {
      csv_file.open(*csv_output);
      if (!csv_file) {
        throw Error("could not open output file '" + csv_output->string() + "'");
      }
      write_csv_header(csv_file);
    }

    std::ofstream draw_file;
    if (draw_output_pdb) {
      if (!is_pdb_draw_output_path(*draw_output_pdb)) {
        throw Error("trajectory mode --draw output path must end with .pdb");
      }

      draw_file.open(*draw_output_pdb);
      if (!draw_file) {
        throw Error("could not open draw output file '" + draw_output_pdb->string() + "'");
      }
      draw_file << "REMARK   Generated by watpocket --draw trajectory (.pdb MODEL output)\n";
    }

    std::vector<std::size_t> waters_per_frame;
    std::unordered_map<std::int64_t, std::size_t> water_presence_counts;
    PointSoA ca_points_buffer;

    std::size_t frame_number = 0;
    std::size_t successful_frames = 0;
    std::size_t skipped_frames = 0;

    while (!trajectory.done()) {
      const auto current_frame_number = frame_number + 1U;
      chemfiles::Frame frame;
      try {
        frame = trajectory.read();
      } catch (const std::exception& e) {
        throw Error("frame " + std::to_string(current_frame_number) + ": " + e.what());
      }
      frame_number = current_frame_number;

      try {
        if (frame.size() != topology_atom_count) {
          throw std::runtime_error("atom-count mismatch: expected "
                                   + std::to_string(topology_atom_count)
                                   + " atoms from topology but frame has "
                                   + std::to_string(frame.size()));
        }

        fill_points_from_atom_indices(frame, ca_atom_indices, "selected C-alpha", ca_points_buffer);
        const auto hull_data = detail::compute_hull_data(ca_points_buffer.view());
        const auto water_residue_ids = find_waters_inside_hull(frame, hull_data, water_refs);

        if (csv_output) {
          write_csv_row(csv_file, current_frame_number, water_residue_ids);
        }

        if (draw_output_pdb) {
          std::vector<PdbAtomRecord> water_atoms;
          if (topology_is_parm7) {
            water_atoms = collect_water_atoms_for_pdb(frame, *parm7_topology, water_residue_ids);
          } else {
            water_atoms = collect_water_atoms_for_pdb(frame, water_residue_ids);
          }
          write_hull_pdb_model(draw_file, hull_data.geometry, current_frame_number, water_atoms);
        }

        if (callbacks.on_frame) {
          callbacks.on_frame(callbacks.user_data, TrajectoryFrameResult{ current_frame_number, water_residue_ids });
        }

        waters_per_frame.push_back(water_residue_ids.size());
        for (const auto resid : water_residue_ids) {
          ++water_presence_counts[resid];
        }
        ++successful_frames;
      } catch (const std::exception& e) {
        ++skipped_frames;
        if (callbacks.on_warning) {
          callbacks.on_warning(callbacks.user_data,
                               "Warning: skipping frame " + std::to_string(current_frame_number) + ": " + e.what());
        }
      }
    }

    if (draw_output_pdb) {
      draw_file << "END\n";
    }

    TrajectorySummary summary = summarize_trajectory(waters_per_frame, water_presence_counts);
    summary.frames_processed = frame_number;
    summary.frames_successful = successful_frames;
    summary.frames_skipped = skipped_frames;

    return summary;
  } catch (const Error&) {
    throw;
  } catch (const std::exception& e) {
    throw Error(e.what());
  }
}

void write_pymol_draw_script(const std::filesystem::path& input_path,
                             const std::filesystem::path& output_path,
                             const std::vector<std::array<double, 6>>& edges,
                             const std::vector<std::int64_t>& water_residue_ids)
{
  std::ofstream out(output_path);
  if (!out) {
    throw Error("could not open draw output file '" + output_path.string() + "'");
  }

  out << "from pymol import cmd\n";
  out << "from pymol.cgo import *\n";
  out << "from pymol import cgo\n\n";

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

void write_hull_pdb(const std::filesystem::path& output_path,
                    const HullGeometry& hull,
                    const std::vector<PdbAtomRecord>& water_atoms)
{
  std::ofstream out(output_path);
  if (!out) {
    throw Error("could not open draw output file '" + output_path.string() + "'");
  }

  out << "REMARK   Generated by watpocket --draw (.pdb)\n";
  write_hull_pdb_records(out, hull, water_atoms);
  out << "END\n";
}

} // namespace watpocket
