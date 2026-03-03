#include <watpocket/watpocket.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>

#include <filesystem>
#include <optional>
#include <string>

namespace nb = nanobind;
using namespace nb::literals;

namespace {

std::optional<std::filesystem::path> optional_path_from_string(const std::optional<std::string> &path)
{
  if (!path) { return std::nullopt; }
  return std::filesystem::path(*path);
}

}// namespace

NB_MODULE(watpocket, m)
{
  m.doc() = "Python bindings for watpocket (nanobind)";

  static nb::exception<watpocket::Error> watpocket_error(m, "Error");
  (void)watpocket_error;

  nb::class_<watpocket::PointSoA>(m, "PointSoA")
    .def(nb::init<>())
    .def("size", &watpocket::PointSoA::size)
    .def("empty", &watpocket::PointSoA::empty)
    .def("clear", &watpocket::PointSoA::clear)
    .def("reserve", &watpocket::PointSoA::reserve, "n"_a)
    .def("resize", &watpocket::PointSoA::resize, "n"_a)
    .def("push_back", &watpocket::PointSoA::push_back, "x"_a, "y"_a, "z"_a)
    .def("point", &watpocket::PointSoA::point, "i"_a)
    .def("set_point", &watpocket::PointSoA::set_point, "i"_a, "x"_a, "y"_a, "z"_a)
    .def("__len__", &watpocket::PointSoA::size)
    .def("view",
      [](const watpocket::PointSoA &self) { return self.view(); },
      nb::keep_alive<0, 1>())
    .def("mutable_view",
      [](watpocket::PointSoA &self) { return self.mutable_view(); },
      nb::keep_alive<0, 1>());

  nb::class_<watpocket::PointSoAView>(m, "PointSoAView")
    .def("size", &watpocket::PointSoAView::size)
    .def("empty", &watpocket::PointSoAView::empty)
    .def("point", &watpocket::PointSoAView::point, "i"_a)
    .def("__len__", &watpocket::PointSoAView::size);

  nb::class_<watpocket::PointSoAMutableView>(m, "PointSoAMutableView")
    .def("size", &watpocket::PointSoAMutableView::size)
    .def("empty", &watpocket::PointSoAMutableView::empty)
    .def("point", &watpocket::PointSoAMutableView::point, "i"_a)
    .def("set_point", &watpocket::PointSoAMutableView::set_point, "i"_a, "x"_a, "y"_a, "z"_a)
    .def("__len__", &watpocket::PointSoAMutableView::size)
    .def("as_const", &watpocket::PointSoAMutableView::as_const, nb::keep_alive<0, 1>());

  nb::class_<watpocket::ResidueSelector>(m, "ResidueSelector")
    .def(nb::init<>())
    .def_rw("chain", &watpocket::ResidueSelector::chain)
    .def_rw("resid", &watpocket::ResidueSelector::resid)
    .def_rw("raw", &watpocket::ResidueSelector::raw);

  nb::class_<watpocket::HullGeometry>(m, "HullGeometry")
    .def(nb::init<>())
    .def_rw("vertices", &watpocket::HullGeometry::vertices)
    .def_rw("bonds", &watpocket::HullGeometry::bonds)
    .def_rw("edges", &watpocket::HullGeometry::edges);

  nb::class_<watpocket::PdbAtomRecord>(m, "PdbAtomRecord")
    .def(nb::init<>())
    .def_rw("atom_name", &watpocket::PdbAtomRecord::atom_name)
    .def_rw("residue_name", &watpocket::PdbAtomRecord::residue_name)
    .def_rw("chain_id", &watpocket::PdbAtomRecord::chain_id)
    .def_rw("residue_id", &watpocket::PdbAtomRecord::residue_id)
    .def_rw("position", &watpocket::PdbAtomRecord::position)
    .def_rw("element", &watpocket::PdbAtomRecord::element);

  nb::class_<watpocket::StructureAnalysisResult>(m, "StructureAnalysisResult")
    .def(nb::init<>())
    .def_rw("hull", &watpocket::StructureAnalysisResult::hull)
    .def_rw("water_residue_ids", &watpocket::StructureAnalysisResult::water_residue_ids)
    .def_rw("water_atoms_for_pdb", &watpocket::StructureAnalysisResult::water_atoms_for_pdb);

  nb::class_<watpocket::TrajectoryWaterPresence>(m, "TrajectoryWaterPresence")
    .def(nb::init<>())
    .def_rw("resid", &watpocket::TrajectoryWaterPresence::resid)
    .def_rw("frames_present", &watpocket::TrajectoryWaterPresence::frames_present)
    .def_rw("fraction", &watpocket::TrajectoryWaterPresence::fraction);

  nb::class_<watpocket::TrajectorySummary>(m, "TrajectorySummary")
    .def(nb::init<>())
    .def_rw("frames_processed", &watpocket::TrajectorySummary::frames_processed)
    .def_rw("frames_successful", &watpocket::TrajectorySummary::frames_successful)
    .def_rw("frames_skipped", &watpocket::TrajectorySummary::frames_skipped)
    .def_rw("min_waters", &watpocket::TrajectorySummary::min_waters)
    .def_rw("max_waters", &watpocket::TrajectorySummary::max_waters)
    .def_rw("mean_waters", &watpocket::TrajectorySummary::mean_waters)
    .def_rw("median_waters", &watpocket::TrajectorySummary::median_waters)
    .def_rw("has_skipped_frames", &watpocket::TrajectorySummary::has_skipped_frames)
    .def_rw("skipped_frame_warnings", &watpocket::TrajectorySummary::skipped_frame_warnings)
    .def_rw("top_waters", &watpocket::TrajectorySummary::top_waters);

  m.def("build_version", []() { return std::string(watpocket::build_version()); });
  m.def("build_git_sha", []() { return std::string(watpocket::build_git_sha()); });
  m.def("parse_residue_selectors",
    [](const std::string &csv) { return watpocket::parse_residue_selectors(csv); },
    "csv"_a);

  m.def("read_structure_points_by_atom_indices",
    [](const std::string &input_path, const std::vector<std::size_t> &atom_indices, const std::string &label) {
      return watpocket::read_structure_points_by_atom_indices(
        std::filesystem::path(input_path), atom_indices, label);
    },
    "input_path"_a,
    "atom_indices"_a,
    "label"_a = "selected atoms");

  m.def("read_trajectory_points_by_atom_indices",
    [](const std::string &topology_path,
      const std::string &trajectory_path,
      const std::size_t frame_number,
      const std::vector<std::size_t> &atom_indices,
      const std::string &label) {
      return watpocket::read_trajectory_points_by_atom_indices(
        std::filesystem::path(topology_path), std::filesystem::path(trajectory_path), frame_number, atom_indices, label);
    },
    "topology_path"_a,
    "trajectory_path"_a,
    "frame_number"_a,
    "atom_indices"_a,
    "label"_a = "selected atoms");

  m.def("analyze_structure_file",
    [](const std::string &input_path, const std::vector<watpocket::ResidueSelector> &selectors) {
      return watpocket::analyze_structure_file(std::filesystem::path(input_path), selectors);
    },
    "input_path"_a,
    "selectors"_a);

  m.def("analyze_trajectory_files",
    [](const std::string &topology_path,
      const std::string &trajectory_path,
      const std::vector<watpocket::ResidueSelector> &selectors,
      const std::optional<std::string> &csv_output,
      const std::optional<std::string> &draw_output_pdb,
      const std::size_t num_threads) {
      return watpocket::analyze_trajectory_files(std::filesystem::path(topology_path),
        std::filesystem::path(trajectory_path),
        selectors,
        optional_path_from_string(csv_output),
        optional_path_from_string(draw_output_pdb),
        num_threads);
    },
    "topology_path"_a,
    "trajectory_path"_a,
    "selectors"_a,
    "csv_output"_a = std::optional<std::string>{},
    "draw_output_pdb"_a = std::optional<std::string>{},
    "num_threads"_a = 1U);

  m.def("write_pymol_draw_script",
    [](const std::string &input_path,
      const std::string &output_path,
      const std::vector<std::array<double, 6>> &edges,
      const std::vector<std::int64_t> &water_residue_ids) {
      watpocket::write_pymol_draw_script(
        std::filesystem::path(input_path), std::filesystem::path(output_path), edges, water_residue_ids);
    },
    "input_path"_a,
    "output_path"_a,
    "edges"_a,
    "water_residue_ids"_a);

  m.def("write_hull_pdb",
    [](const std::string &output_path,
      const watpocket::HullGeometry &hull,
      const std::vector<watpocket::PdbAtomRecord> &water_atoms) {
      watpocket::write_hull_pdb(std::filesystem::path(output_path), hull, water_atoms);
    },
    "output_path"_a,
    "hull"_a,
    "water_atoms"_a);
}
