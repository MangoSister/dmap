#pragma once
#include "base_mesh.h"
#include "displaced_surface.h"
#include "ks/geometry.h"
#include "texture_grid.h"

// Pre-tessellated displaced mesh: evaluate S = P + h*N on a uniform
// barycentric grid per base triangle and emit a ks MeshData. This is the
// truth geometry and the visibility substrate of the A-MVP (plan phase S2);
// the samplers never intersect the tessellation-free representation.
// Grid and micro-face layout mirror the numpy reference
// (code/python/poc/dmapref/reference_surface.py, tessellation_grid).

namespace dmap
{

// Uniform barycentric grid on the triangle domain at subdivision n:
// nodes (i/n, j/n) for i + j <= n; faces alternate up/down micro-triangles.
struct TessellationGrid
{
    std::vector<ks::vec2d> params;
    std::vector<Eigen::Vector3i> faces;
};
TessellationGrid tessellation_grid(int n);

// Append one displaced base triangle at subdivision n: positions S(u, v),
// texcoords (u, v), vertex normals from the normalized surface normal.
// Buffers are unpadded until finalize_mesh_data.
void append_displaced_triangle(ks::MeshData &out, const BaseTriangle &tri, const HeightGrid &field, int n);

// Apply the MeshData buffer padding convention (ks/geometry.h). Call once,
// after the last append.
void finalize_mesh_data(ks::MeshData &out);

// Whole mesh with the project's amplitude convention:
// per-triangle scale = amplitude * mean_edge.
ks::MeshData build_displaced_mesh(const BaseMesh &mesh, const TextureGrid &tex, double amplitude, int n);

// Sum of triangle areas of a finalized MeshData.
double mesh_data_area(const ks::MeshData &m);

// Area of the smooth displaced surface over the triangle domain: centroid
// quadrature of sqrt(det G) on tessellation_grid(n_quad) micro-faces
// (each has parameter area 0.5 / n_quad^2).
double metric_area(const BaseTriangle &tri, const HeightGrid &field, int n_quad);

} // namespace dmap
