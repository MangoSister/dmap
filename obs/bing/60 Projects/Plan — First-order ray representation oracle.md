---
title: Plan — First-order ray representation oracle
tags: [plan, displacement-mapping, ray-tracing, first-order, minmax-mipmap, oracle, DDA]
status: completed-negative
created: 2026-08-24
updated: 2026-08-24
parent: "[[Project — Conservative first-order queries on displacement maps]]"
predecessor: "[[Plan — P-D2 residual clipping ablation]]"
implementation-status: full-oracle-complete-negative
---

# Plan — First-order ray representation oracle

> [!abstract] Purpose
> P-D2.0c rejected the current single-plane first-order residual clip as a broad coherent-ray accelerator, but it did not reject the certified piecewise shell-ray plus hierarchical DDA architecture or every possible first-order node representation. Before area sampling or any GPU port, this plan asks an upper-bound question: **is first-order information fundamentally unable to remove enough traversal work, or did the current hierarchy store the wrong single plane?** No implementation begins until the oracle variants, corpus extensions, metrics, and gates below are reviewed and frozen.

## 1. Decisions preserved from P-D2.0c

Authoritative run `p-d2-0c-47849fef3202` contains 48 exact-audited hierarchies, 864 cases, and 5,184 variant traversals. Every grouped root, closest root, closed owner set, constructed target, hierarchy enclosure/hash, propagated interval, fallback, and deterministic-replay check passes.

On the primary coherent cohort:

| Candidate | Node ratio to `MM-clip` | Leaf-cubic ratio | Decision |
|---|---:|---:|---|
| `FO-CW-clip` | `0.9966` | `0.9674` | failed |
| `HYB-clip` | `0.9906` | `0.9622` | failed |

Both candidates fail the predeclared `0.85` leaf gate and have negative overhead budgets at leaf/node cost ratios 4 and 8. This remains the authoritative result for the current representation. The oracle study does not reinterpret it, authorize a GPU port, or permit a speed claim.

Area-sampling work is deferred by user direction while this bounded representation audit is planned and executed. Closest point and other third applications remain deferred.

## 2. Separate the architectural and representation questions

Two causal comparisons are required:

1. `Ogaki/TFDM/etc. -> certified piecewise shell ray + MM-DDA`: tests the ray architecture, including shell partitioning, certified tube/event supercover, ordered hierarchy traversal, and exact cubic leaves.
2. `MM-DDA -> FO-DDA` with identical surface, tube, hierarchy topology, ordering policy, and leaves: tests the incremental representation value of first-order node data.

These comparisons are isolatable as ablations but are not additively separable runtime terms. A different predicate changes the surviving parameter interval, descendants, memory accesses, ordering, divergence, and leaf calls. Therefore:

- hold the traversal algorithm and topology fixed;
- allow the visited path to change, because that change is the representation's intended effect;
- report the complete traversal for each variant rather than timing a predicate in isolation; and
- never add the two measured speedups as if they were independent.

P-D2 used a shared precomputed UV-tube event supercover and identical quadtree topology. Each predicate intersected the same event interval with its inherited interval, but rejection/contraction changed which descendants were offered. This is a causal CPU operation ablation, not a production incremental GPU-DDA timing result.

## 3. The zero-order baseline is a min/max mipmap

The primary zero-order baseline is not one global min/max range. Every hierarchy node stores outward-rounded `h_min,h_max`; parents fold the four child intervals bottom-up. Event level zero addresses the root and later levels address progressively finer nodes through the full `64x64` leaf grid. `MM-clip` clips the current ray-parameter interval against the node's scalar height band at every level and propagates the contracted interval to children.

Thus `MM-clip` is a hierarchical min/max mipmap with interval clipping. `MM-test` is the weaker ablation that performs only overlap rejection and retains the input interval. `HYB-clip` intersects `MM-clip` with the first-order residual clip.

This is a strong internal zero-order hierarchy, but it is not yet a complete TFDM implementation: it has no TFDM world-space affine AABB construction, GPU texture/fetch policy, full-map layout, OptiX integration, or elapsed timing.

