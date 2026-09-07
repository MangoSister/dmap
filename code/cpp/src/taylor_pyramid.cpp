#include "taylor_pyramid.h"
#include "ks/assertion.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

namespace
{

// Per-texel bilinear coefficients (note S3): centred on the texel,
// h = c0 + c1*du + c2*dv + c3*du*dv. Only the direct construction needs
// them for the whole grid at once; the leaf level computes its own inline.
struct TexelCoeffs
{
    std::vector<double> c1, c2, c3;
};

TexelCoeffs texel_coeffs(const std::vector<double> &V, int nodes, int n_leaf)
{
    TexelCoeffs out;
    size_t count = (size_t)n_leaf * n_leaf;
    out.c1.resize(count);
    out.c2.resize(count);
    out.c3.resize(count);
    double w = 1.0 / n_leaf;
    for (int j = 0; j < n_leaf; ++j) {
        for (int i = 0; i < n_leaf; ++i) {
            double v00 = V[(size_t)j * nodes + i], v10 = V[(size_t)j * nodes + i + 1];
            double v01 = V[(size_t)(j + 1) * nodes + i], v11 = V[(size_t)(j + 1) * nodes + i + 1];
            size_t k = (size_t)j * n_leaf + i;
            out.c1[k] = 0.5 * ((v10 - v00) + (v11 - v01)) / w;
            out.c2[k] = 0.5 * ((v01 - v00) + (v11 - v10)) / w;
            out.c3[k] = (v00 - v10 - v01 + v11) / (w * w);
        }
    }
    return out;
}

// Exact node per texel cell (note S3): the bilinear patch is
// h = c0 + c1*du + c2*dv + c3*du*dv in centred coordinates, and every
// supremum is attained at a cell corner, which also makes the min-max
// channel the range of the four corners.
TaylorLevel leaf_level(const std::vector<double> &V, int nodes, int n_leaf)
{
    TaylorLevel out(n_leaf);
    double w = 1.0 / n_leaf;
    double s = 0.5 * w;
    for (int j = 0; j < n_leaf; ++j) {
        for (int i = 0; i < n_leaf; ++i) {
            double v00 = V[(size_t)j * nodes + i], v10 = V[(size_t)j * nodes + i + 1];
            double v01 = V[(size_t)(j + 1) * nodes + i], v11 = V[(size_t)(j + 1) * nodes + i + 1];
            size_t k = (size_t)j * n_leaf + i;
            out.h0[k] = 0.25 * (v00 + v10 + v01 + v11);
            out.gu[k] = 0.5 * ((v10 - v00) + (v11 - v01)) / w;
            out.gv[k] = 0.5 * ((v01 - v00) + (v11 - v10)) / w;
            double c3 = (v00 - v10 - v01 + v11) / (w * w);
            out.r[k] = std::abs(c3) * s * s;
            out.ru[k] = std::abs(c3) * s;
            out.rv[k] = std::abs(c3) * s;
            out.h_min[k] = std::min(std::min(v00, v10), std::min(v01, v11));
            out.h_max[k] = std::max(std::max(v00, v10), std::max(v01, v11));
        }
    }
    return out;
}

// Conservative 4-to-1 fold (note S4, with the plane built from interval
// hulls): the parent gradient interval is the interval hull of the child
// intervals with the slope at its midpoint; with the parent slope fixed, the
// child planes minus the parent slope are affine per child quadrant, so
// their extremes sit at quadrant corners, and the parent offset is the
// midpoint of the corner extremes (including each child's r). The height
// range folds by min and max, which is exact: the range over a union is the
// union of the ranges.
TaylorLevel fold(const TaylorLevel &child)
{
    int m = child.m / 2;
    TaylorLevel out(m);
    double s = 0.5 / m; // parent half-extent
    const double off[2] = {-0.5 * s, 0.5 * s};
    const double corner_off[2] = {-0.5 * s, 0.5 * s};

    for (int J = 0; J < m; ++J) {
        for (int I = 0; I < m; ++I) {
            size_t kp = (size_t)J * m + I;

            // Gradient interval hulls and the height range over the four
            // children.
            double gu_hi = -INFINITY, gu_lo = INFINITY, gv_hi = -INFINITY, gv_lo = INFINITY;
            double h_min = INFINITY, h_max = -INFINITY;
            for (int a = 0; a < 2; ++a) {
                for (int b = 0; b < 2; ++b) {
                    size_t kc = (size_t)(2 * J + a) * child.m + (2 * I + b);
                    gu_hi = std::max(gu_hi, child.gu[kc] + child.ru[kc]);
                    gu_lo = std::min(gu_lo, child.gu[kc] - child.ru[kc]);
                    gv_hi = std::max(gv_hi, child.gv[kc] + child.rv[kc]);
                    gv_lo = std::min(gv_lo, child.gv[kc] - child.rv[kc]);
                    h_min = std::min(h_min, child.h_min[kc]);
                    h_max = std::max(h_max, child.h_max[kc]);
                }
            }
            double gu_p = 0.5 * (gu_hi + gu_lo);
            double gv_p = 0.5 * (gv_hi + gv_lo);
            out.gu[kp] = gu_p;
            out.ru[kp] = 0.5 * (gu_hi - gu_lo);
            out.gv[kp] = gv_p;
            out.rv[kp] = 0.5 * (gv_hi - gv_lo);
            out.h_min[kp] = h_min;
            out.h_max[kp] = h_max;

            // Child plane minus the parent slope, at the child quadrant
            // corners; dy/dx are the child centre offsets from the parent
            // centre (a indexes v, b indexes u).
            double upper = -INFINITY, lower = INFINITY;
            for (int a = 0; a < 2; ++a) {
                for (int b = 0; b < 2; ++b) {
                    size_t kc = (size_t)(2 * J + a) * child.m + (2 * I + b);
                    double dx = off[b], dy = off[a];
                    double du_g = child.gu[kc] - gu_p;
                    double dv_g = child.gv[kc] - gv_p;
                    double base = child.h0[kc] - child.gu[kc] * dx - child.gv[kc] * dy;
                    for (int cu = 0; cu < 2; ++cu) {
                        for (int cv = 0; cv < 2; ++cv) {
                            double corner = base + du_g * (dx + corner_off[cu]) + dv_g * (dy + corner_off[cv]);
                            upper = std::max(upper, corner + child.r[kc]);
                            lower = std::min(lower, corner - child.r[kc]);
                        }
                    }
                }
            }
            out.h0[kp] = 0.5 * (upper + lower);
            out.r[kp] = 0.5 * (upper - lower);
        }
    }
    return out;
}

// One level built from the node grid instead of by folding (note S4,
// direct construction). Step 1 (gradient): on a texel h_u = c1 + c3*dv is
// linear in dv, so its exact range there is c1 +- |c3|*s_texel, and the
// cell's range is the hull of the ranges of its texels. Step 2 (value):
// with that slope fixed, the residual h - gu*du - gv*dv is bilinear per
// texel, so its extremes over the cell sit at the nodes of the closed cell,
// k + 1 per side, boundary included. Both steps are exact, so r is the
// smallest remainder the chosen slope admits.
//
// Every level reads only the node grid, so the levels are independent of
// each other and could be built in any order or in parallel.
TaylorLevel direct_level(const std::vector<double> &V, int nodes, const TexelCoeffs &c, int n_leaf, int level)
{
    int k = 1 << level;      // texels per cell side
    int m = n_leaf >> level; // cells per side
    TaylorLevel out(m);
    double s_texel = 0.5 / n_leaf;

    for (int J = 0; J < m; ++J) {
        for (int I = 0; I < m; ++I) {
            size_t kp = (size_t)J * m + I;

            double gu_hi = -INFINITY, gu_lo = INFINITY, gv_hi = -INFINITY, gv_lo = INFINITY;
            for (int b = 0; b < k; ++b) {
                for (int a = 0; a < k; ++a) {
                    size_t kt = (size_t)(J * k + b) * n_leaf + (I * k + a);
                    double spread = std::abs(c.c3[kt]) * s_texel;
                    gu_hi = std::max(gu_hi, c.c1[kt] + spread);
                    gu_lo = std::min(gu_lo, c.c1[kt] - spread);
                    gv_hi = std::max(gv_hi, c.c2[kt] + spread);
                    gv_lo = std::min(gv_lo, c.c2[kt] - spread);
                }
            }
            double gu = 0.5 * (gu_hi + gu_lo);
            double gv = 0.5 * (gv_hi + gv_lo);
            out.gu[kp] = gu;
            out.ru[kp] = 0.5 * (gu_hi - gu_lo);
            out.gv[kp] = gv;
            out.rv[kp] = 0.5 * (gv_hi - gv_lo);

            double upper = -INFINITY, lower = INFINITY;
            double h_min = INFINITY, h_max = -INFINITY;
            for (int b = 0; b <= k; ++b) {
                double dv = (b - 0.5 * k) / n_leaf;
                for (int a = 0; a <= k; ++a) {
                    double du = (a - 0.5 * k) / n_leaf;
                    double value = V[(size_t)(J * k + b) * nodes + (I * k + a)];
                    double residual = value - gu * du - gv * dv;
                    upper = std::max(upper, residual);
                    lower = std::min(lower, residual);
                    h_min = std::min(h_min, value);
                    h_max = std::max(h_max, value);
                }
            }
            out.h0[kp] = 0.5 * (upper + lower);
            out.r[kp] = 0.5 * (upper - lower);
            out.h_min[kp] = h_min;
            out.h_max[kp] = h_max;
        }
    }
    return out;
}

} // namespace

