#include "displaced_tessellation.h"
#include "ks/assertion.h"
#include <cmath>

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
        // The grid is barycentric; the surface and the texture coordinates
        // are evaluated through the triangle's chart.
        vec2d st = tri.param(p[0], p[1]);
        double s = st[0], t = st[1];
        vec3d S = displaced_position(tri, field, s, t);
        PointwiseFields f = pointwise_fields(tri, field, s, t);
        vec3d normal = f.n.normalized();
        for (int c = 0; c < 3; ++c)
            out.vertices.push_back((float)S[c]);
        out.texcoords.push_back((float)s);
        out.texcoords.push_back((float)t);
        for (int c = 0; c < 3; ++c)
            out.vertex_normals.push_back((float)normal[c]);
    }
    for (const Eigen::Vector3i &f : grid.faces) {
        out.indices.push_back(base + f[0]);
        out.indices.push_back(base + f[1]);
        out.indices.push_back(base + f[2]);
    }
}

namespace
{

void append_vertex(ks::MeshData &out, const BaseTriangle &tri, const HeightGrid &field, const vec2d &st)
{
    vec3d S = displaced_position(tri, field, st[0], st[1]);
    // Su x Sv, oriented along the base face's winding as ks's meshes are.
    vec3d normal = pointwise_fields(tri, field, st[0], st[1]).n.normalized();
    if (chart_mirrored(tri))
        normal = -normal;
    for (int c = 0; c < 3; ++c)
        out.vertices.push_back((float)S[c]);
    out.texcoords.push_back((float)st[0]);
    out.texcoords.push_back((float)st[1]);
    for (int c = 0; c < 3; ++c)
        out.vertex_normals.push_back((float)normal[c]);
}

// The face keeps the base face's winding: under a mirrored chart the
// lattice order (a, b, c) is reversed.
void append_face(ks::MeshData &out, const BaseTriangle &tri, const HeightGrid &field, const vec2d &a, vec2d b, vec2d c,
                 int triangle, bool interior, std::vector<TexelFace> &faces)
{
    if (chart_mirrored(tri))
        std::swap(b, c);
    uint32_t base = (uint32_t)(out.vertices.size() / 3);
    append_vertex(out, tri, field, a);
    append_vertex(out, tri, field, b);
    append_vertex(out, tri, field, c);
    out.indices.push_back(base);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
    TexelFace face;
    face.triangle = triangle;
    face.st[0] = a;
    face.st[1] = b;
    face.st[2] = c;
    face.interior = interior;
    faces.push_back(face);
}

} // namespace

void append_texel_tessellation(ks::MeshData &out, const BaseTriangle &tri, const HeightGrid &field, int n_leaf, int m,
                               int triangle, std::vector<TexelFace> &faces)
{
    UvTriangle domain(tri.t0, tri.t1, tri.t2);
    double w = 1.0 / n_leaf;
    int64_t i_lo = (int64_t)std::floor(domain.lo[0] / w), i_hi = (int64_t)std::ceil(domain.hi[0] / w) - 1;
    int64_t j_lo = (int64_t)std::floor(domain.lo[1] / w), j_hi = (int64_t)std::ceil(domain.hi[1] / w) - 1;
    for (int64_t j = j_lo; j <= j_hi; ++j) {
        for (int64_t i = i_lo; i <= i_hi; ++i) {
            vec2d c((i + 0.5) * w, (j + 0.5) * w);
            Overlap overlap = classify_square(domain, c, 0.5 * w);
            if (overlap == Overlap::Outside)
                continue;
            double ws = w / m;
            for (int b = 0; b < m; ++b) {
                for (int a = 0; a < m; ++a) {
                    vec2d p00(i * w + a * ws, j * w + b * ws), p10 = p00 + vec2d(ws, 0.0), p01 = p00 + vec2d(0.0, ws),
                                                               p11 = p00 + vec2d(ws, ws);
                    vec2d sub_centre = p00 + vec2d(0.5 * ws, 0.5 * ws);
                    Overlap sub =
                        overlap == Overlap::Inside ? Overlap::Inside : classify_square(domain, sub_centre, 0.5 * ws);
                    if (sub == Overlap::Outside)
                        continue;
                    if (sub == Overlap::Straddle) {
                        ClipPolygon poly = clip_square(domain, sub_centre, 0.5 * ws);
                        for (int k = 1; k + 1 < poly.n; ++k)
                            append_face(out, tri, field, poly.p[0], poly.p[k], poly.p[k + 1], triangle, false, faces);
                        continue;
                    }
                    append_face(out, tri, field, p00, p10, p01, triangle, true, faces);
                    append_face(out, tri, field, p10, p11, p01, triangle, true, faces);
                }
            }
        }
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
    // Each micro-face has parameter area (domain area) / n_quad^2, and
    // sqrt(det G) is the area density in the chart's parameters.
    double w = tri.param_area() / ((double)n_quad * n_quad);
    double area = 0.0;
    for (const Eigen::Vector3i &f : grid.faces) {
        vec2d c = (grid.params[f[0]] + grid.params[f[1]] + grid.params[f[2]]) / 3.0;
        vec2d st = tri.param(c[0], c[1]);
        area += w * pointwise_fields(tri, field, st[0], st[1]).sqrt_det;
    }
    return area;
}

} // namespace dmap
