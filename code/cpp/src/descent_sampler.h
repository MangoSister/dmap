#pragma once
#include "displaced_surface.h"
#include "ks/distrib.h"
#include "ks/rng.h"
#include "taylor_pyramid.h"
#include "texture_grid.h"
#include <vector>

// Phase S6 ("Plan — A-MVP sampling implementation"): hierarchical sample
// warping over the Taylor pyramid, plus the two non-hierarchical table
// baselines (master plan §5A, baselines 2 and 5).
//
// Domain convention: identity chart per face — the triangle
// {u >= 0, v >= 0, u + v <= 1} inside the unit tile. Pyramid cells are the
// squares of the leaf grid and their power-of-two ancestors; cells that
// straddle the hypotenuse are clipped, cells outside get probability zero.
//
// The pdf contract (master plan §5A): the density in parameter units is the
// product of the branch probabilities actually used, divided by the leaf
// cell's clipped area; the exact area pdf divides by sqrt(det G(u, v))
// evaluated pointwise at the sample. Weights only steer variance; any
// strictly positive weights on cells that hold target mass give an unbiased
// sampler.
//
// The pyramid's construction (PyramidBuild) is therefore free to change:
// the weights read h0, gu, gv and never a remainder, admissibility comes
// from the emission sum, and both pdf paths evaluate the height field. It
// is part of the sampler's identity for MIS all the same — the sample side
// and the query side must use the same construction, or the re-walked pdf
// disagrees, exactly as with beta.

namespace dmap
{

// Area of the square [u0, u0 + w] x [v0, v0 + w] clipped to u + v <= 1,
// closed form: with T(x) = max(x, 0)^2 / 2 and d = 1 - u0 - v0, the area is
// T(d) - 2 T(d - w) + T(d - 2w).
double clipped_cell_area(double u0, double v0, double w);

enum class DescentWeight
{
    AreaOnly,        // clipped area x sqrt(det G) at the node midpoint
    Product,         // emission integral over cell ∩ domain x sqrt(det G)
    ProductGeometry, // Product x receiver term at the cell's spatial centre
};

// Shading point for the receiver-aware variant. Weights are deterministic
// functions of (node, receiver) so the pdf query can re-walk them.
struct Receiver
{
    ks::vec3d x, n; // position, unit normal
};

struct DescentSample
{
    double u, v;
    ks::vec3d p;     // S(u, v)
    ks::vec3d n;     // unit surface normal
    double sqrt_det; // pointwise sqrt(det G(u, v))
    double pdf_uv;   // per unit parameter area
    double pdf_area; // per unit surface area = pdf_uv / sqrt_det
};

struct DescentSampler
{
    const BaseTriangle *tri = nullptr;
    HeightGrid field; // (n_leaf + 1)^2 node grid, shared with the pyramid
    DescentWeight variant = DescentWeight::Product;
    // Probability floor: each admissible child gets probability
    // (1 - beta) w / sum(w) + beta / n_admissible. The uniform blend is
    // scale-invariant, so the receiver term's 1/r^2 cannot drown it (the
    // absolute floor of the §5A pseudocode would). beta = 0 is exactly
    // proportional but unbiased only when the weights are positive on every
    // mass-holding child (true for Product: the emission sum certifies mass).
    double beta = 0.05;
    // ProductGeometry only: include the |emitter cosine| factor (from the
    // node's midpoint normal) in the weight. On rough content the midpoint
    // normal summarizes a coarse cell badly, so S7 measures both settings.
    bool emitter_cosine = true;
    // Construction of the bound pyramid. Fold is the default, so the S4-S8
    // measurements are reproduced unchanged; Direct gives a tighter
    // remainder and a slightly different h0, hence slightly different
    // weights (unbiased either way).
    PyramidBuild build = PyramidBuild::Fold;

    TaylorPyramid pyramid;
    // Emission sum pyramid (§5A: "a sum pyramid of E is exact at texel
    // granularity"): per cell, the integral of the piecewise-constant
    // emission over cell ∩ domain, in parameter units. Additive under
    // folding, and zero certifies zero target mass — pruning a zero cell is
    // exact, not heuristic. Indexed like the pyramid levels.
    std::vector<std::vector<double>> e_sum;

    // emission must be an n_leaf x n_leaf texel grid matching field's cells.
    DescentSampler(const BaseTriangle &tri, const HeightGrid &field, const TextureGrid &emission, DescentWeight variant,
                   double beta, PyramidBuild build = PyramidBuild::Fold);

    DescentSample sample(ks::RNG &rng, const Receiver *receiver = nullptr) const;
    // Query-side pdf for MIS: re-walks the branch probabilities of the leaf
    // containing (u, v). Same arithmetic as sample(), so the two agree
    // exactly on sampled points.
    double pdf_uv(double u, double v, const Receiver *receiver = nullptr) const;
    double pdf_area(double u, double v, const Receiver *receiver = nullptr) const;

    // Probability of descending to leaf (i, j): the discrete path product.
    // pdf_uv = leaf_prob / clipped leaf area. Exposed for validation.
    double leaf_prob(int i, int j, const Receiver *receiver = nullptr) const;

    // One node's descent weight (deterministic in (node, receiver)).
    double node_weight(int level, int i, int j, const Receiver *receiver) const;

  private:
    bool admissible(int level, int i, int j) const;
    // Probabilities of the four children (at level `level`) of parent cell
    // (pi, pj). Child c covers (2 pi + (c & 1), 2 pj + (c >> 1)).
    void child_probs(int level, int pi, int pj, const Receiver *receiver, double p[4]) const;
};

// Non-hierarchical texel-table baselines sharing the S5 machinery
// (DistribTable2D): per-texel weights E_t (emission-only, §5A baseline 2)
// or E_t x sqrt(det G) at the texel centre (product table, §5A baseline 5),
// both times the texel's clipped fraction. A sample is uniform inside the
// chosen texel, so on a hypotenuse texel it can land outside the domain;
// such samples carry in_domain = false and contribute zero (the pdf is a
// valid density over the tile, so this wastes samples, never biases).
struct TableSample
{
    double u, v;
    bool in_domain;
    ks::vec3d p, n; // set only when in_domain
    double sqrt_det;
    double pdf_uv; // per unit parameter area over the tile
    double pdf_area;
};

struct TexelTableSampler
{
    const BaseTriangle *tri = nullptr;
    HeightGrid field;
    const TextureGrid *emission = nullptr;
    int n = 0; // texels per side
    ks::DistribTable2D table;

    TexelTableSampler(const BaseTriangle &tri, const HeightGrid &field, const TextureGrid &emission, bool with_metric);

    TableSample sample(ks::RNG &rng) const;
    double pdf_uv(double u, double v) const;
    double pdf_area(double u, double v) const;
};

} // namespace dmap
