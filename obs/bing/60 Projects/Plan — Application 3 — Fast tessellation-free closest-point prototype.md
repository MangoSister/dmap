---
title: Plan — Application 3 — Fast tessellation-free closest-point prototype
tags: [plan, application-3, closest-point, displacement-maps, tessellation-free, taylor-hierarchy, pwos]
status: ready-for-implementation
created: 2026-09-08
updated: 2026-09-08
parent: "[[Project — Conservative first-order queries on displacement maps]]"
related: "[[Related Work — Application 3 — Tessellation-free closest-point queries]]"
---

# Plan — Application 3 — Fast tessellation-free closest-point prototype

## 1. Purpose

The first prototype should answer one narrow question:

> Does the shared first-order displacement hierarchy reduce the work of global closest-point queries enough to justify its additional channels and arithmetic?

It should separately measure:

1. the benefit of a hierarchy over exhaustive displacement-cell search; and
2. the benefit of first-order data over a conventional scalar min/max hierarchy.

Do not begin with a complete Projected Walk on Spheres implementation. First isolate the closest-point kernel and measure the hierarchy on both general and PWoS-like query distributions. Integrate PWoS only after the kernel is correct and shows a useful performance signal.

## 2. Surface and query

On one base triangle, the represented surface is

$$
S(u,v)=P(u,v)+h(u,v)N(u,v),
$$

where $P$ is the affine base triangle, $h$ is the bilinear displacement interpolant, and $N$ is the normalized interpolated displacement direction. For a world-space query point $q$, compute

$$
d^*=\min_{(u,v)\in\mathcal D}\|S(u,v)-q\|,
$$

and return a feasible point $y=S(u^*,v^*)$, its parameters, base-face owner, normal, distance $U=\|y-q\|$, and the final lower/upper gap.

The method is tessellation-free:

- no displacement-resolution triangle mesh is created;
- no microtriangle BVH is built;
- no triangle proxy is used by the leaf solver; and
- every returned point is evaluated directly with $S(u,v)$.

Recursive splitting means subdivision of parameter rectangles for numerical bounds. It does not create triangles or a polygonal surface approximation.

## 3. Smallest decisive experiment

Begin with one planar base triangle and a constant unit displacement direction $N=n$. This retains the bilinear displacement surface and texel-boundary derivative creases, while making the world-space first-order enclosure simple and exact.

For hierarchy node $k$, let $D_k$ be its texture-space rectangle clipped to the base triangle. The stored Taylor data satisfy

$$
h(u,v)=h_{0,k}+g_k^T\xi+e_k(\xi),
\qquad |e_k(\xi)|\le r_k.
$$

The affine surface predictor is

$$
A_k(\xi)=P(\xi)+(h_{0,k}+g_k^T\xi)n.
$$

Its image $A_k(D_k)$ is a finite planar convex polygon. A conservative first-order distance bound is

$$
L_{\mathrm{FO}}(q,k)
=\max\!\left(0,\operatorname{dist}(q,A_k(D_k))-r_k\right).
$$

The scalar min/max channel gives the enclosure

$$
C_{\mathrm{MM},k}
=\{P(\xi)+\alpha n:\xi\in D_k,\ \alpha\in[h_{\min,k},h_{\max,k}]\}.
$$

Test two scalar lower bounds:

- **MM-box:** distance to a conservative world-space AABB of $C_{\mathrm{MM},k}$;
- **MM-prism:** distance to the convex prism $C_{\mathrm{MM},k}$.

MM-box is the cheap practical baseline. MM-prism prevents the comparison from crediting first order merely because an avoidably loose AABB was used. The combined bound is

$$
L_{\mathrm{hybrid}}(q,k)=
\max(L_{\mathrm{MM}}(q,k),L_{\mathrm{FO}}(q,k)).
$$

A larger valid lower bound is better because it rejects more nodes.

