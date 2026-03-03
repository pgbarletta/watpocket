#ifndef WATPOCKET_WATPOCKET_HPP
#define WATPOCKET_WATPOCKET_HPP

#include <watpocket/watpocket_export.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace watpocket {

// Public API must not require Chemfiles/CGAL headers. Keep this header std-only.

class WATPOCKET_LIB_EXPORT Error : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

class PointSoAView;
class PointSoAMutableView;

// Owning structure-of-arrays point container.
// Invariant: all coordinate buffers have the same length.
class PointSoA
{
public:
  [[nodiscard]] std::size_t size() const noexcept { return x_.size(); }
  [[nodiscard]] bool empty() const noexcept { return x_.empty(); }

  void clear() noexcept
  {
    x_.clear();
    y_.clear();
    z_.clear();
  }

  void reserve(const std::size_t n)
  {
    x_.reserve(n);
    y_.reserve(n);
    z_.reserve(n);
  }

  void resize(const std::size_t n)
  {
    const auto current = size();
    if (n <= current) {
      x_.resize(n);
      y_.resize(n);
      z_.resize(n);
      return;
    }

    reserve(n);
    while (size() < n) { push_back(0.0, 0.0, 0.0); }
  }

  void push_back(const double xv, const double yv, const double zv)
  {
    x_.push_back(xv);
    try {
      y_.push_back(yv);
    } catch (...) {
      x_.pop_back();
      throw;
    }

    try {
      z_.push_back(zv);
    } catch (...) {
      y_.pop_back();
      x_.pop_back();
      throw;
    }
  }

  [[nodiscard]] std::array<double, 3> point(const std::size_t i) const
  {
    validate_index(i);
    return { x_[i], y_[i], z_[i] };
  }

  void set_point(const std::size_t i, const double xv, const double yv, const double zv)
  {
    validate_index(i);
    x_[i] = xv;
    y_[i] = yv;
    z_[i] = zv;
  }

  [[nodiscard]] const double *x_data() const noexcept { return x_.data(); }
  [[nodiscard]] const double *y_data() const noexcept { return y_.data(); }
  [[nodiscard]] const double *z_data() const noexcept { return z_.data(); }
  [[nodiscard]] double *x_data() noexcept { return x_.data(); }
  [[nodiscard]] double *y_data() noexcept { return y_.data(); }
  [[nodiscard]] double *z_data() noexcept { return z_.data(); }

  // Returned views are invalidated by operations that reallocate coordinate storage.
  [[nodiscard]] PointSoAView view() const noexcept;
  [[nodiscard]] PointSoAMutableView mutable_view() noexcept;

private:
  void validate_index(const std::size_t i) const
  {
    if (i >= size()) {
      throw Error(
        "point index " + std::to_string(i) + " is out of bounds for PointSoA of size " + std::to_string(size()));
    }
  }

  std::vector<double> x_;
  std::vector<double> y_;
  std::vector<double> z_;
};

class PointSoAView
{
public:
  PointSoAView() = default;

  // Non-owning read-only view over three contiguous coordinate buffers.
  PointSoAView(const double *x, const double *y, const double *z, const std::size_t length)
    : x_(x), y_(y), z_(z), length_(length)
  {
    validate_buffers();
  }

  [[nodiscard]] std::size_t size() const noexcept { return length_; }
  [[nodiscard]] bool empty() const noexcept { return length_ == 0U; }

  [[nodiscard]] std::array<double, 3> point(const std::size_t i) const
  {
    validate_index(i);
    return { x_[i], y_[i], z_[i] };
  }

  [[nodiscard]] const double *x_data() const noexcept { return x_; }
  [[nodiscard]] const double *y_data() const noexcept { return y_; }
  [[nodiscard]] const double *z_data() const noexcept { return z_; }

private:
  void validate_buffers() const
  {
    if (length_ == 0U) { return; }
    if (x_ == nullptr || y_ == nullptr || z_ == nullptr) {
      throw Error("PointSoAView requires non-null x/y/z buffers when length > 0");
    }
  }