## 4. Current first-order node and exact slopes used

For a node centered at `x_c=(u_c,v_c)`, the stored strip is

$$
h_0 + \mathbf g^T(\mathbf x-\mathbf x_c)-r
\le h(\mathbf x) \le
h_0 + \mathbf g^T(\mathbf x-\mathbf x_c)+r.
$$

The slope is node-local, fixed at build time, and independent of the ray.

At a leaf cell:

1. split the four height samples by the frozen diagonal into two affine microtriangles;
2. solve each microtriangle's exact UV height gradient `g_0,g_1`;
3. store `g=(g_0+g_1)/2`;
4. evaluate the four cell corners against that average plane; and
5. center and outward-round the exact binary64-rational residual enclosure into `h0,r`.

At every parent:

1. store the arithmetic mean of the four child slopes;
2. evaluate every child slab at its four UV corners relative to that parent slope;
3. center the exact lower/upper envelope into `h0,r`; and
4. fold scalar `h_min,h_max` independently.

On the regular quadtree, a parent slope is consequently an equal-weight average of descendant leaf gradients. It is not a Chebyshev/minimax plane, least-squares fit over all represented vertices, ray-adaptive slope, or query-aware optimum. The hierarchy also stores gradient uncertainty `rho`, but the P-D2 ray predicate consumes only `g,h0,r`; `rho` is not used by the current ray clip.

## 5. Curve-tube inflation: common coverage, asymmetric predicate cost

The certified nonlinear shell ray is parameterized by shell height `h`. On a segment, the height coordinate is exact and affine; the chord approximation error is in UV. Its certified componentwise tube radius is `(epsilon_u,epsilon_v)`.

Both zero-order and first-order traversals must use the same tube-supercover, so both are offered all hierarchy cells touched by the UV tube rather than only by its centerline.

The predicate consequence differs:

- scalar min/max depends only on exact shell height and `[h_min,h_max]`, so the UV radius does not explicitly widen its height band;
- a first-order plane varies with UV, so UV uncertainty becomes residual-height uncertainty

$$
E_{CW}=|g_u|\epsilon_u+|g_v|\epsilon_v,
$$

and the residual band expands from `[-r,r]` to `[-r-E_CW,r+E_CW]`.

If a future curve certificate also has a height error, that height error must widen both predicates. The current `q(h)` construction has no such height approximation error.

## 6. When a tilted slab can and cannot reject

For the chord centerline define

$$
F(t)=h_r(t)-\left[h_0+\mathbf g^T(\mathbf x_r(t)-\mathbf x_c)\right].
$$

A candidate can be rejected only when the certified interval of `F(t)` is disjoint from `[-r-E,r+E]`.

If a ray follows or crosses the center plane, then `F(t)` is zero or crosses zero. Even an arbitrarily thin valid slab must retain that interval. Tightening the slab can at most shorten the interval; it cannot reject the true plane crossing. Conversely, a ray parallel to the plane but offset in residual space can be rejected by a thin slab even when a broad scalar min/max range accepts it.

Therefore first order needs both a tight surface slab and **ray/slab separation**. Static residual tightness alone does not guarantee traversal savings.

## 7. Exact P-D2.0c corpus

The corpus is

$$
12\ \text{assets}\times4\ \text{windows}\times3\ \text{shells}\times6\ \text{rays}=864\ \text{cases}.
$$

Each window contains `65x65` samples and a `64x64` microcell hierarchy. Source maps are 1K, 2K, or 4K and include both 8-bit and 16-bit inputs.

Content regimes:

- mixed frequency: Rock 05 at 1K/2K/4K, Rocky Terrain 02, Coast Sand Rocks 02, Dirt;
- structured joints: Cobblestone Floor 08, Brick Wall 001, Concrete Layers;
- fine grain: Leather Red 02;
- smooth coherent: Sand 01; and
- difficult high frequency: Rocky Trail 02.

The shell families use one unit right triangle with identity UVs:

- `S0-affine`: identical displacement directions;
- `S1-moderate`: moderate direction divergence; and
- `S2-stress`: stronger but still injective direction divergence.

