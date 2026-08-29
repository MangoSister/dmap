#include "displaced_tessellation.h"
#include "ks/assertion.h"

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

TessellationGrid tessellation_grid(int n)
{
    TessellationGrid grid;
    // Node (i, j) sits at index j*(n+1) - j*(j-1)/2 + i (row j holds n+1-j nodes).
    auto index = [n](int i, int j) { return j * (n + 1) - j * (j - 1) / 2 + i; };
    for (int j = 0; j <= n; ++j) {
        for (int i = 0; i <= n - j; ++i) {
            grid.params.emplace_back((double)i / n, (double)j / n);
        }
    }
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n - j; ++i) {
            grid.faces.emplace_back(index(i, j), index(i + 1, j), index(i, j + 1));
            if (i + j < n - 1) {
                grid.faces.emplace_back(index(i + 1, j), index(i + 1, j + 1), index(i, j + 1));
            }
        }
    }
    return grid;
}

void append_displaced_triangle(ks::MeshData &out, const BaseTriangle &tri, const HeightGrid &field, int n)
{
    TessellationGrid grid = tessellation_grid(n);
    uint32_t base = (uint32_t)(out.vertices.size() / 3);
    for (const vec2d &p : grid.params) {
        double u = p[0], v = p[1];
        vec3d S = displaced_position(tri, field, u, v);
        PointwiseFields f = pointwise_fields(tri, field, u, v);
        vec3d normal = f.n.normalized();
        for (int c = 0; c < 3; ++c)
            out.vertices.push_back((float)S[c]);
        out.texcoords.push_back((float)u);
        out.texcoords.push_back((float)v);
        for (int c = 0; c < 3; ++c)
            out.vertex_normals.push_back((float)normal[c]);
    }
    for (const Eigen::Vector3i &f : grid.faces) {
        out.indices.push_back(base + f[0]);
        out.indices.push_back(base + f[1]);
        out.indices.push_back(base + f[2]);
    }
}

void finalize_mesh_data(ks::MeshData &out)
{
    // Buffer padding convention from ks/geometry.h: 1 dummy float on the
    // vertex buffer, 2 on texcoords, 1 on vertex normals.
    out.vertices.push_back(0.0f);
    out.texcoords.push_back(0.0f);
    out.texcoords.push_back(0.0f);
    out.vertex_normals.push_back(0.0f);
}

ks::MeshData build_displaced_mesh(const BaseMesh &mesh, const TextureGrid &tex, double amplitude, int n)
{
    ks::MeshData out;
    for (int t = 0; t < mesh.n_triangles(); ++t) {
        BaseTriangle tri = mesh.triangle(t);
        HeightGrid field{tex.W, tex.H, amplitude * mean_edge(tri), tex.values.data()};
        append_displaced_triangle(out, tri, field, n);
    }
    finalize_mesh_data(out);
    return out;
}

double mesh_data_area(const ks::MeshData &m)
{
    double area = 0.0;
    for (int t = 0; t < m.tri_count(); ++t) {
        vec3d p0 = m.get_pos(m.indices[3 * t]).cast<double>();
        vec3d p1 = m.get_pos(m.indices[3 * t + 1]).cast<double>();
        vec3d p2 = m.get_pos(m.indices[3 * t + 2]).cast<double>();
        area += 0.5 * (p1 - p0).cross(p2 - p0).norm();
    }
    return area;
}

double metric_area(const BaseTriangle &tri, const HeightGrid &field, int n_quad)
{
    TessellationGrid grid = tessellation_grid(n_quad);
    double w = 0.5 / ((double)n_quad * n_quad);
    double area = 0.0;
    for (const Eigen::Vector3i &f : grid.faces) {
        vec2d c = (grid.params[f[0]] + grid.params[f[1]] + grid.params[f[2]]) / 3.0;
        area += w * pointwise_fields(tri, field, c[0], c[1]).sqrt_det;
    }
    return area;
}

} // namespace dmap
