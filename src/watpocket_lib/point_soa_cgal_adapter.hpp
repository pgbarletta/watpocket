#ifndef WATPOCKET_POINT_SOA_CGAL_ADAPTER_HPP
#define WATPOCKET_POINT_SOA_CGAL_ADAPTER_HPP

#include <array>
#include <vector>

#include <watpocket/watpocket.hpp>

namespace watpocket::detail {

struct HullData
{
  HullGeometry geometry;
  std::vector<std::array<double, 4>> halfspaces;
};

[[nodiscard]] HullData compute_hull_data(PointSoAView points);

[[nodiscard]] bool
  point_inside_or_on_hull(double x, double y, double z, const std::vector<std::array<double, 4>> &halfspaces) noexcept;

}// namespace watpocket::detail

#endif
