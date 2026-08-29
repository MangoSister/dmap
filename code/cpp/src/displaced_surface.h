#pragma once
#include "ks/maths.h"
#include <vector>

// Pointwise evaluation of a displaced surface S = P + h*N over one base
// triangle with an identity chart (triangle (u, v) = texture coordinates).
// Port of the numpy reference (code/python/poc/dmapref: interpolant.py,
// mesh.py, metric.py, dense_reference.py). All math in double.
//
// Formulas from the note "The induced metric of a displaced surface":
//     G = G0 - 2h*B0 + h^2*C0 + gh*gh^T + gh*a^T + a*gh^T        (S3, S6)
// with gh = (h_u, h_v). B0 is symmetrized; C0 comes directly from Nu, Nv
// (the Weingarten identity fails for interpolated vertex normals).

namespace dmap
{

// Bilinear height field over the unit square. values[j*W + i] sits at
// (x, y) = (i/(W-1), j/(H-1)); the gradient is analytic per cell.
// Non-owning view: one texture grid is shared by many triangles, each with
// its own scale (amplitude x mean edge length).
struct HeightGrid
{
    int W = 0, H = 0;
    double scale = 1.0;
    const double *values = nullptr;

    double value(int j, int i) const { return values[(size_t)j * W + i]; }

    // Cell index and local coordinates, matching the numpy reference:
    // fx = clip(x, 0, 1) * (W - 1); i = min(floor(fx), W - 2).
    void cell(double x, double y, int &i, int &j, double &s, double &t) const;

    double h(double x, double y) const;
    ks::vec2d grad(double x, double y) const;
};

// One base triangle: P(u, v) = q0 + u*e1 + v*e2, displacement direction
// N = M/|M| with M the linearly interpolated unit vertex normal.
struct BaseTriangle
{
    ks::vec3d q0, q1, q2;
    ks::vec3d m0, m1, m2;
    ks::vec3d e1, e2, Mu, Mv;
    Eigen::Matrix2d G0;

    BaseTriangle(const ks::vec3d &q0, const ks::vec3d &q1, const ks::vec3d &q2, const ks::vec3d &m0,
                 const ks::vec3d &m1, const ks::vec3d &m2);

    ks::vec3d P(double u, double v) const { return q0 + u * e1 + v * e2; }
    ks::vec3d M(double u, double v) const { return m0 + u * Mu + v * Mv; }
};

// Unit direction N and its parameter derivatives at (u, v):
// Nu = (I - N N^T) Mu / |M| (projector form of the normalization derivative).
struct NormalFrame
{
    ks::vec3d N, Nu, Nv;
};
NormalFrame normal_frame_at(const BaseTriangle &tri, double u, double v);

// All pointwise quantities the samplers and validators need.
struct PointwiseFields
{
    double h, hu, hv;
    double G00, G01, G11;
    double det, sqrt_det;
    double lam_min, lam_max; // eigenvalues of G0^{-1} G
    ks::vec3d n;             // unnormalized surface normal Su x Sv
};
PointwiseFields pointwise_fields(const BaseTriangle &tri, const HeightGrid &field, double u, double v);

// The same fields with (h, hu, hv) supplied by the caller instead of read
// from the height field. Descent weights (plan S6) evaluate a node's Taylor
// plane at the cell centre through the exact pointwise formulas.
PointwiseFields pointwise_fields(const BaseTriangle &tri, double u, double v, double h, double hu, double hv);

// Displaced position S(u, v) = P + h*N.
ks::vec3d displaced_position(const BaseTriangle &tri, const HeightGrid &field, double u, double v);

// Mean of the three side lengths; the project's amplitude convention is
// scale = amplitude * mean_edge.
double mean_edge(const BaseTriangle &tri);

} // namespace dmap