The six deterministic rays are `front-hit`, `oblique-hit`, `grazing-hit`, `grid-corner-hit`, `proxy-edge-hit`, and `near-miss`. There is one fixed ray of each family per window/shell, not a sampled rendering distribution.

## 8. Observed regimes and individual wins

Hybrid ratios to `MM-clip` by ray family are:

| Ray family | Node ratio | Leaf ratio |
|---|---:|---:|
| front hit | `1.0000` | `1.0000` |
| grid-corner hit | `0.9988` | `0.9931` |
| proxy-edge hit | `0.9990` | `0.9295` |
| oblique hit | `0.9604` | `0.8590` |
| near miss | `0.9049` | `0.8091` |
| grazing hit | `0.8333` | `0.6845` |

Representative individual wins include:

- `PH-rock05-1k-W1:S0-affine:grazing-hit`: `53 -> 27` nodes and `26 -> 4` leaf cubics;
- `PH-coastsandrocks02-1k-W2:S0-affine:grazing-hit`: `22 -> 16` nodes and `12 -> 2` cubics;
- `PH-brick001-1k-W0:S0-affine:near-miss`: `17 -> 9` nodes and `6 -> 2` cubics;
- `PH-leather02-1k-W1:S0-affine:oblique-hit`: unchanged `13` nodes but `6 -> 2` cubics; and
- several Coast Sand Rocks and Rocky Trail oblique cases: `8 -> 7` nodes and `4 -> 2` cubics.

The four conditions associated with a useful first-order win are:

1. scalar min/max leaves multiple removable candidates;
2. most node height variation is explained by an affine slope;
3. the ray residual separates from the tilted slab; and
4. certified UV-tube inflation is small enough not to erase the residual advantage.

Smoothness alone is insufficient: Sand 01 produced no leaf reduction in this corpus because its tested min/max paths already left little removable work.

## 9. Important untested regimes

P-D2.0c did not test:

- complete 1K-4K maps with additional coarse hierarchy levels;
- statistical ray distributions or complete camera frames;
- shadow any-hit, closest-hit early termination, or secondary/path rays;
- workloads dominated by misses, silhouettes, or long grazing intervals;
- multiple proxy triangles, adjacency, seams, or tiled UVs;
- distorted UV maps, skinny triangles, or non-identity parameterizations;
- displacement-amplitude, signed-range, and shell-thickness sweeps;
- proxy-direction divergence beyond the three fixed shells;
- animated maps, updates, or deforming proxies;
- GPU payload, cache, occupancy, divergence, or incremental DDA cost;
- optimized, query-adaptive, or multiple first-order planes; and
- complete external TFDM/Ogaki/RMIP/PDM/DMM comparisons.

Earlier procedural experiments included ramps, smooth fields, noise, impulses, and checkerboards. They are mechanism evidence, not part of the frozen P-D2.0c full-corpus gate: ramps were favorable and checker/high-frequency cases were generally unfavorable.

## 10. Candidate representation improvements

### 10.1 Optimal single ray-independent plane

Replace arithmetic gradient averaging by a conservative Chebyshev plane. For represented vertices or affine-leaf extrema `x_i,h_i`, solve

$$
\min_{c,\mathbf g,\rho}\rho
$$

subject to

$$
-\rho\le h_i-c-\mathbf g^T(\mathbf x_i-\mathbf x_c)\le\rho.
$$

This tests whether the present fold, rather than first-order information itself, causes the weak result.

### 10.2 Tube-aware optimal slope oracle

For one node and one ray tube, solve the convex piecewise-linear objective

$$
\min_{\mathbf s}
\left[
\max_i(h_i-\mathbf s^T\mathbf x_i)-
\min_i(h_i-\mathbf s^T\mathbf x_i)
\right]
+2(|s_u|\epsilon_u+|s_v|\epsilon_v).
$$

This is query dependent and therefore an unattainable storage oracle. It establishes the maximum plausible value of any single first-order strip after accounting for UV-tube inflation.

### 10.3 Multi-slope support hierarchy

For a small slope dictionary `s_k`, store