Do not compute point-to-node distance from `NodeBound::box` in its current tangent coordinates. That frame is useful for ray parameterization but is not generally orthonormal, so coordinate distance there is not world-space Euclidean distance. Construct closest-point enclosures directly in world space.

## 4. Stage A: bound opportunity study

This stage isolates bound quality from traversal order, upper-bound seeding, and leaf-solver cost.

For each query point:

1. Run the slow continuous-surface reference in Section 5 to obtain a high-accuracy feasible upper bound $U_{\mathrm{ref}}$.
2. Traverse the same quadtree with MM-box, MM-prism, FO, and hybrid.
3. Hold $U_{\mathrm{ref}}$ fixed for every method.
4. Reject node $k$ when $L(q,k)>U_{\mathrm{ref}}$ under an outward numerical tolerance.
5. Record nodes tested, nodes rejected at each level, and leaf texels that survive.

This is a near-oracle pruning experiment: it estimates the least work each bound could permit if the query already had an excellent feasible point. If FO barely reduces the surviving leaves here, upper-bound heuristics or a different queue cannot create a strong result later.

Compare five policies with identical domains and traversal:

| Policy | Hierarchy data | Purpose |
|---|---|---|
| exhaustive | none | cost without hierarchical pruning |
| MM-box | $h_{\min},h_{\max}$ | cheap scalar hierarchy |
| MM-prism | $h_{\min},h_{\max}$ | strong scalar hierarchy |
| FO | $h_0,g_u,g_v,r$ | isolated first-order effect |
| hybrid | MM and FO | full shared representation |

### Node-level audit

On a smaller deterministic corpus, also solve the reference problem restricted to individual hierarchy nodes, obtaining $d_k$. Check

$$
L(q,k)\le d_k+\epsilon_{\mathrm{round}}
$$

for every audited method and node. Report the normalized slack $d_k-L(q,k)$ and which bound uniquely rejects the node. This is a numerical audit of the derivation, not the final proof.

## 5. Slow tessellation-free reference

The reference searches every displacement texel intersecting the base triangle. For each texel, it minimizes

$$
f(u,v)=\|S(u,v)-q\|^2
$$

directly on the clipped parameter domain.

Use parameter-space branch-and-bound:

1. Bound the image of a parameter rectangle conservatively and compute its distance lower bound.
2. Evaluate $S$ at its center, corners, and edge candidates to obtain feasible upper bounds.
3. Run local minimization from promising candidates only to tighten the upper bound.
4. Split the rectangle along its longer parameter axis when its lower bound can still improve the current solution.
5. Include edges and corners explicitly.
6. Stop when the global feasible upper bound minus the smallest remaining lower bound is below the requested tolerance.

The local optimizer never establishes correctness by itself; it only supplies feasible points. Conservative rectangle bounds establish termination. No triangles are generated.

For initial debugging, dense parameter samples and multistart local minimization may be used as a second numerical check, but they must not replace the bounded reference in recorded results.

## 6. Stage B: actual closest-point traversal

After Stage A shows a useful pruning difference, implement one best-first query shared by all hierarchy policies.

Maintain a min-priority queue keyed by node lower bound and a feasible upper bound $U$. For each query:

1. Insert all intersecting roots.
2. Evaluate cheap feasible candidates at the root centers or projected predictor points.
3. Pop the node with smallest lower bound $L$.
4. Terminate when $U-L_{\min}\le\varepsilon$.
5. Otherwise generate its children, compute their lower bounds, and discard children whose bounds cannot improve $U$.
6. At a displacement texel, minimize directly on the continuous bilinear patch. Continue on-the-fly parameter subdivision when its certificate is not yet tight enough.
7. Return the best feasible point and the remaining global gap.

The following must be identical for MM, FO, and hybrid:

- root construction;
- priority-queue implementation;
- feasible-candidate evaluations;
- leaf optimization and tolerance;
- tie handling; and
- statistics and timing method.

Only the node lower-bound function changes. This isolates the contribution of the hierarchy representation.

## 7. Query corpus

