#pragma once
#include "displaced_surface.h"
#include "emission_tile.h"
#include "ks/distrib.h"
#include "ks/rng.h"
#include "taylor_pyramid.h"
#include "uv_clip.h"
#include <cstdint>
#include <vector>

// Phase S6 ("Plan — A-MVP sampling implementation"): hierarchical sample
// warping over the Taylor pyramid, plus the two non-hierarchical table
// baselines (master plan §5A, baselines 2 and 5), generalized to any chart
// ("Plan — Path tracer with displaced surfaces", D5).
//
// Domain: the triangle's texture-space domain (BaseTriangle::t0..t2) over
// the tile's quadtree. Cells are the squares of the leaf grid and their
// power-of-two ancestors, indexed globally so a domain may span several
// repeats of the tile; the tile data wrap. Cells that straddle the domain
// are clipped, cells outside get probability zero.
//
// The pdf contract (master plan §5A): the density in parameter units is the
// product of the branch probabilities actually used, divided by the leaf
// cell's clipped area; the exact area pdf divides by sqrt(det G(u, v))
// evaluated pointwise at the sample. Weights only steer variance; any
// strictly positive weights on cells that hold target mass give an unbiased
// sampler.
//
// Shared data. The pyramid and the emission tile belong to the displacement
// asset and are shared by every triangle that uses it; the sampler holds
// pointers. What is the triangle's own is its footprint: the roots of its
// descent and the tree of straddling cells below them, each carrying the
// exact clipped area and the exact clipped emission mass, computed once at
// construction with one clip per straddling leaf and additive folds above.
// Inside cells read the tile, outside cells are never visited, so the
// per-sample cost of the triangle's shape is a lookup, and the clipped
// quantities are exact everywhere: the leaf density, the admissibility
// (mass > 0 certifies mass, exactly), and the weights.
//
// The pyramid's construction (PyramidBuild) is part of the sampler's
// identity for MIS: the sample side and the query side must use the same
// pyramid, or the re-walked pdf disagrees, exactly as with beta.

namespace dmap
{

enum class DescentWeight
{
    AreaOnly,        // clipped area x sqrt(det G) at the node midpoint
    Product,         // emission mass over cell ∩ domain x sqrt(det G)
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
    double u, v;     // texture coordinates
    ks::vec3d p;     // S(u, v)
    ks::vec3d n;     // unit surface normal
    double sqrt_det; // pointwise sqrt(det G(u, v))
    double pdf_uv;   // per unit parameter area
    double pdf_area; // per unit surface area = pdf_uv / sqrt_det
};

// One cell of a footprint as seen from its parent.
struct FootprintChild
{
    Overlap overlap = Overlap::Outside;
    double area = 0.0; // |cell ∩ domain|, exact
    double mass = 0.0; // integral of the emission over cell ∩ domain, exact
    int32_t node = -1; // the cell's own FootprintNode when it straddles and is not a leaf
};

// Child c of a cell (i, j) covers (2i + (c & 1), 2j + (c >> 1)).
struct FootprintNode
{
    FootprintChild child[4];
};

struct Footprint
{
    UvTriangle domain;
    // The roots: a 2 x 2 block of cells at root_level whose union covers
    // the domain's bounding box (TFDM's find_roots), addressed like the
    // children of a virtual parent at (root_i0, root_j0).
    int root_level = 0;
    int64_t root_i0 = 0, root_j0 = 0;
    FootprintChild roots[4];
    std::vector<FootprintNode> nodes;
    double total_area = 0.0, total_mass = 0.0;
};

// The root block of a domain on a lattice of n_leaf cells per tile (TFDM's
// find_roots): the level and the lowest cell of the 2 x 2 block.
void footprint_roots(const UvTriangle &domain, int n_leaf, int &level, int64_t &i0, int64_t &j0);

Footprint build_footprint(const BaseTriangle &tri, const EmissionTile &emission);

struct DescentSampler
{
    const BaseTriangle *tri = nullptr;
    HeightGrid field; // the tile's node grid, shared with the pyramid
    const TaylorPyramid *pyramid = nullptr;
    const EmissionTile *emission = nullptr;
    DescentWeight variant = DescentWeight::Product;
    // Probability floor: each admissible child gets probability
    // (1 - beta) w / sum(w) + beta / n_admissible. The uniform blend is
    // scale-invariant, so the receiver term's 1/r^2 cannot drown it (the
    // absolute floor of the §5A pseudocode would). beta = 0 is exactly
    // proportional but unbiased only when the weights are positive on every
    // mass-holding child (true for Product: the clipped mass certifies mass).
    double beta = 0.05;
    // ProductGeometry only: include the |emitter cosine| factor (from the
    // node's midpoint normal) in the weight. On rough content the midpoint
    // normal summarizes a coarse cell badly, so S7 measures both settings.
    bool emitter_cosine = true;
    Footprint footprint;