$$
b_k^-=\min_{\Omega_N}(h-\mathbf s_k^T\mathbf x),\qquad
b_k^+=\max_{\Omega_N}(h-\mathbf s_k^T\mathbf x).
$$

Each pair is a conservative tilted strip. Their intersection is a conservative polyhedral envelope of the node graph. Zero order is the strip `s=0`; the current hybrid intersects `s=0` with one averaged-gradient strip. Test fixed and node-adaptive dictionaries with `K=2` and `K=4`, while reporting their true payload and fetch requirements.

At query time, either intersect all strips or select one/two using the predicted tube-aware width

$$
W_k=(b_k^+-b_k^-)+2(|s_{k,u}|\epsilon_u+|s_{k,v}|\epsilon_v).
$$

### 10.4 Selective two-stage evaluation

Always test scalar min/max first. Fetch/evaluate first-order data only after min/max passes and a compact node tag predicts meaningful slope-dominated contraction. This cannot improve the oracle pruning count, but it can preserve grazing/miss gains without paying first-order cost on obviously unhelpful front-facing nodes.

### 10.5 Adaptive anisotropic hierarchy, only after the plane oracle

If optimal planes are strong but square quadtree nodes mix incompatible orientations, test rectangular or slope/curvature-driven splits. This changes traversal topology and must be evaluated as a separate representation-plus-structure ablation. Do not combine it with the first oracle run.

### 10.6 Explicitly deprioritized ideas

- Repairing direct directional tube projection alone: before its numerical owner guard it saved less than `1%` coherent leaf work relative to componentwise clipping.
- GPU packing before an oracle win: the present operation margin is already insufficient at zero added cost.
- Selecting only favorable grazing examples: regime characterization must retain front, boundary, miss, smooth, and adversarial strata.

## 11. Ordered oracle ladder

Use the same P-D2 cases, tubes, event ownership, hierarchy topology, exact leaves, and root oracle initially.

1. `Z0-MM`: current hierarchical `MM-clip`.
2. `FO-CUR`: current averaged-gradient first-order strip.
3. `HYB-CUR`: current intersection of zero-order and first order.
4. `FO-OPT1`: optimal ray-independent single plane per node.
5. `HYB-OPT1`: min/max intersected with the optimal single plane.
6. `FO-TUBE-ORACLE`: per-node/per-ray tube-aware optimal slope; unattainable upper bound.
7. `FO-DICT2` and `FO-DICT4`: realizable fixed or node-selected support dictionaries.
8. `HYB-DICT2/4`: include the zero-slope min/max strip explicitly.
9. `SELECTIVE-*`: ideal and realizable selective-fetch policies over the best stored representation.

The first implementation must stop after variants 1-6 and report the upper-bound decision. Dictionary/layout work is authorized only if the upper bound is materially stronger than the current hierarchy.

## 12. Required records and metrics

Persist per node/ray:

- current and optimal slopes;
- raw height width, current residual width, optimal residual width, and tube-inflated widths;
- scalar, current-FO, optimal-FO, and oracle surviving intervals;
- node rejection, contraction, descendants, leaves, and exact cubics;
- whether min/max had removable work before applying first order;
- ray/slab residual separation and incidence;
- slope magnitude, gradient dispersion, residual energy, shell family, and ray family; and
- estimated bytes/fetches for every realizable representation.

Report pooled and p50/p95/p99/max node and leaf ratios by asset, window, content regime, shell, ray family, hierarchy level, tube radius, incidence, and min/max candidate multiplicity. Produce a regime map rather than a gallery of selected wins.

## 13. Frozen decision logic

### 13.1 Exact optimization domain

For every node, optimize and certify over all `(size+1)^2` binary64 height vertices in the node's existing rectangular UV domain. This is sufficient for the fixed piecewise-affine two-microtriangle surface because an affine residual reaches its extrema at triangle vertices. It deliberately retains samples outside the proxy-triangle footprint in boundary nodes so that the first oracle changes only the affine representation, not the existing hierarchy domain or topology.

`FO-OPT1` solves the linear program

