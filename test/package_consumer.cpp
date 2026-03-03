#include <watpocket/watpocket.hpp>

int main()
{
  // Compile/link check: public header must not require chemfiles/cgal.
  (void)watpocket::build_version();
  using StructureReader = watpocket::PointSoA (*)(const std::filesystem::path&,
                                                  const std::vector<std::size_t>&,
                                                  std::string_view);
  using TrajectoryReader = watpocket::PointSoA (*)(const std::filesystem::path&,
                                                   const std::filesystem::path&,
                                                   std::size_t,
                                                   const std::vector<std::size_t>&,
                                                   std::string_view);
  StructureReader structure_reader = &watpocket::read_structure_points_by_atom_indices;
  TrajectoryReader trajectory_reader = &watpocket::read_trajectory_points_by_atom_indices;
  (void)structure_reader;
  (void)trajectory_reader;
  return 0;
}
