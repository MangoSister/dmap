#include "taylor_pyramid.h"
#include "ks/assertion.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

namespace
{

// Exact node per texel cell (note S3): the bilinear patch is
// h = c0 + c1*du + c2*dv + c3*du*dv in centred coordinates, and every
// supremum is attained at a cell corner.
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
        }
    }
    return out;
}

// Conservative 4-to-1 fold (note S4, with the plane built from interval
// hulls): the parent gradient interval is the interval hull of the child
// intervals with the slope at its midpoint; with the parent slope fixed,
// the child planes minus the parent slope are affine per child quadrant, so
// their extremes sit at quadrant corners, and the parent offset is the
// midpoint of the corner extremes (including each child's r).
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

            // Gradient interval hulls over the four children.
            double gu_hi = -INFINITY, gu_lo = INFINITY, gv_hi = -INFINITY, gv_lo = INFINITY;
            for (int a = 0; a < 2; ++a) {
                for (int b = 0; b < 2; ++b) {
                    size_t kc = (size_t)(2 * J + a) * child.m + (2 * I + b);
                    gu_hi = std::max(gu_hi, child.gu[kc] + child.ru[kc]);
                    gu_lo = std::min(gu_lo, child.gu[kc] - child.ru[kc]);
                    gv_hi = std::max(gv_hi, child.gv[kc] + child.rv[kc]);
                    gv_lo = std::min(gv_lo, child.gv[kc] - child.rv[kc]);
                }
            }
            double gu_p = 0.5 * (gu_hi + gu_lo);
            double gv_p = 0.5 * (gv_hi + gv_lo);
            out.gu[kp] = gu_p;
            out.ru[kp] = 0.5 * (gu_hi - gu_lo);
            out.gv[kp] = gv_p;
            out.rv[kp] = 0.5 * (gv_hi - gv_lo);

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

} // namespace

TaylorPyramid::TaylorPyramid(const double *values, int nodes, double scale)
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

    levels.push_back(leaf_level(V, nodes, n_leaf));
    while (levels.back().m > 1)
        levels.push_back(fold(levels.back()));
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
