#include "descent_sampler.h"
#include "ks/assertion.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

double clipped_cell_area(double u0, double v0, double w)
{
    auto T = [](double x) {
        x = std::max(x, 0.0);
        return 0.5 * x * x;
    };
    double d = 1.0 - u0 - v0;
    return T(d) - 2.0 * T(d - w) + T(d - 2.0 * w);
}

DescentSampler::DescentSampler(const BaseTriangle &tri, const HeightGrid &field, const TextureGrid &emission,
                               DescentWeight variant, double beta)
    : tri(&tri), field(field), variant(variant), beta(beta), pyramid(field.values, field.W, field.scale)
{
    ASSERT(field.W == field.H, "descent sampler needs a square node grid");
    ASSERT(emission.W == pyramid.n_leaf && emission.H == pyramid.n_leaf,
           "emission must be an n_leaf x n_leaf texel grid (got %d x %d, n_leaf %d)", emission.W, emission.H,
           pyramid.n_leaf);

    // Leaf: integral of the piecewise-constant emission over texel ∩ domain.
    int n = pyramid.n_leaf;
    double wl = 1.0 / n;
    e_sum.resize(pyramid.n_levels);
    e_sum[0].resize((size_t)n * n);
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
            e_sum[0][(size_t)j * n + i] = emission.values[(size_t)j * n + i] * clipped_cell_area(i * wl, j * wl, wl);

    // Fold by summation (exact: the integral is additive over children).
    for (int level = 1; level < pyramid.n_levels; ++level) {
        int m = pyramid.levels[level].m;
        e_sum[level].resize((size_t)m * m);
        const std::vector<double> &child = e_sum[level - 1];
        for (int J = 0; J < m; ++J)
            for (int I = 0; I < m; ++I) {
                double sum = 0.0;
                for (int a = 0; a < 2; ++a)
                    for (int b = 0; b < 2; ++b)
                        sum += child[(size_t)(2 * J + a) * (2 * m) + (2 * I + b)];
                e_sum[level][(size_t)J * m + I] = sum;
            }
    }
}

bool DescentSampler::admissible(int level, int i, int j) const
{
    if (variant == DescentWeight::AreaOnly) {
        double w = pyramid.cell_width(level);
        return clipped_cell_area(i * w, j * w, w) > 0.0;
    }
    // The emission sum certifies mass: zero means E == 0 on cell ∩ domain,
    // so pruning is exact (§5A certified-zero at texel granularity).
    return e_sum[level][(size_t)j * pyramid.levels[level].m + i] > 0.0;
}

double DescentSampler::node_weight(int level, int i, int j, const Receiver *receiver) const
{
    const TaylorLevel &lvl = pyramid.levels[level];
    size_t k = (size_t)j * lvl.m + i;
    double w = pyramid.cell_width(level);
    double u0 = (i + 0.5) * w, v0 = (j + 0.5) * w;

    // Midpoint composition (§5A composedWeight): the node's Taylor plane
    // (h0, gu, gv) through the exact pointwise metric formulas.
    PointwiseFields f = pointwise_fields(*tri, u0, v0, lvl.h0[k], lvl.gu[k], lvl.gv[k]);

    double base = (variant == DescentWeight::AreaOnly) ? clipped_cell_area(i * w, j * w, w) : e_sum[level][k];
    double weight = base * f.sqrt_det;

    if (variant == DescentWeight::ProductGeometry && receiver) {
        // Spatial centre: midpoint height along the centre normal (§5A).
        NormalFrame nf = normal_frame_at(*tri, u0, v0);
        vec3d y = tri->P(u0, v0) + lvl.h0[k] * nf.N;
        vec3d d = y - receiver->x;
        double r2 = std::max(d.squaredNorm(), 1e-12);
        vec3d omega = d / std::sqrt(r2);
        // Clamped receiver cosine x |emitter cosine| / r^2. A zero midpoint
        // cosine does not prove the cell contributes nothing, so the blend
        // floor in child_probs keeps its probability positive.
        double cos_r = std::max(receiver->n.dot(omega), 0.0);
        double cos_e = emitter_cosine ? std::abs(f.n.normalized().dot(omega)) : 1.0;
        weight *= cos_r * cos_e / r2;
    }
    return weight;
}

void DescentSampler::child_probs(int level, int pi, int pj, const Receiver *receiver, double p[4]) const
{
    double w[4];
    bool adm[4];
    int n_adm = 0;
    double sumw = 0.0;
    for (int c = 0; c < 4; ++c) {
        int i = 2 * pi + (c & 1), j = 2 * pj + (c >> 1);
        adm[c] = admissible(level, i, j);
        w[c] = adm[c] ? node_weight(level, i, j, receiver) : 0.0;
        if (adm[c])
            ++n_adm;
        sumw += w[c];
    }
    ASSERT(n_adm > 0, "descended into a cell with no admissible children");
    for (int c = 0; c < 4; ++c) {
        if (!adm[c])
            p[c] = 0.0;
        else if (sumw > 0.0)
            p[c] = (1.0 - beta) * w[c] / sumw + beta / n_adm;
        else
            p[c] = 1.0 / n_adm;
    }
}

