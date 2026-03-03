#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chemfiles.hpp>

#include <watpocket/watpocket.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using Catch::Matchers::ContainsSubstring;

fs::path fixture_path(const std::string& relative_path)
{
#ifdef WATPOCKET_SOURCE_ROOT
  return fs::path(WATPOCKET_SOURCE_ROOT) / relative_path;
#else
  return fs::current_path() / relative_path;
#endif
}

fs::path unique_temp_nc_path(const std::string& stem)
{
  const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  return fs::temp_directory_path() / ("watpocket-" + stem + "-" + std::to_string(ticks) + ".nc");
}

struct TempFileGuard {
  fs::path path;
  ~TempFileGuard() { std::error_code ec; fs::remove(path, ec); }
};

void write_netcdf_from_pdb_frames(const fs::path& output_nc_path, const std::vector<fs::path>& frame_paths)
{
  chemfiles::Trajectory writer(output_nc_path.string(), 'w');
  for (const auto& frame_path : frame_paths) {
    chemfiles::Trajectory reader(frame_path.string(), 'r');
    writer.write(reader.read());
  }
}

void require_coordinates_match(const watpocket::PointSoA& points,
                               const chemfiles::Frame& frame,
                               const std::vector<std::size_t>& atom_indices)
{
  REQUIRE(points.size() == atom_indices.size());
  for (std::size_t i = 0; i < atom_indices.size(); ++i) {
    const auto atom_index = atom_indices[i];
    const auto point = points.point(i);
    const auto& expected = frame.positions().at(atom_index);
    REQUIRE(point[0] == Catch::Approx(expected[0]));
    REQUIRE(point[1] == Catch::Approx(expected[1]));
    REQUIRE(point[2] == Catch::Approx(expected[2]));
  }
}

} // namespace

TEST_CASE("PointSoA preserves invariant across clear resize and push_back", "[api]")
{
  watpocket::PointSoA points;
  REQUIRE(points.empty());

  points.push_back(1.0, 2.0, 3.0);
  points.push_back(4.0, 5.0, 6.0);
  REQUIRE(points.size() == 2);
  REQUIRE(points.point(0) == std::array<double, 3>{ 1.0, 2.0, 3.0 });
  REQUIRE(points.point(1) == std::array<double, 3>{ 4.0, 5.0, 6.0 });

  points.resize(4);
  REQUIRE(points.size() == 4);
  REQUIRE(points.point(2) == std::array<double, 3>{ 0.0, 0.0, 0.0 });
  REQUIRE(points.point(3) == std::array<double, 3>{ 0.0, 0.0, 0.0 });

  points.clear();
  REQUIRE(points.empty());
}

TEST_CASE("PointSoA bounds checks throw watpocket::Error", "[api]")
{
  watpocket::PointSoA points;
  points.push_back(1.0, 2.0, 3.0);

  REQUIRE_THROWS_AS(points.point(1), watpocket::Error);
  REQUIRE_THROWS_AS(points.set_point(5, 9.0, 9.0, 9.0), watpocket::Error);
}

TEST_CASE("PointSoAView validates construction and bounds", "[api]")
{
  REQUIRE_THROWS_AS(watpocket::PointSoAView(nullptr, nullptr, nullptr, 1), watpocket::Error);

  double x[] = { 1.0, 4.0 };
  double y[] = { 2.0, 5.0 };
  double z[] = { 3.0, 6.0 };
  watpocket::PointSoAView view(x, y, z, 2);
  REQUIRE(view.size() == 2);
  REQUIRE(view.point(0) == std::array<double, 3>{ 1.0, 2.0, 3.0 });
  REQUIRE(view.point(1) == std::array<double, 3>{ 4.0, 5.0, 6.0 });
  REQUIRE_THROWS_AS(view.point(2), watpocket::Error);
}

TEST_CASE("PointSoAMutableView mutates backing storage", "[api]")
{
  REQUIRE_THROWS_AS(watpocket::PointSoAMutableView(nullptr, nullptr, nullptr, 1), watpocket::Error);

  watpocket::PointSoA points;
  points.push_back(1.0, 2.0, 3.0);
  points.push_back(4.0, 5.0, 6.0);

  auto mutable_view = points.mutable_view();
  mutable_view.set_point(1, 7.0, 8.0, 9.0);
  REQUIRE(points.point(1) == std::array<double, 3>{ 7.0, 8.0, 9.0 });

  const auto const_view = mutable_view.as_const();
  REQUIRE(const_view.point(1) == std::array<double, 3>{ 7.0, 8.0, 9.0 });
  REQUIRE_THROWS_AS(mutable_view.set_point(2, 0.0, 0.0, 0.0), watpocket::Error);
}