  void validate_index(const std::size_t i) const
  {
    if (i >= length_) {
      throw Error(
        "point index " + std::to_string(i) + " is out of bounds for PointSoAView of size " + std::to_string(length_));
    }
  }

  const double *x_ = nullptr;
  const double *y_ = nullptr;
  const double *z_ = nullptr;
  std::size_t length_ = 0;
};

class PointSoAMutableView
{
public:
  PointSoAMutableView() = default;

  // Non-owning mutable view over three contiguous coordinate buffers.
  PointSoAMutableView(double *x, double *y, double *z, const std::size_t length) : x_(x), y_(y), z_(z), length_(length)
  {
    validate_buffers();
  }

  [[nodiscard]] std::size_t size() const noexcept { return length_; }
  [[nodiscard]] bool empty() const noexcept { return length_ == 0U; }

  [[nodiscard]] std::array<double, 3> point(const std::size_t i) const
  {
    validate_index(i);
    return { x_[i], y_[i], z_[i] };
  }

  void set_point(const std::size_t i, const double xv, const double yv, const double zv)
  {
    validate_index(i);
    x_[i] = xv;
    y_[i] = yv;
    z_[i] = zv;
  }

  [[nodiscard]] const double *x_data() const noexcept { return x_; }
  [[nodiscard]] const double *y_data() const noexcept { return y_; }
  [[nodiscard]] const double *z_data() const noexcept { return z_; }
  [[nodiscard]] double *x_data() noexcept { return x_; }
  [[nodiscard]] double *y_data() noexcept { return y_; }
  [[nodiscard]] double *z_data() noexcept { return z_; }

  [[nodiscard]] PointSoAView as_const() const { return PointSoAView(x_, y_, z_, length_); }

private:
  void validate_buffers() const
  {
    if (length_ == 0U) { return; }
    if (x_ == nullptr || y_ == nullptr || z_ == nullptr) {
      throw Error("PointSoAMutableView requires non-null x/y/z buffers when length > 0");
    }
  }

  void validate_index(const std::size_t i) const
  {
    if (i >= length_) {
      throw Error("point index " + std::to_string(i) + " is out of bounds for PointSoAMutableView of size "
                  + std::to_string(length_));
    }
  }

  double *x_ = nullptr;
  double *y_ = nullptr;
  double *z_ = nullptr;
  std::size_t length_ = 0;
};

inline PointSoAView PointSoA::view() const noexcept { return PointSoAView(x_.data(), y_.data(), z_.data(), x_.size()); }

inline PointSoAMutableView PointSoA::mutable_view() noexcept
{
  return PointSoAMutableView(x_.data(), y_.data(), z_.data(), x_.size());
}

struct ResidueSelector
{
  std::optional<std::string> chain;
  std::int64_t resid = 0;
  std::string raw;
};

struct HullGeometry
{
  std::vector<std::array<double, 3>> vertices;
  std::vector<std::pair<std::size_t, std::size_t>> bonds;
  std::vector<std::array<double, 6>> edges;
};

struct PdbAtomRecord
{
  std::string atom_name;
  std::string residue_name;
  std::string chain_id;
  std::int64_t residue_id = 0;
  std::array<double, 3> position{ 0.0, 0.0, 0.0 };
  std::string element;
};

struct StructureAnalysisResult
{
  HullGeometry hull;
  std::vector<std::int64_t> water_residue_ids;
  std::vector<PdbAtomRecord> water_atoms_for_pdb;
};

struct TrajectoryWaterPresence
{
  std::int64_t resid = 0;
  std::size_t frames_present = 0;
  double fraction = 0.0;
};

struct TrajectorySummary
{
  std::size_t frames_processed = 0;
  std::size_t frames_successful = 0;
  std::size_t frames_skipped = 0;

  std::size_t min_waters = 0;
  std::size_t max_waters = 0;
  double mean_waters = 0.0;
  double median_waters = 0.0;
  bool has_skipped_frames = false;

  std::vector<TrajectoryWaterPresence> top_waters;
  std::unordered_map<std::size_t, std::string> skipped_frame_warnings;
};