    DescentSampler(const BaseTriangle &tri, const HeightGrid &field, const TaylorPyramid &pyramid,
                   const EmissionTile &emission, DescentWeight variant, double beta);

    // The descent draws from rng. The point inside the leaf is drawn from
    // u_leaf when given (the render sampler's 2D draw, path tracer plan D7)
    // and from rng otherwise; a rejected first try continues with rng.
    DescentSample sample(ks::RNG &rng, const Receiver *receiver = nullptr, const ks::vec2d *u_leaf = nullptr) const;
    // Query-side pdf for MIS: re-walks the branch probabilities of the leaf
    // containing (u, v). Same arithmetic as sample(), so the two agree
    // exactly on sampled points.
    double pdf_uv(double u, double v, const Receiver *receiver = nullptr) const;
    double pdf_area(double u, double v, const Receiver *receiver = nullptr) const;

    // Probability of descending to leaf (i, j): the discrete path product,
    // with the leaf's footprint record. pdf_uv = leaf_prob / clipped leaf
    // area. Exposed for validation.
    double leaf_prob(int64_t i, int64_t j, const Receiver *receiver, FootprintChild *leaf = nullptr) const;
    FootprintChild leaf_info(int64_t i, int64_t j) const;

    // Bytes the triangle owns: its footprint (the shared pyramid and
    // emission tile are not counted).
    size_t memory_bytes() const;

    // One node's descent weight (deterministic in (node, receiver)).
    double node_weight(int level, int64_t i, int64_t j, const FootprintChild &info, const Receiver *receiver) const;

  private:
    bool admissible(const FootprintChild &info) const;
    // The footprint record of child c of cell (pi, pj), whose own record is
    // parent: a straddling parent's tree entry, an inside parent's tile data.
    FootprintChild child_info(int level, int64_t pi, int64_t pj, const FootprintChild &parent, int c) const;
    // Probabilities of the four children (at level `level`) of parent cell
    // (pi, pj); level == root_level + 1 means the virtual parent of the roots.
    void child_probs(int level, int64_t pi, int64_t pj, const FootprintChild &parent, const Receiver *receiver,
                     double p[4], FootprintChild info[4]) const;
};

// Non-hierarchical texel-table baselines sharing the S5 machinery
// (DistribTable2D): per-texel weights E_t (emission-only, §5A baseline 2)
// or E_t x sqrt(det G) at the texel centre (product table, §5A baseline 5),
// both times the texel's clipped fraction, over the texel bounding box of
// the domain. A sample is uniform inside the chosen texel, so on a
// straddling texel it can land outside the domain; such samples carry
// in_domain = false and contribute zero (the pdf is a valid density over
// the box, so this wastes samples, never biases).
struct TableSample
{
    double u, v;
    bool in_domain;
    ks::vec3d p, n; // set only when in_domain
    double sqrt_det;
    double pdf_uv; // per unit parameter area over the box
    double pdf_area;
};

struct TexelTableSampler
{
    const BaseTriangle *tri = nullptr;
    HeightGrid field;
    const EmissionTile *emission = nullptr;
    UvTriangle domain;
    int64_t i0 = 0, j0 = 0; // texel box origin
    int ni = 0, nj = 0;     // texel box size
    ks::DistribTable2D table;

    double total_mass = 0.0; // integral of the emission over the domain, parameter units

    TexelTableSampler(const BaseTriangle &tri, const HeightGrid &field, const EmissionTile &emission, bool with_metric);

    // The texel from rng, the point inside it from u_point when given.
    TableSample sample(ks::RNG &rng, const ks::vec2d *u_point = nullptr) const;
    double pdf_uv(double u, double v) const;
    double pdf_area(double u, double v) const;
    // Bytes the triangle owns: the table over its texel box.
    size_t memory_bytes() const;
};

} // namespace dmap
