#pragma once
#include "ks/maths.h"
#include <vector>

// Pointwise evaluation of a displaced surface S = P + h*N over one base
// triangle. Port of the numpy reference (code/python/poc/dmapref:
// interpolant.py, mesh.py, metric.py, dense_reference.py). All math in
// double.
//
// Formulas from the note "The induced metric of a displaced surface":
//     G = G0 - 2h*B0 + h^2*C0 + gh*gh^T + gh*a^T + a*gh^T        (S3, S6)
// with gh = (h_u, h_v). B0 is symmetrized; C0 comes directly from Nu, Nv
// (the Weingarten identity fails for interpolated vertex normals).
//
// The chart. A triangle is parameterized by the coordinates its
// displacement texture is indexed with. Under the identity chart those are
// the barycentric coordinates (vertex 0 at (0, 0), vertex 1 at (1, 0),
// vertex 2 at (0, 1)), the convention of every A-MVP experiment. Under a
// general chart each vertex carries its own texture coordinates, and the
// map from texture coordinates to barycentric coordinates is one constant
// affine map per triangle. Because P and M are affine in the barycentric
// coordinates, the triangle parameterized by texture coordinates is again
// of the form P = q0 + s*e1 + t*e2, M = m0 + s*Mu + t*Mv, so every formula
// below runs unchanged; only the domain (a general triangle in the texture
// plane) and the vertex positions differ. Measured: bound tightness is
// chart-indifferent for the metric quantities ("Result — Chart dependence
// study").

namespace dmap
{

// Bilinear height field over the unit tile. values[j*W + i] sits at
// (x, y) = (i/(W-1), j/(H-1)); the gradient is analytic per cell.
//     h = scale * bilinear(values) + offset,
// so a texture value t in [0, 1] displaces by strength * (t - midlevel)
// with scale = strength, offset = -strength * midlevel (the Blender
// Displace-modifier convention). Coordinates outside the tile wrap when
// repeat is set and clamp otherwise.
// Non-owning view: one node grid is shared by many triangles.
struct HeightGrid
{
    int W = 0, H = 0;
    double scale = 1.0;
    const double *values = nullptr;
    double offset = 0.0;
    bool repeat = false;

    double value(int j, int i) const { return values[(size_t)j * W + i]; }

    // Cell index and local coordinates, matching the numpy reference:
    // fx = wrap_or_clip(x) * (W - 1); i = min(floor(fx), W - 2).
    void cell(double x, double y, int &i, int &j, double &s, double &t) const;

    double h(double x, double y) const;
    ks::vec2d grad(double x, double y) const;
};

// One base triangle in its chart's parameterization (s, t):
//     P(s, t) = q0 + s*e1 + t*e2,   M(s, t) = m0 + s*Mu + t*Mv,
// displacement direction N = M/|M|. p0, p1, p2 are the vertex positions,
// t0, t1, t2 their parameters (texture coordinates); under the identity
// chart q0 = p0, e1 = p1 - p0, e2 = p2 - p0.
struct BaseTriangle
{
    ks::vec3d q0, e1, e2;
    ks::vec3d m0, Mu, Mv;
    Eigen::Matrix2d G0;

    ks::vec3d p0, p1, p2;
    ks::vec2d t0, t1, t2;
    // Barycentric coordinates from parameters: (u, v) = J * ((s, t) - t0).
    Eigen::Matrix2d J;

    // Identity chart.
    BaseTriangle(const ks::vec3d &p0, const ks::vec3d &p1, const ks::vec3d &p2, const ks::vec3d &m0,
                 const ks::vec3d &m1, const ks::vec3d &m2);
    // General chart: per-vertex texture coordinates. Asserts the triangle
    // is not degenerate in the texture plane.
    BaseTriangle(const ks::vec3d &p0, const ks::vec3d &p1, const ks::vec3d &p2, const ks::vec3d &m0,
                 const ks::vec3d &m1, const ks::vec3d &m2, const ks::vec2d &t0, const ks::vec2d &t1,
                 const ks::vec2d &t2);

    ks::vec3d P(double s, double t) const { return q0 + s * e1 + t * e2; }
    ks::vec3d M(double s, double t) const { return m0 + s * Mu + t * Mv; }

    ks::vec2d bary(double s, double t) const { return J * (ks::vec2d(s, t) - t0); }
    ks::vec2d param(double u, double v) const { return t0 + u * (t1 - t0) + v * (t2 - t0); }
    bool inside(double s, double t) const
    {
        ks::vec2d b = bary(s, t);
        return b[0] >= 0.0 && b[1] >= 0.0 && b[0] + b[1] <= 1.0;
    }
    // Area of the domain in parameter units, |det[t1 - t0, t2 - t0]| / 2.
    double param_area() const;
};

// Unit direction N and its parameter derivatives at (s, t):
// Nu = (I - N N^T) Mu / |M| (projector form of the normalization derivative).
struct NormalFrame
{
    ks::vec3d N, Nu, Nv;
};
NormalFrame normal_frame_at(const BaseTriangle &tri, double s, double t);

// All pointwise quantities the samplers and validators need.
struct PointwiseFields
{
    double h, hu, hv;
    double G00, G01, G11;
    double det, sqrt_det;
    double lam_min, lam_max; // eigenvalues of G0^{-1} G
    ks::vec3d n;             // unnormalized surface normal Su x Sv
};
PointwiseFields pointwise_fields(const BaseTriangle &tri, const HeightGrid &field, double s, double t);

// The same fields with (h, hu, hv) supplied by the caller instead of read
// from the height field. Descent weights (plan S6) evaluate a node's Taylor
// plane at the cell centre through the exact pointwise formulas.
PointwiseFields pointwise_fields(const BaseTriangle &tri, double s, double t, double h, double hu, double hv);

// Displaced position S(s, t) = P + h*N.
ks::vec3d displaced_position(const BaseTriangle &tri, const HeightGrid &field, double s, double t);

// The base face's winding normal (p1 - p0) x (p2 - p0), unnormalized: the
// orientation ks's meshes use for a one-sided surface. The chart is
// mirrored when e1 x e2 points against it (flip_v does that), and then
// Su x Sv points against the winding normal too.
ks::vec3d winding_normal(const BaseTriangle &tri);
bool chart_mirrored(const BaseTriangle &tri);

// Mean of the three side lengths of the base triangle; the experiments'
// amplitude convention is scale = amplitude * mean_edge.
double mean_edge(const BaseTriangle &tri);

} // namespace dmap