$$
\min_{g_u,g_v,b^-,b^+} b^+-b^-
$$

subject to

$$
b^-\le h_i-g_u(u_i-u_c)-g_v(v_i-v_c)\le b^+
$$

for every node vertex. `FO-TUBE-ORACLE` adds auxiliary absolute-slope variables and minimizes

$$
(b^+-b^-)+2(|g_u|\epsilon_u+|g_v|\epsilon_v)
$$

for the current certified ray tube. Both optimizers may return binary64 slopes, but every returned strip is reconstructed and outward-rounded from exact rational evaluation of those binary64 slopes and node vertices before traversal. Optimizer success, primal feasibility, objective, and exact post-certification width are persisted. Any optimizer failure or nonfinite result is a gate failure, not a silent current-plane fallback.

### 13.2 Support-hull upper bound

Also construct `SUPPORT-HULL-ORACLE` from the convex support planes of the node's `(u,v,h)` vertices. Every numerical facet normal `(a,b,c)` is recertified by setting its upper support value to the exact binary64-rational maximum of `a u_i+b v_i+c h_i` over all node vertices and rounding outward. The shell-ray UV tube widens that halfspace by `|a|epsilon_u+|b|epsilon_v`; its height coordinate remains exact. Intersect all recertified halfspaces with the inherited interval.

This polytope contains every represented microtriangle in the node and is an unattainable upper bound for finite multi-slope strip collections over the same fixed vertex domain. Degenerate coplanar nodes use their exact certified optimal strip plus the existing UV event interval rather than a joggled three-dimensional hull. Persist facet counts and every exact enclosure audit.

### 13.3 Frozen first-run variants

The first CPU run contains exactly:

1. `MM-clip` — authoritative hierarchical min/max mip baseline;
2. `FO-CUR` — current arithmetic-average plane;
3. `HYB-CUR` — `MM-clip` intersected with `FO-CUR`;
4. `FO-OPT1` — optimal ray-independent single plane;
5. `HYB-OPT1` — `MM-clip` intersected with `FO-OPT1`;
6. `FO-TUBE-ORACLE` — query-dependent tube-aware single plane;
7. `HYB-TUBE-ORACLE` — its intersection with `MM-clip`; and
8. `SUPPORT-HULL-ORACLE` — all recertified support halfspaces.

All eight variants consume the identical P-D2 tubes, event supercover, node topology, ordering rule, and exact cubic leaf solver. Do not add slope dictionaries or selective-fetch heuristics to this run.

### 13.4 Correctness gate

Every variant must:

1. match P-D1.6 grouped roots, closest root, and closed owner sets in all 864 cases;
2. retain every constructed target;
3. keep every propagated interval inside its parent/event interval;
4. pass exact vertex/microtriangle enclosure for every optimized strip and support halfspace actually used;
5. have zero optimizer, hull, predicate, or leaf fallback;
6. reproduce the authoritative `MM-clip`, `FO-CUR`, and `HYB-CUR` counts byte-for-byte; and
7. reproduce a content-hash-selected deterministic subset byte-for-byte.

Any correctness failure blocks every usefulness decision.

### 13.5 Realizable optimized-plane gate

`HYB-OPT1` advances as a same-layout replacement only if, on the coherent cohort:

1. pooled leaf-cubic ratio to `MM-clip` is at most `0.85`;
2. pooled node-predicate ratio is at most `1.10`;
3. at least 7 of 12 source assets have strictly fewer leaf cubics;
4. no source asset has a node or leaf ratio above `1.20`; and
5. `delta_max>0` for leaf/node cost ratios 4 and 8 under the same projected 10% internal-win model as P-D2.0c.

This deliberately reuses the original P-D2 usefulness gate; changing only the build-time plane fit does not earn a weaker target.

### 13.6 Unattainable-headroom gate

Further query-adaptive or multi-slope representation work is authorized only if `SUPPORT-HULL-ORACLE` passes all correctness checks and, on the coherent cohort:

1. leaf-cubic ratio is at most `0.70`;
2. node-predicate ratio is at most `0.95` when one hull-node test is counted per offered node;
3. at least 9 of 12 assets have strictly fewer leaf cubics;
4. no asset has a node or leaf ratio above `1.10`; and
5. its operation counts provide `delta_max>=0.15` at leaf/node cost ratios 4 and 8 before facet cost is charged.

The deliberately stronger headroom requirement leaves room for realizing the oracle with multiple stored strips. Passing it does not authorize GPU work; it authorizes planning `K=2/4` dictionaries. If `HYB-TUBE-ORACLE` also approaches the hull result, prioritize query-adaptive slope selection; if only the hull passes, multiple simultaneous support planes are essential.

### 13.7 Decision tree

The frozen logic is:

1. If `HYB-OPT1` passes, retain optimized ray-independent fitting and stop before planning its production builder.
2. If `HYB-OPT1` fails but the support hull passes, use the relation between `HYB-TUBE-ORACLE` and the hull to choose query-adaptive single slopes versus a small multi-slope dictionary.
3. If the support hull fails, stop first-order ray-representation work on this topology and corpus; no choice of a finite collection of affine support strips has enough headroom.
4. If a later dictionary `K<=4` does not retain enough oracle benefit after byte/fetch accounting, stop before GPU.
5. Reopen full-resolution or GPU work only after a realizable representation passes a separately frozen workload-weighted gate with no correctness failure.

### 13.8 Frozen outputs

Write a content-addressed run containing configuration/source hashes, optimizer records, exact enclosure audits, support facets, compact traversal records, node traces, paired per-case ratios, automatic gates, and deterministic subset audits. Generate one four-panel figure showing: current versus optimal residual/tube widths; coherent work by variant; per-ray-family oracle headroom; and asset-level hull-versus-realizable gaps. Counts are CPU operation evidence, not elapsed timing.

## 14. Non-scope and stop point

This plan does not authorize:

- area sampling;
- CUDA/OptiX work;
- full-resolution replay;
- external baseline implementation;
- adaptive topology;
- Catmull-Clark surfaces; or
- a paper claim that first order is faster.

**Stop point:** the optimization domain, first-run variants, correctness construction, outputs, and go/no-go thresholds are frozen above. The next authorized implementation is the CPU oracle math plus focused analytic tests. Run a one-window anchor before the 864-case corpus and stop after the upper-bound decision.

## 15. Execution record

### 2026-08-24 — analytic implementation and anchor complete

Implemented:

- `scripts/p_d2_representation_oracle.py`: optimal ray-independent and tube-aware strip LPs, exact post-certification over binary64 vertices, recertified support-hull halfspaces, and a cached P-D2-compatible predicate;
- `scripts/run_p_d2_representation_oracle.py`: content-addressed anchor/full runner with P-D1.6 root/owner checks and byte-identical current-count checks;
- `scripts/test_p_d2_representation_oracle.py`: affine recovery, current-versus-optimal width, tube-aware slope, planar support rejection, and nonlinear hull-enclosure tests; and
- an optional predicate hook in `scripts/p_d2_hierarchy_replay.py`, with the prior P-D2 behavior unchanged.

SciPy `1.18.0` was added to the project-local Conda/OpenBLAS environment and pinned in `environment-figures.yml`. All 30 discovered P-D2 tests pass.

Authoritative anchor run: `p-d2-r0a-b27db5531171` under `experiments/p_d2_representation_oracle/`. It covers `PH-rock05-1k-W0`, 18 cases, and 144 traversals. Every exact root, closest root, closed owner set, target, interval, model enclosure, no-fallback, determinism, and previous-count reproduction check passes.

Anchor ratios to `MM-clip` are:

| Cohort/variant | Node ratio | Leaf-cubic ratio |
|---|---:|---:|
| coherent `HYB-CUR` | `0.9912` | `0.9500` |
| coherent `HYB-OPT1` | `0.9912` | `0.9500` |
| coherent support hull | `0.9912` | `0.9500` |
| all `HYB-CUR` | `0.8842` | `0.7447` |
| all `HYB-OPT1` | `0.8340` | `0.7447` |
| all support hull | `0.7683` | `0.7021` |
| boundary-stress support hull | `0.5575` | `0.5000` |