PyramidBuild pyramid_build_from_string(const std::string &name)
{
    if (name == "fold")
        return PyramidBuild::Fold;
    ASSERT(name == "direct", "unknown pyramid build [%s], expected fold or direct", name.c_str());
    return PyramidBuild::Direct;
}

const char *pyramid_build_name(PyramidBuild build) { return build == PyramidBuild::Fold ? "fold" : "direct"; }

TaylorPyramid::TaylorPyramid(const double *values, int nodes, double scale, PyramidBuild build) : build(build)
{
    int n = nodes - 1;
    ASSERT(n >= 1 && (n & (n - 1)) == 0, "pyramid needs a (2^L + 1) x (2^L + 1) node grid, got %d nodes", nodes);
    n_leaf = n;
    n_levels = 1;
    while ((1 << n_levels) <= n)
        ++n_levels; // L + 1 levels, root included

    std::vector<double> V((size_t)nodes * nodes);
    for (size_t k = 0; k < V.size(); ++k)
        V[k] = values[k] * scale;

    if (build == PyramidBuild::Fold) {
        levels.push_back(leaf_level(V, nodes, n_leaf));
        while (levels.back().m > 1)
            levels.push_back(fold(levels.back()));
    } else {
        TexelCoeffs c = texel_coeffs(V, nodes, n_leaf);
        for (int level = 0; level < n_levels; ++level)
            levels.push_back(direct_level(V, nodes, c, n_leaf, level));
    }
}

