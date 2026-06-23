#include <emscripten/emscripten.h>

#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Surface_mesh.h>

#include <exception>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace PMP = CGAL::Polygon_mesh_processing;

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using Point = Kernel::Point_3;
using Mesh = CGAL::Surface_mesh<Point>;
using Primitive = CGAL::AABB_face_graph_triangle_primitive<Mesh>;
using AabbTraits = CGAL::AABB_traits_3<Kernel, Primitive>;
using Tree = CGAL::AABB_tree<AabbTraits>;

static std::vector<double> g_projected_margin;
static std::vector<double> g_generated_positions;
static std::string g_report = "CGAL engine has not run yet.";
static int g_border_halfedges = 0;
static int g_invalid_faces = 0;
static int g_self_intersections = 0;

struct Vec3 {
  double x;
  double y;
  double z;
};

static Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 mul(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
static double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static double norm(Vec3 a) { return std::sqrt(dot(a, a)); }
static Vec3 normalize(Vec3 a) {
  const double n = norm(a);
  return n > 1e-12 ? mul(a, 1.0 / n) : Vec3{0.0, 0.0, 1.0};
}

static std::uint64_t edge_key(std::uint32_t a, std::uint32_t b) {
  if (a > b) std::swap(a, b);
  return (static_cast<std::uint64_t>(a) << 32) | b;
}

static double distance_to_segment(Vec3 p, Vec3 a, Vec3 b) {
  const Vec3 ab = sub(b, a);
  const double len2 = dot(ab, ab);
  if (len2 <= 1e-12) return norm(sub(p, a));
  double t = dot(sub(p, a), ab) / len2;
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  return norm(sub(p, add(a, mul(ab, t))));
}

static void push_tri(Vec3 a, Vec3 b, Vec3 c) {
  g_generated_positions.push_back(a.x);
  g_generated_positions.push_back(a.y);
  g_generated_positions.push_back(a.z);
  g_generated_positions.push_back(b.x);
  g_generated_positions.push_back(b.y);
  g_generated_positions.push_back(b.z);
  g_generated_positions.push_back(c.x);
  g_generated_positions.push_back(c.y);
  g_generated_positions.push_back(c.z);
}

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
int cgal_generate_splint(
    const double* positions,
    int position_count,
    const unsigned int* indices,
    int index_count,
    const double* margin,
    int margin_count,
    double offset,
    double thickness,
    double barrier_radius) {
  g_generated_positions.clear();
  g_projected_margin.clear();
  g_report.clear();
  g_border_halfedges = 0;
  g_invalid_faces = 0;
  g_self_intersections = -1;

  if (!positions || !indices || !margin || position_count < 9 || index_count < 3 || margin_count < 9) {
    g_report = "CGAL splint input is empty or incomplete.";
    return 0;
  }
  if (position_count % 3 != 0 || index_count % 3 != 0 || margin_count % 3 != 0) {
    g_report = "Input array sizes must be multiples of 3.";
    return 0;
  }

  try {
    const int vertex_count = position_count / 3;
    std::vector<Vec3> vertices;
    vertices.reserve(vertex_count);
    Vec3 minp{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    Vec3 maxp{-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};

    Mesh mesh;
    std::vector<Mesh::Vertex_index> mesh_vertices;
    mesh_vertices.reserve(vertex_count);
    for (int i = 0; i < vertex_count; ++i) {
      const Vec3 p{positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]};
      vertices.push_back(p);
      mesh_vertices.push_back(mesh.add_vertex(Point(p.x, p.y, p.z)));
      minp.x = std::min(minp.x, p.x); minp.y = std::min(minp.y, p.y); minp.z = std::min(minp.z, p.z);
      maxp.x = std::max(maxp.x, p.x); maxp.y = std::max(maxp.y, p.y); maxp.z = std::max(maxp.z, p.z);
    }

    std::vector<std::array<std::uint32_t, 3>> faces_in;
    faces_in.reserve(index_count / 3);
    for (int i = 0; i < index_count; i += 3) {
      const std::uint32_t a = indices[i];
      const std::uint32_t b = indices[i + 1];
      const std::uint32_t c = indices[i + 2];
      if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size() || a == b || b == c || a == c) {
        ++g_invalid_faces;
        continue;
      }
      faces_in.push_back({a, b, c});
      const auto face = mesh.add_face(mesh_vertices[a], mesh_vertices[b], mesh_vertices[c]);
      if (face == Mesh::null_face()) ++g_invalid_faces;
    }

    if (faces_in.empty()) {
      g_report = "No valid faces for CGAL splint generation.";
      return 0;
    }

    PMP::remove_degenerate_faces(mesh);
    PMP::stitch_borders(mesh);
    g_border_halfedges = count_border_halfedges(mesh);

    Tree tree(faces(mesh).first, faces(mesh).second, mesh);
    tree.accelerate_distance_queries();

    std::vector<Vec3> projected_margin;
    const int margin_points = margin_count / 3;
    projected_margin.reserve(margin_points);
    g_projected_margin.reserve(margin_count);
    for (int i = 0; i < margin_points; ++i) {
      const Point q = tree.closest_point(Point(margin[i * 3], margin[i * 3 + 1], margin[i * 3 + 2]));
      const Vec3 p{q.x(), q.y(), q.z()};
      projected_margin.push_back(p);
      g_projected_margin.push_back(p.x);
      g_projected_margin.push_back(p.y);
      g_projected_margin.push_back(p.z);
    }

    const double diag = norm(sub(maxp, minp));
    if (barrier_radius <= 0.0) {
      barrier_radius = std::max(0.35, diag * 0.006);
    }

    std::unordered_map<std::uint64_t, std::vector<int>> edge_faces;
    edge_faces.reserve(faces_in.size() * 3);
    for (int fi = 0; fi < static_cast<int>(faces_in.size()); ++fi) {
      const auto f = faces_in[fi];
      edge_faces[edge_key(f[0], f[1])].push_back(fi);
      edge_faces[edge_key(f[1], f[2])].push_back(fi);
      edge_faces[edge_key(f[2], f[0])].push_back(fi);
    }

    std::vector<std::vector<int>> adjacency(faces_in.size());
    std::vector<unsigned char> border_face(faces_in.size(), 0);
    for (const auto& item : edge_faces) {
      const auto& fs = item.second;
      if (fs.size() == 1) {
        border_face[fs[0]] = 1;
      } else {
        for (int a : fs) {
          for (int b : fs) {
            if (a != b) adjacency[a].push_back(b);
          }
        }
      }
    }

    std::vector<unsigned char> barrier(faces_in.size(), 0);
    int barrier_faces = 0;
    for (int fi = 0; fi < static_cast<int>(faces_in.size()); ++fi) {
      const auto f = faces_in[fi];
      const Vec3 centroid = mul(add(add(vertices[f[0]], vertices[f[1]]), vertices[f[2]]), 1.0 / 3.0);
      double best = std::numeric_limits<double>::infinity();
      for (int i = 0; i < margin_points; ++i) {
        const Vec3 a = projected_margin[i];
        const Vec3 b = projected_margin[(i + 1) % margin_points];
        best = std::min(best, distance_to_segment(centroid, a, b));
      }
      if (best <= barrier_radius) {
        barrier[fi] = 1;
        ++barrier_faces;
      }
    }

    std::vector<unsigned char> outside(faces_in.size(), 0);
    std::queue<int> q;
    for (int fi = 0; fi < static_cast<int>(faces_in.size()); ++fi) {
      if (border_face[fi] && !barrier[fi]) {
        outside[fi] = 1;
        q.push(fi);
      }
    }

    if (q.empty()) {
      g_report = "CGAL splint flood fill found no mesh border outside the margin.";
      return 0;
    }

    while (!q.empty()) {
      const int fi = q.front();
      q.pop();
      for (int ni : adjacency[fi]) {
        if (!outside[ni] && !barrier[ni]) {
          outside[ni] = 1;
          q.push(ni);
        }
      }
    }

    std::vector<unsigned char> selected(faces_in.size(), 0);
    int selected_faces = 0;
    for (int fi = 0; fi < static_cast<int>(faces_in.size()); ++fi) {
      if (!outside[fi]) {
        selected[fi] = 1;
        ++selected_faces;
      }
    }

    if (selected_faces <= barrier_faces || selected_faces < 10) {
      g_report = "CGAL splint selected too few faces. Margin barrier may be open or too thin.";
      return 0;
    }

    std::vector<Vec3> normals(vertices.size(), Vec3{0.0, 0.0, 0.0});
    for (int fi = 0; fi < static_cast<int>(faces_in.size()); ++fi) {
      if (!selected[fi]) continue;
      const auto f = faces_in[fi];
      const Vec3 n = normalize(cross(sub(vertices[f[1]], vertices[f[0]]), sub(vertices[f[2]], vertices[f[0]])));
      normals[f[0]] = add(normals[f[0]], n);
      normals[f[1]] = add(normals[f[1]], n);
      normals[f[2]] = add(normals[f[2]], n);
    }
    for (auto& n : normals) n = normalize(n);

    auto offset_vertex = [&](std::uint32_t vi, double d) {
      return add(vertices[vi], mul(normals[vi], d));
    };

    const double inner_offset = offset;
    const double outer_offset = offset + thickness;

    for (int fi = 0; fi < static_cast<int>(faces_in.size()); ++fi) {
      if (!selected[fi]) continue;
      const auto f = faces_in[fi];
      push_tri(offset_vertex(f[0], inner_offset), offset_vertex(f[2], inner_offset), offset_vertex(f[1], inner_offset));
      push_tri(offset_vertex(f[0], outer_offset), offset_vertex(f[1], outer_offset), offset_vertex(f[2], outer_offset));
    }

    int wall_edges = 0;
    for (const auto& item : edge_faces) {
      const auto& fs = item.second;
      int selected_count = 0;
      for (int fi : fs) if (selected[fi]) ++selected_count;
      if (selected_count == 0 || selected_count == static_cast<int>(fs.size())) continue;

      std::uint32_t a = static_cast<std::uint32_t>(item.first >> 32);
      std::uint32_t b = static_cast<std::uint32_t>(item.first & 0xffffffffu);
      const Vec3 ai = offset_vertex(a, inner_offset);
      const Vec3 bi = offset_vertex(b, inner_offset);
      const Vec3 ao = offset_vertex(a, outer_offset);
      const Vec3 bo = offset_vertex(b, outer_offset);
      push_tri(ai, ao, bo);
      push_tri(ai, bo, bi);
      ++wall_edges;
    }

    if (g_generated_positions.empty()) {
      g_report = "CGAL splint generated no triangles.";
      return 0;
    }

    std::ostringstream out;
    out << "CGAL splint generated. input_faces=" << faces_in.size()
        << ", selected_faces=" << selected_faces
        << ", barrier_faces=" << barrier_faces
        << ", wall_edges=" << wall_edges
        << ", output_triangles=" << (g_generated_positions.size() / 9);
    g_report = out.str();
    return 1;
  } catch (const std::exception& e) {
    g_report = std::string("CGAL splint exception: ") + e.what();
    return 0;
  } catch (...) {
    g_report = "Unknown CGAL splint exception.";
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
const double* cgal_get_generated_positions_ptr() {
  return g_generated_positions.empty() ? nullptr : g_generated_positions.data();
}

EMSCRIPTEN_KEEPALIVE
int cgal_get_generated_positions_count() {
  return static_cast<int>(g_generated_positions.size());
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
