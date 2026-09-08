#include "descent_sampler.h"
#include "ks/assertion.h"
#include <algorithm>
#include <cmath>

namespace dmap
{

using ks::vec2d;
using ks::vec3d;

namespace
{

int64_t floor_div(double x, double w) { return (int64_t)std::floor(x / w); }

// Last cell of width w that a box ending at x overlaps: an edge exactly on
// a cell boundary does not bring in the next, empty, cell.
int64_t last_cell(double x, double w) { return (int64_t)std::ceil(x / w) - 1; }

double cell_width_at(int level, int n_leaf) { return std::ldexp(1.0, level) / n_leaf; }

vec2d cell_centre(int level, int64_t i, int64_t j, int n_leaf)
{
    double w = cell_width_at(level, n_leaf);
    return vec2d((i + 0.5) * w, (j + 0.5) * w);
}

// Recursive construction of a footprint record: the overlap class of the
// cell, and for straddling cells the exact clipped area and mass, from one
// clip per straddling leaf and additive folds above it.
FootprintChild build_child(Footprint &fp, const EmissionTile &emission, int level, int64_t i, int64_t j)
{
    int n_leaf = emission.n_leaf;
    double w = cell_width_at(level, n_leaf);
    vec2d c = cell_centre(level, i, j, n_leaf);
    FootprintChild out;
    out.overlap = classify_square(fp.domain, c, 0.5 * w);
    if (out.overlap == Overlap::Outside)
        return out;
    if (out.overlap == Overlap::Inside) {
        out.area = w * w;
        out.mass = emission.sum(level, i, j);
        return out;
    }
    if (level == 0) {
        out.area = clip_square(fp.domain, c, 0.5 * w).area();
        out.mass = emission.texel(i, j) * out.area;
        return out;
    }
    int32_t index = (int32_t)fp.nodes.size();
    fp.nodes.emplace_back();
    FootprintNode node;
    for (int ch = 0; ch < 4; ++ch) {
        node.child[ch] = build_child(fp, emission, level - 1, 2 * i + (ch & 1), 2 * j + (ch >> 1));
        out.area += node.child[ch].area;
        out.mass += node.child[ch].mass;
    }
    fp.nodes[index] = node; // after the recursion: the vector may have grown
    out.node = index;
    return out;
}

} // namespace

void footprint_roots(const UvTriangle &domain, int n_leaf, int &level, int64_t &i0, int64_t &j0)
{
    // TFDM's find_roots: the coarsest level at which the domain's bounding
    // box spans at most two cells per axis; the 2 x 2 block from the box's
    // lowest cell covers it.
    level = 0;
    while (true) {
        double w = cell_width_at(level, n_leaf);
        int64_t ilo = floor_div(domain.lo[0], w), ihi = last_cell(domain.hi[0], w);
        int64_t jlo = floor_div(domain.lo[1], w), jhi = last_cell(domain.hi[1], w);
        if (ihi - ilo <= 1 && jhi - jlo <= 1) {
            i0 = ilo;
            j0 = jlo;
            return;
        }
        ++level;
    }
}

Footprint build_footprint(const BaseTriangle &tri, const EmissionTile &emission)
{
    Footprint fp;
    fp.domain = UvTriangle(tri.t0, tri.t1, tri.t2);
    footprint_roots(fp.domain, emission.n_leaf, fp.root_level, fp.root_i0, fp.root_j0);
    for (int ch = 0; ch < 4; ++ch) {
        fp.roots[ch] = build_child(fp, emission, fp.root_level, fp.root_i0 + (ch & 1), fp.root_j0 + (ch >> 1));
        fp.total_area += fp.roots[ch].area;
        fp.total_mass += fp.roots[ch].mass;
    }
    return fp;
}

DescentSampler::DescentSampler(const BaseTriangle &tri, const HeightGrid &field, const TaylorPyramid &pyramid,
                               const EmissionTile &emission, DescentWeight variant, double beta)
    : tri(&tri), field(field), pyramid(&pyramid), emission(&emission), variant(variant), beta(beta)
{
    ASSERT(field.W == field.H && field.W == pyramid.n_leaf + 1,
           "descent sampler: node grid %d x %d does not match "
           "the pyramid's %d leaf cells",
           field.W, field.H, pyramid.n_leaf);
    ASSERT(emission.n_leaf == pyramid.n_leaf, "emission must be an n_leaf x n_leaf texel grid (got %d, n_leaf %d)",
           emission.n_leaf, pyramid.n_leaf);
    footprint = build_footprint(tri, emission);
    if (!field.repeat) {
        ASSERT(footprint.domain.lo.minCoeff() >= 0.0 && footprint.domain.hi.maxCoeff() <= 1.0,
               "a domain outside the unit tile needs a repeating height grid");
    }
}

bool DescentSampler::admissible(const FootprintChild &info) const
{
    if (info.overlap == Overlap::Outside)
        return false;
    // The clipped mass is exact, so zero certifies zero target mass and
    // pruning is exact (§5A certified-zero at texel granularity).
    return variant == DescentWeight::AreaOnly ? info.area > 0.0 : info.mass > 0.0;
}

FootprintChild DescentSampler::child_info(int level, int64_t pi, int64_t pj, const FootprintChild &parent, int c) const
{
    if (parent.node >= 0)
        return footprint.nodes[parent.node].child[c];
    if (parent.overlap != Overlap::Inside)
        return FootprintChild{}; // outside, or a straddling leaf: nothing below
    // An inside parent: every descendant is inside and reads the tile.
    int64_t i = 2 * pi + (c & 1), j = 2 * pj + (c >> 1);
    double w = pyramid->cell_width(level);
    FootprintChild out;
    out.overlap = Overlap::Inside;
    out.area = w * w;
    out.mass = emission->sum(level, i, j);
    return out;
}

double DescentSampler::node_weight(int level, int64_t i, int64_t j, const FootprintChild &info,
                                   const Receiver *receiver) const
{
    TaylorNode nd = pyramid->node(level, i, j);
    vec2d c = cell_centre(level, i, j, pyramid->n_leaf);
    double u0 = c[0], v0 = c[1];

    // Midpoint composition (§5A composedWeight): the node's Taylor plane
    // (h0, gu, gv) through the exact pointwise metric formulas.
    PointwiseFields f = pointwise_fields(*tri, u0, v0, nd.h0, nd.gu, nd.gv);

    double base = (variant == DescentWeight::AreaOnly) ? info.area : info.mass;
    double weight = base * f.sqrt_det;

    if (variant == DescentWeight::ProductGeometry && receiver) {
        // Spatial centre: midpoint height along the centre normal (§5A).
        NormalFrame nf = normal_frame_at(*tri, u0, v0);
        vec3d y = tri->P(u0, v0) + nd.h0 * nf.N;
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

void DescentSampler::child_probs(int level, int64_t pi, int64_t pj, const FootprintChild &parent,
                                 const Receiver *receiver, double p[4], FootprintChild info[4]) const
{
    double w[4];
    bool adm[4];
    int n_adm = 0;
    double sumw = 0.0;
    bool virtual_root = level > footprint.root_level;
    for (int c = 0; c < 4; ++c) {
        int child_level = level - 1;
        int64_t i, j;
        if (virtual_root) {
            info[c] = footprint.roots[c];
            i = footprint.root_i0 + (c & 1);
            j = footprint.root_j0 + (c >> 1);
        } else {
            info[c] = child_info(child_level, pi, pj, parent, c);
            i = 2 * pi + (c & 1);
            j = 2 * pj + (c >> 1);
        }
        adm[c] = admissible(info[c]);
        w[c] = adm[c] ? node_weight(child_level, i, j, info[c], receiver) : 0.0;
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

DescentSample DescentSampler::sample(ks::RNG &rng, const Receiver *receiver, const vec2d *u_leaf) const
{
    // The virtual parent of the roots sits one level above them; its
    // "children" are the roots themselves.
    int level = footprint.root_level + 1;
    int64_t i = 0, j = 0;
    FootprintChild cur;
    double P = 1.0;
    for (; level >= 1; --level) {
        double p[4];
        FootprintChild info[4];
        child_probs(level, i, j, cur, receiver, p, info);
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
        if (level > footprint.root_level) {
            i = footprint.root_i0 + (c & 1);
            j = footprint.root_j0 + (c >> 1);
        } else {
            i = 2 * i + (c & 1);
            j = 2 * j + (c >> 1);
        }
        cur = info[c];
    }

    // Uniform inside leaf ∩ domain by rejection inside the leaf square. The
    // offsets are float, so u = (i + xi) / n_leaf re-quantizes to leaf i
    // exactly (i + xi is exact in double, and dividing by a power of two is
    // exact), which is what lets the query-side pdf walk the identical
    // path. A sliver may reject many times; after a bounded number of
    // tries the clipped polygon is sampled directly instead.
    int n = pyramid->n_leaf;
    double wl = 1.0 / n;
    double u, v;
    bool inside_leaf = cur.overlap == Overlap::Inside;
    int tries = 0;
    for (;;) {
        if (u_leaf && tries == 0) {
            u = (i + (double)(float)(*u_leaf)[0]) * wl;
            v = (j + (double)(float)(*u_leaf)[1]) * wl;
        } else {
            u = (i + (double)rng.next()) * wl;
            v = (j + (double)rng.next()) * wl;
        }
        if (inside_leaf || footprint.domain.inside(vec2d(u, v)))
            break;
        if (++tries == 32) {
            ClipPolygon poly = clip_square(footprint.domain, cell_centre(0, i, j, n), 0.5 * wl);
            vec2d s = sample_polygon(poly, rng.next(), rng.next(), rng.next());
            u = s[0];
            v = s[1];
            break;
        }
    }

    DescentSample out;
    out.u = u;
    out.v = v;
    PointwiseFields f = pointwise_fields(*tri, field, u, v);
    out.p = tri->P(u, v) + f.h * normal_frame_at(*tri, u, v).N;
    out.n = f.n.normalized();
    out.sqrt_det = f.sqrt_det;
    out.pdf_uv = P / cur.area;
    out.pdf_area = out.pdf_uv / f.sqrt_det;
    return out;
}

double DescentSampler::leaf_prob(int64_t i, int64_t j, const Receiver *receiver, FootprintChild *leaf) const
{
    int L = footprint.root_level;
    // The root containing the leaf must be one of the four.
    int64_t ri = i >> L, rj = j >> L;
    int64_t di = ri - footprint.root_i0, dj = rj - footprint.root_j0;
    if (di < 0 || di > 1 || dj < 0 || dj > 1)
        return 0.0;

    FootprintChild cur;
    double P = 1.0;
    int64_t pi = 0, pj = 0;
    for (int level = L + 1; level >= 1; --level) {
        double p[4];
        FootprintChild info[4];
        child_probs(level, pi, pj, cur, receiver, p, info);
        int c;
        if (level > L) {
            c = (int)(di | (dj << 1));
            pi = ri;
            pj = rj;
        } else {
            int64_t ci = i >> (level - 1), cj = j >> (level - 1); // the leaf's ancestor at level - 1
            c = (int)((ci - 2 * pi) | ((cj - 2 * pj) << 1));
            pi = ci;
            pj = cj;
        }
        P *= p[c];
        cur = info[c];
        if (P == 0.0)
            return 0.0;
    }
    if (leaf)
        *leaf = cur;
    return P;
}

FootprintChild DescentSampler::leaf_info(int64_t i, int64_t j) const
{
    int L = footprint.root_level;
    int64_t ri = i >> L, rj = j >> L;
    int64_t di = ri - footprint.root_i0, dj = rj - footprint.root_j0;
    if (di < 0 || di > 1 || dj < 0 || dj > 1)
        return FootprintChild{};
    FootprintChild cur = footprint.roots[di | (dj << 1)];
    int64_t pi = ri, pj = rj;
    for (int level = L; level >= 1; --level) {
        int64_t ci = i >> (level - 1), cj = j >> (level - 1);
        int c = (int)((ci - 2 * pi) | ((cj - 2 * pj) << 1));
        cur = child_info(level - 1, pi, pj, cur, c);
        pi = ci;
        pj = cj;
    }
    return cur;
}

double DescentSampler::pdf_uv(double u, double v, const Receiver *receiver) const
{
    int n = pyramid->n_leaf;
    int64_t i = floor_div(u, 1.0 / n);
    int64_t j = floor_div(v, 1.0 / n);
    FootprintChild leaf;
    double P = leaf_prob(i, j, receiver, &leaf);
    if (P == 0.0)
        return 0.0;
    return P / leaf.area;
}

double DescentSampler::pdf_area(double u, double v, const Receiver *receiver) const
{
    double p = pdf_uv(u, v, receiver);
    if (p == 0.0)
        return 0.0;
    return p / pointwise_fields(*tri, field, u, v).sqrt_det;
}

TexelTableSampler::TexelTableSampler(const BaseTriangle &tri, const HeightGrid &field, const EmissionTile &emission,
                                     bool with_metric)
    : tri(&tri), field(field), emission(&emission), domain(tri.t0, tri.t1, tri.t2)
{
    int n = emission.n_leaf;
    double wl = 1.0 / n;
    // Texel box of the domain.
    i0 = floor_div(domain.lo[0], wl);
    j0 = floor_div(domain.lo[1], wl);
    ni = (int)(last_cell(domain.hi[0], wl) - i0 + 1);
    nj = (int)(last_cell(domain.hi[1], wl) - j0 + 1);
    std::vector<float> w((size_t)ni * nj);
    for (int b = 0; b < nj; ++b)
        for (int a = 0; a < ni; ++a) {
            int64_t i = i0 + a, j = j0 + b;
            vec2d c = cell_centre(0, i, j, n);
            double clip_frac = 0.0;
            switch (classify_square(domain, c, 0.5 * wl)) {
            case Overlap::Inside:
                clip_frac = 1.0;
                break;
            case Overlap::Straddle:
                clip_frac = clip_square(domain, c, 0.5 * wl).area() / (wl * wl);
                break;
            case Overlap::Outside:
                break;
            }
            double f = emission.texel(i, j) * clip_frac;
            total_mass += f * wl * wl;
            if (with_metric && f > 0.0)
                f *= pointwise_fields(tri, field, c[0], c[1]).sqrt_det;
            w[(size_t)b * ni + a] = (float)f;
        }
    table = ks::DistribTable2D(w.data(), ni, nj);
}

TableSample TexelTableSampler::sample(ks::RNG &rng, const vec2d *u_point) const
{
    float pdf_f;
    ks::vec2i cell;
    ks::vec2 s = table.sample_linear(ks::vec2(rng.next(), rng.next()), pdf_f, &cell);
    int n = emission->n_leaf;
    double wl = 1.0 / n;
    TableSample out;
    if (u_point) {
        s = ks::vec2((cell[0] + (float)(*u_point)[0]) / ni, (cell[1] + (float)(*u_point)[1]) / nj);
    }
    out.u = (i0 + (double)s[0] * ni) * wl;
    out.v = (j0 + (double)s[1] * nj) * wl;
    out.pdf_uv = pdf_f / ((double)ni * nj * wl * wl);
    out.in_domain = domain.inside(vec2d(out.u, out.v));
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

double TexelTableSampler::pdf_uv(double u, double v) const
{
    int n = emission->n_leaf;
    double x = (u * n - i0) / ni, y = (v * n - j0) / nj;
    if (x < 0.0 || x >= 1.0 || y < 0.0 || y >= 1.0)
        return 0.0;
    double wl = 1.0 / n;
    return table.pdf(ks::vec2((float)x, (float)y)) / ((double)ni * nj * wl * wl);
}

double TexelTableSampler::pdf_area(double u, double v) const
{
    double p = pdf_uv(u, v);
    if (p == 0.0)
        return 0.0;
    return p / pointwise_fields(*tri, field, u, v).sqrt_det;
}

size_t DescentSampler::memory_bytes() const
{
    return sizeof(Footprint) + footprint.nodes.size() * sizeof(FootprintNode);
}

size_t TexelTableSampler::memory_bytes() const
{
    size_t bytes = sizeof(ks::DistribTable2D) + table.margin.cdf.size() * sizeof(float);
    for (const ks::DistribTable &row : table.cond)
        bytes += sizeof(ks::DistribTable) + row.cdf.size() * sizeof(float);
    return bytes;
}

} // namespace dmap