DescentSample DescentSampler::sample(ks::RNG &rng, const Receiver *receiver) const
{
    int L = pyramid.n_levels - 1;
    int i = 0, j = 0;
    double P = 1.0;
    for (int level = L; level >= 1; --level) {
        double p[4];
        child_probs(level - 1, i, j, receiver, p);
        double xi = rng.next();
        int c = -1;
        double cum = 0.0;
        for (int k = 0; k < 4; ++k) {
            cum += p[k];
            if (xi < cum) {
                c = k;
                break;
            }
        }
        if (c < 0 || p[c] == 0.0) { // xi beyond cum by rounding: last admissible
            for (int k = 3; k >= 0; --k)
                if (p[k] > 0.0) {
                    c = k;
                    break;
                }
        }
        P *= p[c];
        i = 2 * i + (c & 1);
        j = 2 * j + (c >> 1);
    }

    // Uniform inside leaf ∩ domain by rejection. The expected try count is
    // (leaf area) / (clipped area); the descent picks a sliver with
    // probability roughly proportional to its clipped area, so the products
    // stay bounded in expectation. The offsets are float, so
    // u = (i + xi) / n_leaf re-quantizes to leaf i exactly (i + xi is exact
    // in double, and dividing by a power of two is exact) — the query-side
    // pdf walks the identical path.
    int n = pyramid.n_leaf;
    double wl = 1.0 / n;
    double A = clipped_cell_area(i * wl, j * wl, wl);
    double u, v;
    do {
        u = (i + (double)rng.next()) * wl;
        v = (j + (double)rng.next()) * wl;
    } while (u + v > 1.0);

    DescentSample out;
    out.u = u;
    out.v = v;
    PointwiseFields f = pointwise_fields(*tri, field, u, v);
    out.p = tri->P(u, v) + f.h * normal_frame_at(*tri, u, v).N;
    out.n = f.n.normalized();
    out.sqrt_det = f.sqrt_det;
    out.pdf_uv = P / A;
    out.pdf_area = out.pdf_uv / f.sqrt_det;
    return out;
}

double DescentSampler::leaf_prob(int i, int j, const Receiver *receiver) const
{
    int L = pyramid.n_levels - 1;
    double P = 1.0;
    for (int level = L; level >= 1; --level) {
        int pi = i >> level, pj = j >> level;             // parent at `level`
        int ci = i >> (level - 1), cj = j >> (level - 1); // child at level - 1
        double p[4];
        child_probs(level - 1, pi, pj, receiver, p);
        int c = (ci - 2 * pi) | ((cj - 2 * pj) << 1);
        P *= p[c];
        if (P == 0.0)
            return 0.0;
    }
    return P;
}

double DescentSampler::pdf_uv(double u, double v, const Receiver *receiver) const
{
    int n = pyramid.n_leaf;
    int i = std::min((int)(u * n), n - 1);
    int j = std::min((int)(v * n), n - 1);
    double P = leaf_prob(i, j, receiver);
    if (P == 0.0)
        return 0.0;
    double wl = 1.0 / n;
    return P / clipped_cell_area(i * wl, j * wl, wl);
}

double DescentSampler::pdf_area(double u, double v, const Receiver *receiver) const
{
    double p = pdf_uv(u, v, receiver);
    if (p == 0.0)
        return 0.0;
    return p / pointwise_fields(*tri, field, u, v).sqrt_det;
}

TexelTableSampler::TexelTableSampler(const BaseTriangle &tri, const HeightGrid &field, const TextureGrid &emission,
                                     bool with_metric)
    : tri(&tri), field(field), emission(&emission), n(emission.W)
{
    ASSERT(emission.W == emission.H, "table sampler needs a square emission grid");
    double wl = 1.0 / n;
    std::vector<float> w((size_t)n * n);
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            double clip_frac = clipped_cell_area(i * wl, j * wl, wl) / (wl * wl);
            double f = emission.values[(size_t)j * n + i] * clip_frac;
            if (with_metric) {
                double uc = (i + 0.5) * wl, vc = (j + 0.5) * wl;
                f *= pointwise_fields(tri, field, uc, vc).sqrt_det;
            }
            w[(size_t)j * n + i] = (float)f;
        }
    table = ks::DistribTable2D(w.data(), n, n);
}

TableSample TexelTableSampler::sample(ks::RNG &rng) const
{
    float pdf_f;
    ks::vec2 s = table.sample_linear(ks::vec2(rng.next(), rng.next()), pdf_f);
    TableSample out;
    out.u = s[0];
    out.v = s[1];
    out.pdf_uv = pdf_f;
    out.in_domain = (out.u + out.v <= 1.0);
    if (out.in_domain) {
        PointwiseFields f = pointwise_fields(*tri, field, out.u, out.v);
        out.p = tri->P(out.u, out.v) + f.h * normal_frame_at(*tri, out.u, out.v).N;
        out.n = f.n.normalized();
        out.sqrt_det = f.sqrt_det;
        out.pdf_area = out.pdf_uv / f.sqrt_det;
    } else {
        out.sqrt_det = 0.0;
        out.pdf_area = 0.0;
    }
    return out;
}

double TexelTableSampler::pdf_uv(double u, double v) const { return table.pdf(ks::vec2((float)u, (float)v)); }

double TexelTableSampler::pdf_area(double u, double v) const
{
    return pdf_uv(u, v) / pointwise_fields(*tri, field, u, v).sqrt_det;
}

} // namespace dmap