The anchor suggests that better planes can shorten downstream stress/miss traversal, while its coherent paths have no additional representational headroom even under the full support hull. This is not the frozen decision because it is the same single window whose earlier work counts were unrepresentative. Proceed unchanged to all 48 windows; do not modify variants or gates from the anchor result.

### 2026-08-24 — full corpus complete; representation branch stopped

Authoritative full run: `p-d2-r0-05a8b7b3f3c4` under `experiments/p_d2_representation_oracle/`. Derived gate report: `p-d2-r0-report-af2d251f38cb` under `experiments/p_d2_representation_report/`. The four-panel figure is `figures/p_d2_representation_oracle/representation_oracle_p-d2-r0-report-af2d251f38cb` in PNG, PDF, and SVG form.

The run rebuilt all 48 frozen windows and replayed all 864 cases through eight variants, for 6,912 traversals. Every grouped root, closest root, closed owner set, constructed target, propagated interval, exact optimized-strip/support-halfspace enclosure, no-fallback requirement, deterministic-subset check, and authoritative current-count reproduction check passes. The final discovered P-D2 regression suite passes 30/30 tests.

The plane optimizer is not vacuous. Relative to the current arithmetic-average plane, the ray-independent optimal strip has median residual-width ratio `0.6360`; the query-dependent objective has median effective tube-width ratio `0.5783`. Extreme residual ratios caused by nearly zero current widths make the mean unsuitable, so the persisted report uses robust quantiles and traversal counts for the decision.

However, the improved static representation does not produce enough ordinary coherent traversal headroom:

| Frozen gate candidate | Coherent node ratio | Coherent leaf-cubic ratio | Winning assets | `delta_max`, cost 4 | `delta_max`, cost 8 | Decision |
|---|---:|---:|---:|---:|---:|---|
| `HYB-OPT1` | `0.9841` | `0.9611` | `7/12` | `-0.1709` | `-0.2564` | fail |
| `SUPPORT-HULL-ORACLE` | `0.9818` | `0.9548` | `7/12` | `-0.1601` | `-0.2369` | fail |

`HYB-OPT1` misses the required `0.85` leaf ratio and positive break-even margins. More decisively, the recertified support hull—an unattainable zero-facet-cost upper bound for any finite collection of affine support strips over the same node domain—removes only `4.52%` of coherent leaf cubics and `1.82%` of coherent node visits. It also misses the stronger `0.70` leaf, `0.95` node, `9/12` asset, and positive-headroom gates. The strong conditional regimes remain grazing (`0.6609×` hull leaves), boundary stress (`0.7267×`), and near misses (`0.7727×`), while front hits are unchanged and grid-corner hits are `0.9914×`.

**Scientific decision:** stop first-order affine-support representation work on the fixed square min/max-mipmap topology and this corpus. Do not implement `K=2/4` slope dictionaries, selective support-strip fetching, a full-resolution replay, or a GPU port of this representation. The support-hull failure shows that choosing better single slopes is not the missing ingredient; even the best convex combination of all affine support directions lacks enough coherent work reduction before its real facet cost is charged.

This conclusion is intentionally scoped. It does not reject the certified piecewise nonlinear shell-ray plus scalar min/max-DDA architecture, which requires a separate comparison with Ogaki/TFDM. It also does not reject nonconvex piecewise bounds, adaptive/anisotropic hierarchy topology, proxy-footprint-aware node domains, or a specialized grazing/miss accelerator. Those are new hypotheses and are not authorized by this completed plan.

Safe paper-facing result language is:

> Optimizing the stored displacement plane substantially reduces median static residual width, but neither the optimized hybrid nor an idealized convex support hull materially reduces coherent traversal work on our frozen practical corpus. We therefore retain first-order clipping as a conservative formulation and negative ablation, not as a ray-acceleration claim.

---

Related: [[Plan — P-D2 residual clipping ablation]] · [[Plan — Practical first-order DDA ray traversal]] · [[Project — Conservative first-order queries on displacement maps]]