TEST_CASE("parse_residue_selectors parses residue-only selectors", "[api]")
{
  const auto selectors = watpocket::parse_residue_selectors("12,15,18");
  REQUIRE(selectors.size() == 3);
  REQUIRE_FALSE(selectors[0].chain.has_value());
  REQUIRE(selectors[0].resid == 12);
  REQUIRE(selectors[2].resid == 18);
}

TEST_CASE("parse_residue_selectors parses chain-qualified selectors", "[api]")
{
  const auto selectors = watpocket::parse_residue_selectors("A:12,B:18");
  REQUIRE(selectors.size() == 2);
  REQUIRE(selectors[0].chain.value() == "A");
  REQUIRE(selectors[0].resid == 12);
  REQUIRE(selectors[1].chain.value() == "B");
  REQUIRE(selectors[1].resid == 18);
}

TEST_CASE("parse_residue_selectors rejects empty selector", "[api]")
{
  REQUIRE_THROWS_AS(watpocket::parse_residue_selectors(","), watpocket::Error);
}

TEST_CASE("read_structure_points_by_atom_indices returns selected coordinates in caller order", "[api]")
{
  const auto structure_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  REQUIRE(fs::exists(structure_path));

  const std::vector<std::size_t> atom_indices{ 0, 12, 12, 37 };
  const auto points =
    watpocket::read_structure_points_by_atom_indices(structure_path, atom_indices, "selected structure atoms");

  chemfiles::Trajectory structure_trajectory(structure_path.string(), 'r');
  const auto frame = structure_trajectory.read();
  require_coordinates_match(points, frame, atom_indices);
  REQUIRE(points.point(1) == points.point(2));
}

TEST_CASE("read_structure_points_by_atom_indices accepts empty atom index list", "[api]")
{
  const auto structure_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  REQUIRE(fs::exists(structure_path));

  const auto points = watpocket::read_structure_points_by_atom_indices(structure_path, {});
  REQUIRE(points.empty());
}

TEST_CASE("read_structure_points_by_atom_indices reports invalid atom index with label", "[api]")
{
  const auto structure_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  REQUIRE(fs::exists(structure_path));

  try {
    (void)watpocket::read_structure_points_by_atom_indices(structure_path, { 999999 }, "structure label");
    FAIL("Expected watpocket::Error");
  } catch (const watpocket::Error& e) {
    REQUIRE_THAT(e.what(), ContainsSubstring("structure label"));
    REQUIRE_THAT(e.what(), ContainsSubstring("999999"));
  }
}

TEST_CASE("read_structure_points_by_atom_indices rejects parm7 topology-only input", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/complex_wcn.parm7");
  REQUIRE(fs::exists(topology_path));

  REQUIRE_THROWS_AS(watpocket::read_structure_points_by_atom_indices(topology_path, { 0 }), watpocket::Error);
}

TEST_CASE("read_trajectory_points_by_atom_indices reads requested 1-based frame from NetCDF", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  const auto frame0_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  const auto frame1_path = fixture_path("tests/data/wcn/1complex_wcn.pdb");
  REQUIRE(fs::exists(topology_path));
  REQUIRE(fs::exists(frame0_path));
  REQUIRE(fs::exists(frame1_path));

  const auto trajectory_path = unique_temp_nc_path("api-read-traj");
  TempFileGuard cleanup{ trajectory_path };
  write_netcdf_from_pdb_frames(trajectory_path, { frame0_path, frame1_path });

  const std::vector<std::size_t> atom_indices{ 0, 11, 11, 25 };
  const auto points = watpocket::read_trajectory_points_by_atom_indices(
    topology_path, trajectory_path, 2, atom_indices, "selected trajectory atoms");

  chemfiles::Trajectory expected_traj(frame1_path.string(), 'r');
  const auto expected_frame = expected_traj.read();
  require_coordinates_match(points, expected_frame, atom_indices);
  REQUIRE(points.point(1) == points.point(2));
}

TEST_CASE("read_trajectory_points_by_atom_indices rejects frame number zero", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  const auto frame0_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  REQUIRE(fs::exists(topology_path));
  REQUIRE(fs::exists(frame0_path));

  const auto trajectory_path = unique_temp_nc_path("api-frame-zero");
  TempFileGuard cleanup{ trajectory_path };
  write_netcdf_from_pdb_frames(trajectory_path, { frame0_path });

  REQUIRE_THROWS_AS(watpocket::read_trajectory_points_by_atom_indices(topology_path, trajectory_path, 0, { 0 }),
                    watpocket::Error);
}