[[nodiscard]] WATPOCKET_LIB_EXPORT std::string_view build_version() noexcept;

// Parse `--resnums` style selectors: "12,15" or "A:12,B:18".
// Throws watpocket::Error on invalid input.
[[nodiscard]] WATPOCKET_LIB_EXPORT std::vector<ResidueSelector> parse_residue_selectors(std::string_view csv);

// Read the first frame from a structure input and extract points at caller-provided atom indices.
//
// Contract:
// - `atom_indices` are 0-based and interpreted against the loaded frame atom order.
// - Output preserves `atom_indices` order and duplicates.
// - Empty `atom_indices` is valid and returns an empty PointSoA.
// - `label` is used in validation error messages (default: "selected atoms").
//
// Throws watpocket::Error on IO/format errors, unsupported topology-only inputs (.parm7/.prmtop),
// or invalid atom indices.
[[nodiscard]] WATPOCKET_LIB_EXPORT PointSoA read_structure_points_by_atom_indices(
  const std::filesystem::path &input_path,
  const std::vector<std::size_t> &atom_indices,
  std::string_view label = "selected atoms");

// Read one frame from topology+trajectory inputs and extract points at caller-provided atom indices.
//
// Contract:
// - `frame_number` is 1-based (`0` is invalid).
// - `atom_indices` are 0-based and interpreted against the selected frame atom order.
// - Output preserves `atom_indices` order and duplicates.
// - Empty `atom_indices` is valid and returns an empty PointSoA.
// - `label` is used in validation error messages (default: "selected atoms").
//
// Constraints:
// - `trajectory_path` must be NetCDF (.nc).
// - `topology_path` accepts structure files or Amber .parm7/.prmtop, matching trajectory-mode rules.
//
// Throws watpocket::Error on IO/format/validation errors, including out-of-range frame numbers
// and topology/trajectory atom-count mismatches.
[[nodiscard]] WATPOCKET_LIB_EXPORT PointSoA read_trajectory_points_by_atom_indices(
  const std::filesystem::path &topology_path,
  const std::filesystem::path &trajectory_path,
  std::size_t frame_number,
  const std::vector<std::size_t> &atom_indices,
  std::string_view label = "selected atoms");

// Analyze a single-structure input file (first frame) and return computed results.
// Throws watpocket::Error on IO/validation/geometry errors.
[[nodiscard]] WATPOCKET_LIB_EXPORT StructureAnalysisResult analyze_structure_file(
  const std::filesystem::path &input_path,
  const std::vector<ResidueSelector> &selectors);

// Trajectory-mode analysis (topology + NetCDF trajectory).
//
// If `csv_output` is set, a CSV file is written with schema: frame,resnums,total_count
// If `draw_output_pdb` is set, a multi-model PDB is written using MODEL/ENDMDL.
//
// Frame-local analysis errors are recorded in `TrajectorySummary::skipped_frame_warnings`
// and counted as skipped frames.
//
// Throws watpocket::Error if all processed frames fail.
[[nodiscard]] WATPOCKET_LIB_EXPORT TrajectorySummary analyze_trajectory_files(
  const std::filesystem::path &topology_path,
  const std::filesystem::path &trajectory_path,
  const std::vector<ResidueSelector> &selectors,
  const std::optional<std::filesystem::path> &csv_output,
  const std::optional<std::filesystem::path> &draw_output_pdb);

// Draw writers (std-only API). These operate on watpocket-owned types and do not require Chemfiles.
WATPOCKET_LIB_EXPORT void write_pymol_draw_script(const std::filesystem::path &input_path,
  const std::filesystem::path &output_path,
  const std::vector<std::array<double, 6>> &edges,
  const std::vector<std::int64_t> &water_residue_ids);

WATPOCKET_LIB_EXPORT void write_hull_pdb(const std::filesystem::path &output_path,
  const HullGeometry &hull,
  const std::vector<PdbAtomRecord> &water_atoms);

}// namespace watpocket

#endif
