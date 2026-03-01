#ifndef WATPOCKET_WATPOCKET_HPP
#define WATPOCKET_WATPOCKET_HPP

#include <watpocket/watpocket_export.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace watpocket {

// Public API must not require Chemfiles/CGAL headers. Keep this header std-only.

class WATPOCKET_LIB_EXPORT Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

struct ResidueSelector {
  std::optional<std::string> chain;
  std::int64_t resid = 0;
  std::string raw;
};

struct HullGeometry {
  std::vector<std::array<double, 3>> vertices;
  std::vector<std::pair<std::size_t, std::size_t>> bonds;
  std::vector<std::array<double, 6>> edges;
};

struct PdbAtomRecord {
  std::string atom_name;
  std::string residue_name;
  std::string chain_id;
  std::int64_t residue_id = 0;
  std::array<double, 3> position{ 0.0, 0.0, 0.0 };
  std::string element;
};

struct StructureAnalysisResult {
  HullGeometry hull;
  std::vector<std::int64_t> water_residue_ids;
  std::vector<PdbAtomRecord> water_atoms_for_pdb;
};

struct TrajectoryFrameResult {
  std::size_t frame = 0; // 1-based
  std::vector<std::int64_t> water_residue_ids;
};

struct TrajectoryWaterPresence {
  std::int64_t resid = 0;
  std::size_t frames_present = 0;
  double fraction = 0.0;
};

struct TrajectorySummary {
  std::size_t frames_processed = 0;
  std::size_t frames_successful = 0;
  std::size_t frames_skipped = 0;

  std::size_t min_waters = 0;
  std::size_t max_waters = 0;
  double mean_waters = 0.0;
  double median_waters = 0.0;

  std::vector<TrajectoryWaterPresence> top_waters;
};

struct TrajectoryCallbacks {
  void* user_data = nullptr;
  void (*on_frame)(void* user_data, const TrajectoryFrameResult& result) = nullptr;
  void (*on_warning)(void* user_data, std::string_view message) = nullptr;
};

[[nodiscard]] WATPOCKET_LIB_EXPORT std::string_view build_version() noexcept;

// Parse `--resnums` style selectors: "12,15" or "A:12,B:18".
// Throws watpocket::Error on invalid input.
[[nodiscard]] WATPOCKET_LIB_EXPORT std::vector<ResidueSelector> parse_residue_selectors(std::string_view csv);

// Analyze a single-structure input file (first frame) and return computed results.
// Throws watpocket::Error on IO/validation/geometry errors.
[[nodiscard]] WATPOCKET_LIB_EXPORT StructureAnalysisResult analyze_structure_file(
  const std::filesystem::path& input_path,
  const std::vector<ResidueSelector>& selectors);

// Trajectory-mode analysis (topology + NetCDF trajectory).
//
// If `csv_output` is set, a CSV file is written with schema: frame,resnums,total_count
// If `draw_output_pdb` is set, a multi-model PDB is written using MODEL/ENDMDL.
//
// Frame-local analysis errors are reported via callbacks (if provided) and counted as skipped frames.
[[nodiscard]] WATPOCKET_LIB_EXPORT TrajectorySummary analyze_trajectory_files(
  const std::filesystem::path& topology_path,
  const std::filesystem::path& trajectory_path,
  const std::vector<ResidueSelector>& selectors,
  const std::optional<std::filesystem::path>& csv_output,
  const std::optional<std::filesystem::path>& draw_output_pdb,
  const TrajectoryCallbacks& callbacks = {});

// Draw writers (std-only API). These operate on watpocket-owned types and do not require Chemfiles.
WATPOCKET_LIB_EXPORT void write_pymol_draw_script(const std::filesystem::path& input_path,
                                                 const std::filesystem::path& output_path,
                                                 const std::vector<std::array<double, 6>>& edges,
                                                 const std::vector<std::int64_t>& water_residue_ids);

WATPOCKET_LIB_EXPORT void write_hull_pdb(const std::filesystem::path& output_path,
                                        const HullGeometry& hull,
                                        const std::vector<PdbAtomRecord>& water_atoms);

} // namespace watpocket

#endif