### 7.1 Procedural displacement fields

Use these first because each exposes a specific behavior:

- constant and affine ramps: $r_k$ should vanish or become very small, so FO should be strongest;
- one-dimensional sine wave;
- two-dimensional smooth wave;
- Gaussian bump;
- mixed-frequency field;
- checker-like or rough field, where the Taylor residual may erase the advantage.

Run at several map resolutions, beginning with 64 or 128 cells per side for the reference and increasing only after correctness is stable.

### 7.2 Existing displacement assets

After the procedural study, include representative smooth and rough maps already under `code/data/simple/`, such as terrain, cobble, rock, brick, leather, and sandrock. Freeze exact files, scaling, filtering, and hierarchy construction in the experiment configuration.

### 7.3 General queries

Generate deterministic groups:

- points offset along the surface normal;
- points with tangential and normal offsets;
- uniform directions around sampled surface points;
- points outside the base-triangle footprint;
- points near base edges and vertices;
- points near displacement creases; and
- points far from the surface.

### 7.4 PWoS-like queries

Without running the PDE solver, sample a surface point $x$, a direction $\omega$ uniformly on the sphere, and a radius $R$:

$$
q=x+R\omega.
$$

Sweep $R$ relative to displacement-texel size, for example $0.25$, $1$, $4$, and $16$ texels. Record results by radius. These queries approximate the projection workload generated by PWoS while keeping the closest-point experiment independent of its stochastic estimator.

## 8. Measurements

Every experiment should report both means and tail behavior:

- lower-bound violations;
- final position, parameter, owner, and distance errors;
- final certificate gap;
- node tests per query;
- nodes tested per hierarchy level;
- leaf texels entering the continuous solver;
- on-the-fly leaf subdivisions;
- evaluations of $S$, its derivatives, and bounds;
- priority-queue pushes and maximum size;
- microseconds per query, including p50, p95, and p99;
- bytes for two-channel MM, first-order data, and the full eight-channel hierarchy; and
- preprocessing time.

Record pairwise pruning events:

- MM passes and FO rejects;
- FO passes and MM rejects;
- both reject; and
- neither rejects.

These counts reveal whether the channels are complementary and whether a level-dependent policy is appropriate.

## 9. Initial plots

The first run should produce only plots that answer the hierarchy question:

1. CDF of surviving leaf texels per query for exhaustive, MM-prism, FO, and hybrid.
2. Median and p95 node tests versus PWoS query radius.
3. Per-level fractions of nodes uniquely rejected by MM and FO.
4. Bound slack $d_k-L(q,k)$ versus hierarchy level.
5. Query time versus map roughness or Taylor residual.
6. One diagnostic cross-section showing the query, displaced curve, current best radius, and the nodes retained by MM and FO.

Do not build polished paper figures before the opportunity study is positive.

## 10. Continuation decision

Proceed to the full varying-normal and multi-face surface when all of the following hold:

- no audited lower-bound violation;
- the returned distance agrees with the slow continuous reference at the requested tolerance;
- FO or hybrid reduces leaf solves substantially relative to MM-prism on smooth and sloped fields;
- the reduction remains visible on PWoS-like queries; and
- the additional bound cost produces a measurable end-to-end speedup.

A useful target is at least a $2\times$ reduction in continuous leaf solves or a 20--30% CPU time reduction on the regimes where first order is expected to help, without a large aggregate regression on rough maps. Treat these as initial decision thresholds, not paper claims.

If hybrid helps only below a particular level, test a residual- or level-gated policy: use scalar MM at coarse levels and read first-order channels only at fine levels. If FO provides little opportunity even with $U_{\mathrm{ref}}$, stop the first-order closest-point direction before implementing PWoS.

## 11. Later stages

### 11.1 Varying normals and general shells

Generalize the spatial first-order predictor and remainder to the complete surface $P+hN$. Audit it first on one triangle with smoothly varying vertex normals, then on multiple adjacent base faces. Add deterministic ownership for equal-distance solutions and report non-uniqueness at creases rather than hiding it.

