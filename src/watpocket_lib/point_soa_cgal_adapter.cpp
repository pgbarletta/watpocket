#include "point_soa_cgal_adapter.hpp"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Kernel/global_functions_3.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/convex_hull_3.h>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/property_map/property_map.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <numeric>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace watpocket::detail {
namespace {

  using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
  using Point3 = Kernel::Point_3;
  using Plane3 = Kernel::Plane_3;
  using Mesh = CGAL::Surface_mesh<Point3>;

  struct SoAPointPropertyMap
  {
    using key_type = std::size_t;
    using value_type = Point3;
    using reference = value_type;
    using category = boost::readable_property_map_tag;

    const double *x = nullptr;
    const double *y = nullptr;
    const double *z = nullptr;
  };

  Point3 get(const SoAPointPropertyMap &map, const std::size_t idx)
  {
    return Point3(map.x[idx], map.y[idx], map.z[idx]);
  }

  struct IndexToPoint
  {
    explicit IndexToPoint(const SoAPointPropertyMap *map_) : map(map_) {}

    Point3 operator()(const std::size_t idx) const { return get(*map, idx); }

    const SoAPointPropertyMap *map = nullptr;
  };

  bool are_all_collinear(const PointSoAView points, const SoAPointPropertyMap &point_map)
  {
    if (points.size() < 3U) { return true; }

    const auto p0 = get(point_map, 0U);
    const auto p1 = get(point_map, 1U);
    for (std::size_t i = 2U; i < points.size(); ++i) {
      if (!CGAL::collinear(p0, p1, get(point_map, i))) { return false; }
    }

    return true;
  }

  bool are_all_coplanar(const PointSoAView points, const SoAPointPropertyMap &point_map)
  {
    if (points.size() < 4U) { return true; }

    const auto p0 = get(point_map, 0U);
    const auto p1 = get(point_map, 1U);
    const auto p2 = get(point_map, 2U);
    const auto plane = Plane3(p0, p1, p2);

    for (std::size_t i = 3U; i < points.size(); ++i) {
      if (!plane.has_on(get(point_map, i))) { return false; }
    }

    return true;
  }

  std::vector<std::array<double, 6>> edges_from_mesh(const Mesh &mesh)
  {
    std::vector<std::array<double, 6>> edges;
    edges.reserve(mesh.number_of_edges());

    std::set<std::pair<std::size_t, std::size_t>> edge_pairs;
    for (const auto edge : mesh.edges()) {
      const auto halfedge = mesh.halfedge(edge);
      const auto source = mesh.source(halfedge);
      const auto target = mesh.target(halfedge);

      const auto source_point = mesh.point(source);
      const auto target_point = mesh.point(target);

      const auto source_coords = std::array<double, 3>{
        CGAL::to_double(source_point.x()), CGAL::to_double(source_point.y()), CGAL::to_double(source_point.z())
      };
      const auto target_coords = std::array<double, 3>{
        CGAL::to_double(target_point.x()), CGAL::to_double(target_point.y()), CGAL::to_double(target_point.z())
      };

      const auto min_vertex = std::min(source_coords, target_coords);
      const auto max_vertex = std::max(source_coords, target_coords);

      // Deduplicate edges in a deterministic way based on coordinates.
      const auto source_hash =
        std::hash<double>{}(min_vertex[0]) ^ std::hash<double>{}(min_vertex[1]) ^ std::hash<double>{}(min_vertex[2]);
      const auto target_hash =
        std::hash<double>{}(max_vertex[0]) ^ std::hash<double>{}(max_vertex[1]) ^ std::hash<double>{}(max_vertex[2]);
      const auto edge_key = std::minmax(source_hash, target_hash);
      if (!edge_pairs.insert(edge_key).second) { continue; }

      edges.push_back({ min_vertex[0], min_vertex[1], min_vertex[2], max_vertex[0], max_vertex[1], max_vertex[2] });
    }

    return edges;
  }

}// namespace

