#include <emscripten/emscripten.h>

#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Surface_mesh.h>

#include <exception>
#include <sstream>
#include <string>
#include <vector>

namespace PMP = CGAL::Polygon_mesh_processing;

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using Point = Kernel::Point_3;
using Mesh = CGAL::Surface_mesh<Point>;
using Primitive = CGAL::AABB_face_graph_triangle_primitive<Mesh>;
using AabbTraits = CGAL::AABB_traits_3<Kernel, Primitive>;
using Tree = CGAL::AABB_tree<AabbTraits>;

static std::vector<double> g_projected_margin;
static std::string g_report = "CGAL engine has not run yet.";
static int g_border_halfedges = 0;
static int g_invalid_faces = 0;
static int g_self_intersections = 0;

static int count_border_halfedges(const Mesh& mesh) {
  int count = 0;
  for (const auto h : mesh.halfedges()) {
    if (mesh.is_border(h)) {
      ++count;
    }
  }
  return count;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
int cgal_process(
    const double* positions,
    int position_count,
    const unsigned int* indices,
    int index_count,
    const double* margin,
    int margin_count) {
  g_projected_margin.clear();
  g_report.clear();
  g_border_halfedges = 0;
  g_invalid_faces = 0;
  g_self_intersections = 0;

  if (!positions || !indices || position_count < 9 || index_count < 3) {
    g_report = "Input mesh is empty or incomplete.";
    return 0;
  }

  if (position_count % 3 != 0 || index_count % 3 != 0 || margin_count % 3 != 0) {
    g_report = "Input array sizes must be multiples of 3.";
    return 0;
  }

  try {
    Mesh mesh;
    const int vertex_count = position_count / 3;
    std::vector<Mesh::Vertex_index> vertices;
    vertices.reserve(vertex_count);

    for (int i = 0; i < vertex_count; ++i) {
      vertices.push_back(mesh.add_vertex(Point(
          positions[i * 3],
          positions[i * 3 + 1],
          positions[i * 3 + 2])));
    }

    for (int i = 0; i < index_count; i += 3) {
      const unsigned int ia = indices[i];
      const unsigned int ib = indices[i + 1];
      const unsigned int ic = indices[i + 2];
      if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size()) {
        ++g_invalid_faces;
        continue;
      }

      const auto face = mesh.add_face(vertices[ia], vertices[ib], vertices[ic]);
      if (face == Mesh::null_face()) {
        ++g_invalid_faces;
      }
    }

    const std::size_t faces_before_repair = mesh.number_of_faces();
    PMP::remove_degenerate_faces(mesh);
    PMP::stitch_borders(mesh);

    g_border_halfedges = count_border_halfedges(mesh);
    g_self_intersections = -1;

    if (mesh.number_of_faces() == 0) {
      g_report = "CGAL created no valid faces from the STL mesh.";
      return 0;
    }

    Tree tree(faces(mesh).first, faces(mesh).second, mesh);
    tree.accelerate_distance_queries();

    const int margin_points = margin_count / 3;
    g_projected_margin.reserve(margin_points * 3);
    for (int i = 0; i < margin_points; ++i) {
      const Point p(margin[i * 3], margin[i * 3 + 1], margin[i * 3 + 2]);
      const Point q = tree.closest_point(p);
      g_projected_margin.push_back(q.x());
      g_projected_margin.push_back(q.y());
      g_projected_margin.push_back(q.z());
    }

    std::ostringstream out;
    out << "CGAL diagnostics OK. vertices=" << mesh.number_of_vertices()
        << ", faces_before_repair=" << faces_before_repair
        << ", faces_after_repair=" << mesh.number_of_faces()
        << ", invalid_faces=" << g_invalid_faces
        << ", border_halfedges=" << g_border_halfedges
        << ", self_intersections=not_checked"
        << ", projected_margin_points=" << margin_points;
    g_report = out.str();
    return 1;
  } catch (const std::exception& e) {
    g_report = std::string("CGAL exception: ") + e.what();
    return 0;
  } catch (...) {
    g_report = "Unknown CGAL exception.";
    return 0;
  }
}

EMSCRIPTEN_KEEPALIVE
const double* cgal_get_projected_ptr() {
  return g_projected_margin.empty() ? nullptr : g_projected_margin.data();
}

EMSCRIPTEN_KEEPALIVE
int cgal_get_projected_count() {
  return static_cast<int>(g_projected_margin.size());
}

EMSCRIPTEN_KEEPALIVE
const char* cgal_get_report_ptr() {
  return g_report.c_str();
}

EMSCRIPTEN_KEEPALIVE
int cgal_get_border_halfedge_count() {
  return g_border_halfedges;
}

EMSCRIPTEN_KEEPALIVE
int cgal_get_invalid_face_count() {
  return g_invalid_faces;
}

EMSCRIPTEN_KEEPALIVE
int cgal_get_self_intersection_count() {
  return g_self_intersections;
}

}