### 11.2 Object-level search

Use a coarse BVH over displaced base-face enclosures, followed by the texture hierarchy within each candidate face. A BVH over the coarse proxy faces is allowed; the method forbids displacement-resolution tessellation, not acceleration over the original coarse mesh.

### 11.3 External baselines

After the tessellation-free method is stable, compare against:

- exhaustive continuous-surface search;
- the same best-first traversal with scalar min/max bounds;
- dense displacement tessellation plus a triangle closest-point BVH; and
- a practical mesh closest-point implementation when integration cost is reasonable.

Report query time together with fine-mesh construction time, memory, update cost, and instancing or texture sharing. Closest-point branch-and-bound itself is classical; the paper contribution is the conservative displacement-specific distance bound and reuse of the common hierarchy.

### 11.4 PWoS integration

Only after the closest-point kernel passes:

1. reproduce a small released-code PWoS example with its original closest-point backend;
2. log or replay its closest-point query distribution;
3. replace only that backend with the tessellation-free query;
4. keep mean-value filtering disabled in the first comparison;
5. use the same supplied or fixed valid-tube radius; and
6. compare PDE output, projection count, projection time, and total time.

Closest-point projection alone does not establish a valid PWoS tube or local-feature-size bound. Treat those as separate geometry services.

## 12. Code organization

Reuse:

- `code/cpp/src/taylor_pyramid.{h,cpp}` for all eight hierarchy channels;
- `code/cpp/src/displaced_surface.{h,cpp}` for direct evaluation of $S$ and derivatives;
- `code/cpp/src/uv_clip.{h,cpp}` for node/domain clipping;
- `code/cpp/src/displaced_tessellation.{h,cpp}` only for later mesh baselines, never in the proposed query; and
- the structure and reporting style of `code/cpp/src/test_traversal_cost.cpp`.

Add:

- `code/cpp/src/closest_point_bounds.{h,cpp}`: world-space MM, FO, and hybrid bounds;
- `code/cpp/src/closest_point_leaf.{h,cpp}`: continuous texel minimization and certificate;
- `code/cpp/src/closest_point_query.{h,cpp}`: exhaustive and best-first searches;
- `code/cpp/src/validate_closest_point.cpp`: deterministic correctness and node-bound audit;
- `code/cpp/src/test_closest_point_hierarchy.cpp`: opportunity, cost, and ablation study; and
- corresponding TOML configurations under `code/cpp/configs/`.

Keep functions small and aligned with the formulas above. Each executable should print a direct pass/fail verdict and persist enough per-query data to reproduce aggregate tables and plots.

## 13. Recommended implementation order on the second desktop

1. Build and run one existing CPU validation task without changing code.
2. Implement finite-polygon, world-AABB, and convex-prism point distances with unit tests.
3. Implement the constant-normal FO bound and node-level audit.
4. Implement the slow continuous per-texel reference.
5. Run Stage A on ramps, smooth waves, and one rough field.
6. Inspect pruning and bound-slack plots before writing the actual query.
7. If positive, implement the shared best-first traversal and Stage B timing.
8. Add existing real displacement maps.
9. Generalize to varying normals and multiple faces.
10. Integrate PWoS only after the closest-point result is independently convincing.

The first checkpoint is the Stage A table and CDF, not a renderer or PDE image.

## 14. Expected first checkpoint

The initial handoff should contain:

- the exact commit and build configuration;
- deterministic configuration files and seeds;
- zero node-bound audit failures on the small corpus;
- one table comparing exhaustive, MM-box, MM-prism, FO, and hybrid;
- CDFs of surviving leaves for general and PWoS-like queries;
- results separated by map and radius; and
- a short conclusion: proceed, revise the bound, or stop.

Related: [[Project — Conservative first-order queries on displacement maps]] · [[Related Work — Application 3 — Tessellation-free closest-point queries]]