HullData compute_hull_data(const PointSoAView points)
{
  if (points.size() < 4U) { throw std::runtime_error("need at least 4 points to build a 3D convex hull"); }

  const SoAPointPropertyMap point_map{ points.x_data(), points.y_data(), points.z_data() };

  if (are_all_collinear(points, point_map)) {
    throw std::runtime_error("selected points are collinear; cannot build a 3D convex hull");
  }

  if (are_all_coplanar(points, point_map)) {
    throw std::runtime_error("selected points are coplanar; cannot build a 3D convex hull");
  }

  std::vector<std::size_t> indices(points.size());
  std::iota(indices.begin(), indices.end(), 0U);

  Mesh mesh;
  const IndexToPoint index_to_point(&point_map);
  const auto begin = boost::make_transform_iterator(indices.begin(), index_to_point);
  const auto end = boost::make_transform_iterator(indices.end(), index_to_point);
  CGAL::convex_hull_3(begin, end, mesh);

  HullData hull_data;
  hull_data.geometry.edges = edges_from_mesh(mesh);

  // Collect vertices and bonds in a deterministic order from the mesh.
  std::unordered_map<Mesh::Vertex_index, std::size_t> vertex_indices;
  vertex_indices.reserve(mesh.number_of_vertices());

  hull_data.geometry.vertices.reserve(mesh.number_of_vertices());
  for (const auto v : mesh.vertices()) {
    const auto p = mesh.point(v);
    vertex_indices[v] = hull_data.geometry.vertices.size();
    hull_data.geometry.vertices.push_back({ CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z()) });
  }

  std::set<std::pair<std::size_t, std::size_t>> bonds;
  for (const auto e : mesh.edges()) {
    const auto h = mesh.halfedge(e);
    const auto s = mesh.source(h);
    const auto t = mesh.target(h);

    const auto si = vertex_indices.at(s);
    const auto ti = vertex_indices.at(t);
    const auto ordered = std::minmax(si, ti);
    bonds.insert({ ordered.first, ordered.second });
  }
  hull_data.geometry.bonds.assign(bonds.begin(), bonds.end());

  // A convex combination of the input points is inside their convex hull.
  double cx = 0.0;
  double cy = 0.0;
  double cz = 0.0;
  for (std::size_t i = 0; i < points.size(); ++i) {
    cx += points.x_data()[i];
    cy += points.y_data()[i];
    cz += points.z_data()[i];
  }
  const auto inv_n = 1.0 / static_cast<double>(points.size());
  const Point3 interior_point(cx * inv_n, cy * inv_n, cz * inv_n);

  // Build inward-oriented halfspaces from faces.
  hull_data.halfspaces.reserve(mesh.number_of_faces());
  for (const auto f : mesh.faces()) {
    std::vector<Point3> face_points;
    for (auto v : vertices_around_face(mesh.halfedge(f), mesh)) { face_points.push_back(mesh.point(v)); }

    if (face_points.size() < 3U) { continue; }

    Plane3 plane(face_points[0], face_points[1], face_points[2]);
    if (plane.oriented_side(interior_point) == CGAL::ON_POSITIVE_SIDE) { plane = plane.opposite(); }

    hull_data.halfspaces.push_back({ CGAL::to_double(plane.a()),
      CGAL::to_double(plane.b()),
      CGAL::to_double(plane.c()),
      CGAL::to_double(plane.d()) });
  }

  return hull_data;
}

bool point_inside_or_on_hull(const double x,
  const double y,
  const double z,
  const std::vector<std::array<double, 4>> &halfspaces) noexcept
{
  const Point3 point(x, y, z);
  for (const auto &plane : halfspaces) {
    const Plane3 cgal_plane(plane[0], plane[1], plane[2], plane[3]);
    if (cgal_plane.oriented_side(point) == CGAL::ON_POSITIVE_SIDE) { return false; }
  }

  return true;
}

}// namespace watpocket::detail