TEST_CASE("read_trajectory_points_by_atom_indices validates frame range with requested and available counts", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  const auto frame0_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  REQUIRE(fs::exists(topology_path));
  REQUIRE(fs::exists(frame0_path));

  const auto trajectory_path = unique_temp_nc_path("api-frame-range");
  TempFileGuard cleanup{ trajectory_path };
  write_netcdf_from_pdb_frames(trajectory_path, { frame0_path });

  try {
    (void)watpocket::read_trajectory_points_by_atom_indices(topology_path, trajectory_path, 2, { 0 });
    FAIL("Expected watpocket::Error");
  } catch (const watpocket::Error& e) {
    REQUIRE_THAT(e.what(), ContainsSubstring("requested frame 2"));
    REQUIRE_THAT(e.what(), ContainsSubstring("1 frames"));
  }
}

TEST_CASE("read_trajectory_points_by_atom_indices reports invalid atom index with label", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  const auto frame0_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  REQUIRE(fs::exists(topology_path));
  REQUIRE(fs::exists(frame0_path));

  const auto trajectory_path = unique_temp_nc_path("api-invalid-index");
  TempFileGuard cleanup{ trajectory_path };
  write_netcdf_from_pdb_frames(trajectory_path, { frame0_path });

  try {
    (void)watpocket::read_trajectory_points_by_atom_indices(
      topology_path, trajectory_path, 1, { 999999 }, "trajectory label");
    FAIL("Expected watpocket::Error");
  } catch (const watpocket::Error& e) {
    REQUIRE_THAT(e.what(), ContainsSubstring("trajectory label"));
    REQUIRE_THAT(e.what(), ContainsSubstring("999999"));
  }
}

TEST_CASE("read_trajectory_points_by_atom_indices accepts empty atom index list", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  const auto frame0_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  REQUIRE(fs::exists(topology_path));
  REQUIRE(fs::exists(frame0_path));

  const auto trajectory_path = unique_temp_nc_path("api-empty");
  TempFileGuard cleanup{ trajectory_path };
  write_netcdf_from_pdb_frames(trajectory_path, { frame0_path });

  const auto points = watpocket::read_trajectory_points_by_atom_indices(topology_path, trajectory_path, 1, {});
  REQUIRE(points.empty());
}

TEST_CASE("read_trajectory_points_by_atom_indices is deterministic across repeated calls", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  const auto frame0_path = fixture_path("tests/data/wcn/0complex_wcn.pdb");
  const auto frame1_path = fixture_path("tests/data/wcn/1complex_wcn.pdb");
  REQUIRE(fs::exists(topology_path));
  REQUIRE(fs::exists(frame0_path));
  REQUIRE(fs::exists(frame1_path));

  const auto trajectory_path = unique_temp_nc_path("api-deterministic");
  TempFileGuard cleanup{ trajectory_path };
  write_netcdf_from_pdb_frames(trajectory_path, { frame0_path, frame1_path });

  const std::vector<std::size_t> atom_indices{ 4, 10, 27 };
  const auto first = watpocket::read_trajectory_points_by_atom_indices(topology_path, trajectory_path, 2, atom_indices);
  const auto second = watpocket::read_trajectory_points_by_atom_indices(topology_path, trajectory_path, 2, atom_indices);

  REQUIRE(first.size() == second.size());
  for (std::size_t i = 0; i < first.size(); ++i) {
    REQUIRE(first.point(i) == second.point(i));
  }
}

TEST_CASE("read_trajectory_points_by_atom_indices rejects non-NetCDF trajectory extension", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/complex_wcn.parm7");
  const auto non_netcdf_path = fixture_path("tests/data/wcn/traj.pdb");
  REQUIRE(fs::exists(topology_path));
  REQUIRE(fs::exists(non_netcdf_path));

  REQUIRE_THROWS_AS(watpocket::read_trajectory_points_by_atom_indices(topology_path, non_netcdf_path, 1, { 0 }),
                    watpocket::Error);
}

TEST_CASE("read_trajectory_points_by_atom_indices reports topology-trajectory atom-count mismatch", "[api]")
{
  const auto topology_path = fixture_path("tests/data/wcn/complex_wcn.parm7");
  const auto small_structure_path = fixture_path("tests/data/wcn/sal.pdb");
  REQUIRE(fs::exists(topology_path));
  REQUIRE(fs::exists(small_structure_path));

  const auto mismatch_trajectory_path = unique_temp_nc_path("api-mismatch");
  TempFileGuard cleanup{ mismatch_trajectory_path };
  write_netcdf_from_pdb_frames(mismatch_trajectory_path, { small_structure_path });

  try {
    (void)watpocket::read_trajectory_points_by_atom_indices(topology_path, mismatch_trajectory_path, 1, { 0 });
    FAIL("Expected watpocket::Error");
  } catch (const watpocket::Error& e) {
    REQUIRE_THAT(e.what(), ContainsSubstring("atom-count mismatch between topology and trajectory"));
  }
}