void minmax_from_taylor(const TaylorLevel &level, double s, std::vector<double> &lo, std::vector<double> &hi)
{
    size_t n = level.h0.size();
    lo.resize(n);
    hi.resize(n);
    for (size_t k = 0; k < n; ++k) {
        double half = std::abs(level.gu[k]) * s + std::abs(level.gv[k]) * s + level.r[k];
        lo[k] = level.h0[k] - half;
        hi[k] = level.h0[k] + half;
    }
}

MipPyramid::MipPyramid(const double *cell_values, int n)
{
    ASSERT(n >= 1 && (n & (n - 1)) == 0, "mip pyramid needs a 2^L x 2^L cell grid, got %d", n);
    n_leaf = n;
    n_levels = 1;
    while ((1 << n_levels) <= n)
        ++n_levels;

    MipLevel leaf;
    leaf.m = n;
    leaf.mean.assign(cell_values, cell_values + (size_t)n * n);
    leaf.max.assign(cell_values, cell_values + (size_t)n * n);
    levels.push_back(std::move(leaf));

    while (levels.back().m > 1) {
        const MipLevel &c = levels.back();
        MipLevel p;
        p.m = c.m / 2;
        p.mean.resize((size_t)p.m * p.m);
        p.max.resize((size_t)p.m * p.m);
        for (int J = 0; J < p.m; ++J) {
            for (int I = 0; I < p.m; ++I) {
                double sum = 0.0, mx = -INFINITY;
                for (int a = 0; a < 2; ++a) {
                    for (int b = 0; b < 2; ++b) {
                        double v = c.mean[(size_t)(2 * J + a) * c.m + (2 * I + b)];
                        double vmax = c.max[(size_t)(2 * J + a) * c.m + (2 * I + b)];
                        sum += v;
                        mx = std::max(mx, vmax);
                    }
                }
                p.mean[(size_t)J * p.m + I] = 0.25 * sum;
                p.max[(size_t)J * p.m + I] = mx;
            }
        }
        levels.push_back(std::move(p));
    }
}

} // namespace dmap
