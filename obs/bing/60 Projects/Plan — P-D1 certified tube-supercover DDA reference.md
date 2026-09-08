---
title: Plan — P-D1 certified tube-supercover DDA reference
tags: [plan, displacement-mapping, ray-tracing, DDA, certification, CPU]
status: complete
created: 2026-08-23
updated: 2026-08-23
parent: "[[Plan — Practical first-order DDA ray traversal]]"
implementation-status: complete
---

# Plan — P-D1 certified tube-supercover DDA reference

> [!abstract] P-D1 decision
> Build one deliberately unoptimized CPU reference that proves lazy piecewise shell-ray linearization plus tube-supercover DDA cannot omit a candidate admitted by an independent recursive traversal. P-D1 is a correctness and mechanism stage. It may report work counts and interval contraction, but it may not make a CPU or GPU speed claim.

## 1. Scope and non-scope

P-D1 includes:

1. the actual triangle-proxy shell map used by the active project;
2. certified componentwise and directional chord-error bounds for its curvy shell-space ray;
3. lazy, node-local parameter subdivision with no fixed segment cap;
4. closed-set tube-supercover enumeration with explicit edge/corner ties;
5. an independent recursive cell reference;
6. exact shared leaf/root evaluation after candidate enumeration; and
7. comparison on all 48 frozen practical-map windows plus analytic edge cases.

P-D1 excludes traversal optimization, packed hierarchy layouts, CUDA/OptiX integration, external baselines, full-resolution timing, and any paper-facing performance conclusion. P-D2 owns the min/max versus first-order clipping ablation; P-D3/P-D4 own GPU cost and speed.

## 2. Exact geometric contract

For one proxy triangle, use local coordinates `(u,v)` with triangle domain

$$
\Delta=\{(u,v):u\ge0,\ v\ge0,\ u+v\le1\}.
$$

The affine base and affine displacement direction are

$$
B(u,v)=p_0+u(p_1-p_0)+v(p_2-p_0),
$$

$$
N(u,v)=n_0+u(n_1-n_0)+v(n_2-n_0).
$$

The shell map and represented displaced surface are

$$
F(u,v,h)=B(u,v)+hN(u,v),\qquad
X(u,v)=F(u,v,d(u,v)).
$$

For a world ray `R(t)=o+td`, the curvy shell ray is

$$
q(t)=(u(t),v(t),h(t))=F^{-1}(R(t)).
$$

P-D1 uses the exact rational coefficient construction for this triangle shell, with a second independent evaluation obtained by solving `F(u,v,h)=R(t)` numerically in high precision. Intervals that contain a shell singularity are split; if the denominator cannot be separated from zero, the case takes an uncapped conservative nonlinear fallback and is counted.

At hierarchy node `A`, fit/store the first-order predictor

$$
p_A(u,v)=a_A+g_{u,A}(u-u_A)+g_{v,A}(v-v_A)
$$

and outward residual interval

$$
r_A(u,v)=d(u,v)-p_A(u,v)\in[r_A^-,r_A^+].
$$

The ray-side residual coordinate is

$$
\rho_A(t)=h(t)-p_A(u(t),v(t)).
$$

This is the common first-order coordinate used later by traversal and the area application; P-D1 tests its conservative use, not its speed.

## 3. Certified node-local linearization

On parameter interval `I=[t_0,t_1]`, each rational coordinate is represented as `f(t)=P(t)/Q(t)`. Interval arithmetic evaluates

$$
f''(t)=\frac{(P''Q-PQ'')Q-2(P'Q-PQ')Q'}{Q^3}
$$

and obtains a certified bound `|f''|≤M_f` whenever `0∉Q(I)`. The secant interpolation error then obeys

$$
|f(t)-\bar f(t)|\le \epsilon_f=\frac{M_f|I|^2}{8}.
$$

Two certificates are persisted:

- **componentwise:** `(epsilon_u,epsilon_v,epsilon_h)` bounds the axis-aligned shell-space tube;
- **directional:** interval evaluation of

$$
\rho_A''=h''-g_{u,A}u''-g_{v,A}v''
$$

gives `epsilon_rho=M_rho|I|²/8` directly, without replacing it by the looser sum `epsilon_h+|g_u|epsilon_u+|g_v|epsilon_v`.

Every accepted interval is independently checked at high-precision interior samples for debugging, but samples never establish conservativeness. Outward interval arithmetic is the certificate.

## 4. Lazy segmentation contract

There is no prism-owned ray and no global `HSeg[32]` array. A ray/prism invocation owns a small traversal state `(node, I)`:

1. restrict the world-ray interval to the shell/prism;
2. at the current node and interval, form one secant and its certified tube;
3. accept it when the componentwise UV tube and directional residual error meet the node-relative certificate;
4. otherwise split `I` at its midpoint and process both halves at the same node; and
5. after spatial descent clips `I`, reconstruct the child-local chord from the two exact rational endpoint evaluations.

Thus a “reconstructed chord” is simply a new secant over a shorter child interval. A “traversal-requested knot” is the midpoint introduced only because the current node could not certify its interval. Knots are per invocation, generated lazily, and never stored in the mesh or prism.

The initial acceptance policy is frozen as both:

$$
\max(\epsilon_u/w_A,\epsilon_v/h_A)\le\eta_{uv},
$$

$$
\epsilon_\rho\le\eta_\rho\max(r_A^+-r_A^-,\tau_{abs}),
$$

with sweeps `eta_uv,eta_rho ∈ {0.25,0.5,1.0}` and a scale-aware absolute floor. This sweep changes extra candidates, never correctness. No maximum split count is allowed; reaching a numerical/resource guard invokes the conservative fallback.

## 5. Closed tube-supercover DDA

For an accepted secant over normalized parameter `s∈[0,1]`, the UV enclosure is

$$
(\bar u(s),\bar v(s))\oplus[-\epsilon_u,\epsilon_u]\times[-\epsilon_v,\epsilon_v].
$$

The CPU DDA is an event-based supercover enumerator:

1. compute all parameters at which either tube side `bar u±epsilon_u` crosses a vertical grid boundary or `bar v±epsilon_v` crosses a horizontal boundary;
2. process the sorted event intervals in increasing `s`;
3. enumerate the Cartesian product of overlapped row/column indices on each open interval;
4. at an event, apply closed ownership and include every cell touched at an edge or corner; and
5. for each cell, solve the affine inequalities against its box expanded by `(epsilon_u,epsilon_v)` to obtain the exact conservative `s` subinterval.

Candidate cells are then intersected with the closed proxy triangle. Degenerate zero-length intervals are retained. Simultaneous x/y events advance both axes; no epsilon jump is used. Duplicate visits merge by integer cell ID and interval union. This event enumerator is the mathematical CPU reference for the later incremental GPU DDA, not its final implementation layout.

## 6. Independent recursive reference

The reference recursively visits a node only when its closed UV box intersects the same certified tube and the proxy triangle. It tests all children independently and never uses DDA event generation. At every tested level, compare:

- sorted integer candidate cell IDs;
- canonical merged parameter intervals;
- admitted leaf IDs after min/max and first-order predicates; and
- sorted exact leaf roots.

Both traversals call one shared leaf equation only after their independently generated candidate sets are frozen. Root values are sorted by `(t,u,v,leaf_id)` before byte comparison so traversal order cannot hide or manufacture agreement.

## 7. Frozen inputs

### Displacement windows

Use all 48 immutable `65×65` native-sample windows from P-D0 run `p-d0-71627ba7b07f`. Preserve source code values and apply displacement amplitude as a separate shell parameter; do not normalize each crop by its percentiles.

### Shell families

Use three deterministic triangle shells:

1. `S0-affine`: identical directions `n_0=n_1=n_2`, where the shell ray is straight and one segment should suffice;
2. `S1-moderate`: moderate direction divergence with positive shell Jacobian across the tested height band; and
3. `S2-stress`: stronger but still injective direction divergence, intended to trigger lazy splits.

The exact vertices, directions, safe height intervals, determinant lower bounds, and coefficient hashes must be serialized before rays are evaluated.

### Ray families

Freeze deterministic batches for primary-like hits, oblique hits, grazing/silhouette, empty-space near misses, triangle-edge crossings, exact grid-edge/corner ties, and incoherent directions. Include analytic constructed rays whose shell curves hit a chosen leaf and separate random seeds into decision and held-out sets. Persist ray values before traversal.

## 8. Reuse and deliberate rejection of old code

Reuse:

- the triangle-shell coefficient formulas in `nrtdsm/gpu_kernels/nrtdsm_intersection_kernels.h`, ported to Python and cross-checked rather than trusted;
- the schema, run lifecycle, frozen-map adapters, and percentile summaries from the Step 1 Python infrastructure;
- ray-family patterns from `scripts/step1_ray_families.py`; and
- first-order plane/residual construction from `scripts/first_order_ray_core.py` where its assumptions match the native window contract.

Do not reuse as correctness logic:

- midpoint-only chord-error estimation;
- the old heuristic `hBow`;
- zero-width chord DDA;
- fixed 32-segment, 16-stack, or 5000-step caps;
- epsilon jumps at grid corners; or
- planar world-space microtriangles as a substitute for the declared leaf surface.

## 9. Outputs and metrics

Create a content-addressed run under `experiments/p_d1_tube_supercover/` containing immutable configuration, shell/ray manifests, per-segment certificates, candidate traces, root records, and summary. Generate figures under `figures/p_d1_tube_supercover/`.

Per ray, report p50/p95/p99/max for:

- lazy knots and accepted segments;
- componentwise versus directional certificate width;
- supercover cells and centerline-only cells;
- recursive nodes and DDA events;
- admitted min/max and first-order leaves;
- fallback count; and
- residual interval width before and after first-order affine clipping.

Correctness metrics are exact candidate omissions/extras, admitted-leaf disagreement, root-count disagreement, closest-root mismatch, and maximum high-precision residual `||F(u,v,d(u,v))-R(t)||`.

Required figures:

1. one ray/prism view showing its complete curvy shell ray, all lazy knots, and certified tubes;
2. DDA versus recursive cell coverage including edge/corner ties;
3. componentwise versus directional residual certificates on the same intervals;
4. segments/ray and cells/ray distributions by shell/ray family; and
5. first-order interval-contraction distributions as a mechanism diagnostic only.

## 10. P-D1 pass/fail rule

P-D1 passes only if:

- DDA has zero candidate omissions relative to recursive traversal on every analytic, decision, and held-out case;
- admitted leaf sets and sorted exact root records are byte-identical when both paths use the shared predicate/leaf evaluator;
- every reported root satisfies the independent high-precision world-space residual tolerance;
- all exact grid-edge/corner tie cases pass without epsilon jumps;
- no fixed segment/step/stack limit can return a miss; and
- all fallbacks are conservative, terminate, and are reported.

Candidate extras are allowed and measured. CPU time is explicitly ignored. Any omission blocks P-D2 and GPU work; it must be fixed in the mathematical contract, not hidden by widening an unrelated tolerance or dropping the ray.

## 11. Ordered implementation

1. `P-D1.0`: serialize and test the three shell families and rational/high-precision inverse agreement.
2. `P-D1.1`: implement outward interval arithmetic and second-derivative chord certificates; test against analytic affine/quadratic/rational cases.
3. `P-D1.2`: implement uncapped lazy node-local splitting and fallback accounting.
4. `P-D1.3`: implement the independent recursive closed-tube reference.
5. `P-D1.4`: implement event-based tube-supercover DDA and exhaustive edge/corner tie tests.
6. `P-D1.5`: connect the shared exact leaf/root evaluator and compare canonical records.
7. `P-D1.6`: run all 48 windows, generate the report/figures, and make the pass/fail call.

Stop and write the exact substep test cases before implementing each numbered item. After P-D1 passes, write P-D2 before changing traversal predicates or making any timing measurement.

## 12. Paper-draft checkpoint

Do not write a performance claim from P-D1. Once P-D1.1–P-D1.4 pass, the useful draft material is limited to:

- the formal shell map and residual coordinate;
- the second-derivative tube certificate;
- the lazy segmentation/supercover algorithm; and
- a theorem/lemma stating candidate completeness under the certified enclosure and closed ownership rules.

Wait for P-D2/P-D4 before writing any sentence that says first-order traversal is faster.

## 13. Frozen P-D1.0 test cases — before implementation

P-D1.0 is limited to shell-map/inverse validation. Its exact tests are:

1. construct `S0/S1/S2` on the standard unit proxy triangle with identity UVs and height band `[0,1]`;
2. expand `det[Fu,Fv,Fh]` as a tensor-product power polynomial and convert it to Bernstein form on `[0,1]^3`; require every shell's conservative Bernstein lower bound to be positive (the square in `(u,v)` conservatively contains the triangle);
3. for each shell, freeze interior, proxy-edge, vertex-adjacent, `h=0`, and `h=1` constructed points, with normalized front-facing, oblique, and grazing-like ray directions;
4. place each ray origin so the constructed point occurs at a known positive `t`, then require the rational `h`-parameter inverse to recover `(a,b,u,v,t)` and reconstruct the world point within `2e-12` in binary64;
5. independently solve `[E1(h),E2(h),-d](a,b,t)^T=o-(p0+h n0)` with 80-digit Decimal Gaussian elimination and require agreement with the rational path within `5e-12`;
6. bound the rational denominator with univariate Bernstein coefficients on `[0,1]`; require separation from zero for every frozen constructed case; and
7. persist shell coefficient hashes, determinant bounds, input cases, errors, and a pass/fail result before implementing P-D1.1.

No displacement image, hierarchy, segmentation, DDA, leaf root, or timing is touched by P-D1.0.

### P-D1.0 outcome — 2026-08-23

Run `p-d1-0-bd9124ee4b3d` passed all 21 constructed cases. The maximum binary64 parameter error, Decimal cross-check error, and world reconstruction error were each at most `4.55e-16`. All three tensor-Bernstein Jacobian lower bounds are positive and all rational denominators are separated from zero. Four unit tests pass. This authorizes P-D1.1 only.

## 14. Frozen P-D1.1 test cases — before implementation

P-D1.1 is limited to certified chord tubes; it does not split rays or visit hierarchy cells. Its exact tests are:

1. verify univariate power-to-Bernstein range conversion on constant, affine, quadratic, and quartic polynomials against extrema and dense samples on non-unit intervals;
2. implement the rational second-derivative numerator

$$
S_N=(N''D-ND'')D-2(N'D-ND')D'
$$

so `(N/D)''=S_N/D³`, and verify it against high-accuracy finite differences away from singularities;
3. require the `S0-affine` UV chord certificate to reduce to roundoff padding only;
4. on every P-D1.0 constructed ray, certify `(u,v)` over `[0,1]` and four fixed dyadic subintervals, then require 80-digit Decimal samples at 257 locations to lie inside the componentwise tube;
5. for fixed gradients `(1.3,-0.9)`, `(0.6,0.6)`, and `(-0.4,1.1)`, certify the direct directional error and require Decimal projected samples to lie inside it;
6. record the direct-directional/componentwise-projection ratio without requiring it to be below one in every interval (interval range correlation can make either bound looser); and
7. use a synthetic denominator with a root inside the interval to require an explicit `denominator-not-separated` result, never a finite certificate.

Persist per-interval denominator/numerator ranges, componentwise radii, directional radii, sampled maxima, ratios, and the gate result. No sample is allowed to establish the certificate; it only audits the outward analytic bound.

### P-D1.1 outcome — 2026-08-23

Run `p-d1-1-c6d756ee5891` passed all 105 shell-ray intervals and all 315 directional certificates. Every 80-digit Decimal audit sample was inside its analytic outward bound, and a synthetic denominator root correctly refused certification. Four unit tests pass.

The direct directional radius divided by the componentwise projected radius had minimum `0.0158`, median `0.855`, and maximum effectively `1.0`. This is early mechanism evidence that a node plane direction can retain cancellation lost by `|g_u|epsilon_u+|g_v|epsilon_v`; it is not yet evidence of fewer traversal segments or faster tracing. P-D1.2 must test whether lazy splitting converts this bound reduction into fewer knots without changing coverage.

## 15. Frozen P-D1.2 lazy-segmentation experiment — before implementation

P-D1.2 asks one question only: when the hierarchy contains actual first-order predictors from practical native windows, does the direct directional certificate reduce traversal-requested knots and reconstructed chords while retaining every constructed hit? It still uses recursive node visits, not DDA.

### Inputs

- all 48 immutable P-D0 `65×65` native windows;
- source code values divided only by the full integer code range (`255` or `65535`), with no crop-wise percentile normalization;
- the fixed-diagonal piecewise-affine height surface already used by the CPU oracle: two UV microtriangles per cell;
- exact 64×64-cell first-order hierarchies built analytically from leaf planes and conservative child folding;
- all three certified `S0/S1/S2` triangle shells from P-D1.0; and
- six deterministic rays per `(window,shell)`:
  `front-hit`, `oblique-hit`, `grazing-hit`, `grid-corner-hit`, `proxy-edge-hit`, and `near-miss`.

The five hit rays are constructed through an exact represented surface point and store that point plus every closed cell touching its UV coordinate. The near-miss passes through the shell band without being used as a required-hit case. Ray values and hierarchy hashes are persisted before comparing variants.

### Node representation

At a leaf, average the two microtriangle gradients to obtain `g_A`; bound the residual of both exact planes at all four cell corners. At an internal node, average child gradients and conservatively fold child residual slabs at all child corners. Store outward `(h0,g,r,h_min,h_max)` exactly as in the analytic Step 1 hierarchy, but on unnormalized native windows.

The hierarchy gate requires dense validation of every exact microtriangle against every ancestor slab, with zero violation above `2e-12`.

### Lazy state and acceptance

A state is `(node,[h0,h1])`. It is clipped to the node's UV box using the certified componentwise UV tube, then a new local chord is reconstructed from exact rational endpoint evaluations. For node extents `(w_A,h_A)`, accept UV linearization only if

$$
\max(\epsilon_u/w_A,\epsilon_v/h_A)\le\eta_{uv}.
$$

For residual half-width `r_A`, define the source-scale floor

$$
\tau_{abs}=\frac{2}{2^{b}-1},
$$

where `b` is the source bit depth. Accept the residual linearization only if

$$
\epsilon_{proj}\le\eta_\rho\max(2r_A,\tau_{abs}).
$$

`L-CW` uses `epsilon_proj=|g_u|epsilon_u+|g_v|epsilon_v`; `L-DIR` uses the direct certificate for `g_u u+g_v v`. Both use the same componentwise UV tube, clipping, node predictor, and residual overlap predicate.

If either acceptance inequality fails, split the local height interval at its exact floating midpoint and revisit the same node. There is no segment/depth/step cap. If no distinct midpoint exists, accept the unsplit conservative tube and record `precision-floor`; this cannot return a miss.

After acceptance, reject only when the chord residual interval widened by `epsilon_proj` is disjoint from `[-r_A,r_A]`; otherwise descend recursively. Closed UV ownership is used throughout. P-D1.3 will independently validate coverage; P-D1.2 only audits known constructed hits and analytic certificates.

### Frozen sweeps and records

Run the Cartesian sweep

$$
\eta_{uv},\eta_\rho\in\{0.25,0.5,1.0\}
$$

for both `L-CW` and `L-DIR`. Per ray/variant persist:

- node tests, local linearizations, accepted node-local chords, unique interval chords, unique knots;
- UV, residual, and combined split events;
- maximum refinement depth and precision-floor events;
- UV and height rejections, admitted leaves, and required-hit cell coverage;
- maximum sampled componentwise/directional certificate violation; and
- direct/componentwise projected-radius ratios.

Primary mechanism plots use the central policy `(0.5,0.5)` and show p50/p95/p99/max plus paired `L-DIR/L-CW` ratios. The complete sweep establishes sensitivity; it is not tuned per map or ray.

### P-D1.2 gate

Pass only if:

- all 48 native hierarchies satisfy the analytic/dense slab validation;
- both variants retain every required constructed-hit cell for every shell, window, and policy;
- sampled curve deviations never exceed their analytic certificates beyond `2e-12` audit tolerance;
- every traversal terminates without a miss-producing resource cap; and
- inputs and results regenerate deterministically.

There is no required performance win. Report honestly whether `L-DIR` reduces median/p95 knots, local linearizations, or split events. If it does not, retain directional bounds only as a tighter predicate candidate and do not claim a segmentation advantage.

### Implementation files and outputs

- `scripts/p_d1_lazy_segmentation.py` — native-window adapter, frozen rays, uncapped lazy traversal, records and summaries;
- `scripts/run_p_d1_lazy_segmentation.py` — content-addressed run under `experiments/p_d1_lazy_segmentation/`;
- `scripts/test_p_d1_lazy_segmentation.py` — hierarchy, hit-retention, certificate, precision-floor, and determinism tests; and
- figures under `figures/p_d1_lazy_segmentation/`.

P-D1.2 does not modify `nrtdsm/`, legacy Mode 1, or any GPU kernel.

### P-D1.2 outcome — 2026-08-23

Run `p-d1-2-2039100ba73e` passed all correctness gates across 48 native hierarchies, 864 frozen rays, and 15,552 policy/variant traces:

- zero missing constructed-hit cells;
- zero hierarchy violations above tolerance;
- zero componentwise or directional certificate violations;
- zero precision-floor events; and
- deterministic manifests and traces.

The proposed **directional segmentation advantage is not supported**. At the central policy, both variants have p50/p95/p99 `0/0/0` extra knots, median 32 local linearizations, and median 7 unique accepted interval chords. `L-CW` requested only five total splits across 864 rays and `L-DIR` requested three; the two removed splits were isolated oblique rays on `PH-rockytrail02-4k-W0`. Every paired median/p95 work ratio is `1.0`.

Decision: do not claim that the first-order hierarchy reduces piecewise-ray segment count. Lazy reconstruction remains useful and certified, but node UV clipping already shortens the ray interval enough in the frozen practical regime. Treat segmentation as representation-independent infrastructure. The first-order ray hypothesis now rests on residual-space rejection/interval clipping and a viable GPU layout, to be tested separately.

Artifacts:

- `experiments/p_d1_lazy_segmentation/p-d1-2-2039100ba73e/`;
- `figures/p_d1_lazy_segmentation/lazy_segmentation_p-d1-2-2039100ba73e.{png,pdf,svg}`; and
- five passing P-D1.2 unit tests.

## 16. Frozen P-D1.3 recursive tube-coverage reference — before implementation

P-D1.3 isolates UV coverage. It creates the independent canonical candidate-cell reference that the event-based DDA must reproduce in P-D1.4. It does not apply min/max or first-order height predicates and does not evaluate leaf roots.

### Inputs and tubes

- reuse all 864 immutable P-D1.2 rays and their valid shell-height intervals;
- form one certified componentwise UV tube for each valid interval; P-D1.2 established that additional knots are almost never requested, so no first-order/directional split policy is involved;
- test every quadtree resolution `1×1,2×2,4×4,8×8,16×16,32×32,64×64`; and
- add analytic horizontal, vertical, diagonal, zero-length, exact grid-edge, exact grid-corner, proxy-edge, and radius-larger-than-cell tubes.

### Two independent enumerators

1. **Exhaustive box reference:** loop over every cell at the requested resolution. Solve the closed affine inequalities for the chord against the cell box expanded by `(epsilon_u,epsilon_v)`, intersect with `[0,1]`, and reject boxes wholly outside the proxy triangle.
2. **Recursive quadtree reference:** start at the unit root, test a node's expanded box, and visit its four children in fixed Morton order until the requested level. It never loops over the target grid and never uses DDA events.

Each enumerator returns a map from integer `(level,ix,iy)` to a closed normalized segment-parameter interval. Degenerate point intervals are retained. Duplicate ownership merges by interval union. The proxy filter uses the closed condition `u_min+v_min≤1`.

### Independence and comparison

Implement the exhaustive and recursive slab solvers separately, with different control flow. Candidate integer cell sets must be identical. For matching cells, independently computed interval endpoints must agree within four binary64 ULPs; the canonical persisted record uses the outward union of both intervals so P-D1.4 cannot inherit a favorable rounding choice from either implementation.

For every analytic edge/corner case, persist the expected closed cell set explicitly and require exact equality. No epsilon movement, half-open ownership, or dropped zero-length interval is permitted.

### Metrics and gate

Report per tube and level:

- exhaustive cells tested, recursive nodes tested, and candidate cells;
- zero-length candidates and cells added by nonzero tube radius relative to centerline coverage;
- maximum interval-endpoint ULP disagreement; and
- omissions/extras by enumerator.

P-D1.3 passes only with zero candidate-set disagreement across every practical and analytic case, all explicit edge/corner expectations satisfied, endpoint disagreement at most four ULPs, deterministic output, and no resource cap. Candidate counts are diagnostics, not performance measurements.

Outputs go under `experiments/p_d1_recursive_tube_reference/` with figures under `figures/p_d1_recursive_tube_reference/`. P-D1.3 does not modify legacy Mode 1 or GPU code.

### P-D1.3 outcome — 2026-08-23

Run `p-d1-3-6a5d55c4c7c3` passed across all 864 practical tubes at seven resolutions (6,048 records) plus nine explicit analytic ownership cases:

- the exhaustive box loop and recursive quadtree produced zero candidate-set disagreements;
- independently computed interval endpoints agreed exactly in binary64 (maximum difference `0` ULP);
- horizontal, vertical, diagonal-corner, stationary-point, grid-edge, grid-corner, proxy-edge, and large-radius expected sets all matched; and
- the complete P-D0 through P-D1.3 regression suite has 24 passing tests.

At `64×64`, the recursive reference tested p50 `45` nodes versus p50 `2,143` proxy cells for the exhaustive oracle; this is an operation count, not a timing result. The certified tube added p50 `0`, p95 `3`, and p99 about `8.37` cells over centerline coverage at that level, with a maximum of `11`. Thus most frozen rays have an almost centerline-sized tube, but the tail is nonzero and justifies a true supercover rather than the legacy zero-width chord.

Artifacts:

- `experiments/p_d1_recursive_tube_reference/p-d1-3-6a5d55c4c7c3/`;
- `figures/p_d1_recursive_tube_reference/tube_coverage_p-d1-3-6a5d55c4c7c3.{png,pdf,svg}`; and
- `scripts/p_d1_recursive_tube_reference.py`, its runner, and four unit tests.

P-D1.3 initially authorized only P-D1.4. During the independent P-D1.4 implementation, exact event arithmetic exposed that this authorization was premature: the two P-D1.3 float interval solvers share cancellation-prone boundary arithmetic. They still agree with one another, but selected practical endpoints truncate the exact interval by as much as `22` ULP. Candidate sets in the tested corpus are unchanged, yet the persisted float interval cannot serve as the canonical conservative oracle.

### Frozen P-D1.3a numerical correction — before implementation

Before continuing P-D1.4:

1. retain the separately structured exhaustive loop and recursive quadtree enumerators;
2. represent every binary64 tube endpoint/radius and dyadic cell boundary as an exact rational number;
3. solve and propagate both enumerators' closed parameter intervals exactly, so inherited recursive clipping cannot silently truncate a descendant interval;
4. require their exact candidate sets and rational interval endpoints to be identical;
5. outward-convert the exact canonical endpoints to binary64 for persistence;
6. retain the original two float solvers as diagnostics only and report their signed ULP distance from the exact outward interval, including whether either float interval is inward/non-conservative; and
7. rerun all 864 tubes, seven levels, and analytic ownership cases under a new schema/run ID, then rerun the full regression suite.

The correction passes only with zero exact candidate/interval disagreement, all analytic expected sets matched, no inward float discrepancy hidden from the report, deterministic persistence, and no change to source tubes. P-D1.4 must consume the corrected run rather than `p-d1-3-6a5d55c4c7c3`. This is a correctness repair to the CPU oracle, not widening the DDA tolerance to accommodate a flawed reference.

### P-D1.3a outcome — 2026-08-23

Corrected run `p-d1-3-dedfc83b59c8` passed. The source tube file is byte-identical to the initial run, while the exhaustive and recursive structures now agree on exact rational candidate sets and exact rational intervals across all 6,048 practical records. The analytic sets still match.

The diagnostic confirms why the correction mattered: the old outward-padded float calculation has `15,288` inward endpoint occurrences when both old enumerators are counted (`7,644` unique exhaustive endpoints), with maximum discrepancy `36,615` ULP. The maximum absolute parameter discrepancy is only about `4.07e-14` (p99 about `6.89e-15`), so this did not change any tested candidate set; nevertheless, a canonical conservative oracle may not knowingly truncate even that amount. The exact-outward run replaces, rather than supplements, `p-d1-3-6a5d55c4c7c3` for all later stages.

## 17. Frozen P-D1.4 event-supercover experiment — before implementation

P-D1.4 adds the event-based CPU DDA promised in Section 5 and compares it to the immutable P-D1.3 canonical coverage. It still performs no displacement-height predicate, leaf equation, root solve, or timing comparison.

### Event construction and traversal

For a tube centerline `c(s)=c_0+s delta`, `s in [0,1]`, at grid resolution `n`, construct events from `0`, `1`, and every in-range solution of

$$
c_u(s)\pm\epsilon_u=k/n,\qquad
c_v(s)\pm\epsilon_v=k/n,
$$

for integer grid boundaries `k`. Because all inputs are binary64 and grid boundaries are dyadic, order distinct events as exact rational values in the CPU oracle, then outward-convert them to binary64 only for persisted records and ULP comparison. At each event, evaluate closed row and column ownership and emit their Cartesian product. On each nonempty open event interval, evaluate its exact rational midpoint, emit the constant active Cartesian product over that interval, and merge repeated cell intervals by integer ID. Record a precision-fallback event whenever distinct exact events collapse to one binary64 value or have no representable binary64 midpoint; exact rational ordering handles it conservatively and terminates, without moving the ray by an epsilon. The later GPU implementation must reproduce these cases with directed rounding or an exact fallback.

Clip emitted cells to the closed proxy triangle and order the final stream by `(entry parameter, Morton code, exit parameter)`. Simultaneous x/y events are one tie group and all closed owners are retained. Reversing the chord must reverse traversal order without changing the candidate set. The implementation must not call either P-D1.3 slab solver or derive candidates by looping over all 2D cells.

### Frozen cases

1. Reuse all 864 P-D1.3 tubes at every level `0..6` and compare against the persisted canonical candidate records.
2. Reuse all nine P-D1.3 analytic cases.
3. Add explicit reversed horizontal, vertical, and diagonal chords.
4. Add nonzero-radius tubes narrower than, equal to, and wider than one cell.
5. Add partially out-of-domain chords that enter and leave the unit square/proxy triangle.
6. Add simultaneous grid-corner and proxy-edge events in both directions.
7. Add adjacent-binary64 event stress cases and require the precision fallback to remain conservative and terminate.
8. Add a fixed-seed held-out fuzz set spanning zero/nonzero deltas, zero/nonzero radii, and endpoints in `[-0.25,1.25]^2`; the seed and generated tube values are serialized before comparison.

### Records and metrics

Persist per tube/level:

- unique event values, open event intervals, simultaneous tie groups, and precision fallbacks;
- maximum active rows/columns, raw emitted cell-events, duplicates merged, and final candidates;
- ordered `(cell,entry,exit)` records and tube-added cells relative to a zero-radius run;
- omissions/extras relative to P-D1.3; and
- maximum endpoint ULP disagreement after both sides use their declared outward canonicalization.

The mechanism figure shows events/candidates versus resolution, duplicate work, and the p50/p95/p99 tube overhead. None is labeled as elapsed time.

### P-D1.4 gate

Pass only if:

- practical, analytic, and held-out candidate sets exactly match the independent recursive/exhaustive reference;
- canonical interval endpoints differ by at most four ULPs;
- every explicit edge/corner tie retains all closed owners, including point-only cells;
- event records are nondecreasing in parameter with deterministic Morton tie order;
- reversed cases preserve sets and reverse the strict traversal order;
- no resource cap exists and every precision fallback is conservative, terminating, and counted; and
- repeated generation is deterministic.

Implementation files are `scripts/p_d1_event_supercover.py`, `scripts/run_p_d1_event_supercover.py`, and `scripts/test_p_d1_event_supercover.py`, with content-addressed outputs under `experiments/p_d1_event_supercover/` and figures under `figures/p_d1_event_supercover/`. Legacy Mode 1 and GPU kernels remain untouched.

### P-D1.4 outcome — 2026-08-23

Run `p-d1-4-a95c6592be27` passed all frozen gates:

- 6,048 practical tube/level records, 22 analytic/special cases, and 256 fixed-seed held-out fuzz cases;
- zero event-only or reference-only cells;
- maximum event/reference endpoint difference `1` ULP;
- deterministic nondecreasing entry order and all four reversed pairs consistent;
- every closed edge/corner owner retained; and
- six explicit precision-fallback events, all from the constructed collapsed-binary64-event stress case and none from the practical corpus.

At `64×64`, exact event counts are p50 `8` and p95 `46`; final candidates are p50 `4` and p95 `25`. The current event oracle deliberately re-emits active Cartesian products at event points and intervals, so its duplicate counts are not a proposed GPU work layout or timing prediction. It proves the closed supercover event contract only.

Artifacts:

- `experiments/p_d1_event_supercover/p-d1-4-a95c6592be27/`;
- `figures/p_d1_event_supercover/event_supercover_p-d1-4-a95c6592be27.{png,pdf,svg}`; and
- seven P-D1.4 tests, bringing the P-D0 through P-D1.4 regression suite to 31 passing tests.

## 18. Frozen P-D1.5 shared leaf/root evaluator — before implementation

P-D1.5 connects the now-verified candidate generators to one declared leaf equation. It is a focused implementation-and-unit gate on analytic cases and a small practical subset; P-D1.6 owns the full 48-window run and final P-D1 report. P-D1.5 still performs no timing and no min/max/first-order pruning comparison.

### Represented leaf equation

Each `64×64` displacement cell uses the already frozen `p00–p11` diagonal and two affine UV-height microtriangles. For one microtriangle, write its plane as

$$
\alpha u+\beta v+\gamma h+\kappa=0.
$$

For the shell ray

$$
u(h)=U(h)/D(h),\qquad v(h)=V(h)/D(h),
$$

the exact represented leaf equation is the cubic-or-lower polynomial

$$
G(h)=\alpha U(h)+\beta V(h)+\gamma hD(h)+\kappa D(h)=0.
$$

Thus P-D1.5 does not intersect planar world-space microtriangles. It solves the actual triangle-proxy shell with the declared piecewise-affine displacement interpolant. A candidate's normalized event interval is mapped back to its stored shell-height interval before root isolation.

### Input reconstruction and immutability

- consume P-D1.2 frozen rays/hierarchies, corrected P-D1.3 tubes, and P-D1.4 ordered candidates;
- reconstruct and serialize each tube's `(h0,h1)`, shell/ray coefficient hash, source-window hash, and candidate-source hashes before root comparison;
- require the reconstructed `(uv0,uv1,epsilon)` to match the persisted tube record; and
- use all three shells and all six ray families on `PH-rock05-1k-W0` for the implementation gate, plus analytic polynomial/ownership cases. P-D1.6 later expands unchanged code to all 48 windows.

### Root isolation, ownership, and fallbacks

1. Isolate all real roots of `G` on the closed candidate height interval by derivative-bracketed linear/quadratic/cubic solving; do not accept raw complex-root filtering as the oracle.
2. Independently audit roots with an 80-digit Decimal interval/root path over the same stored polynomial coefficients.
3. Evaluate `(u,v,t)` from the rational shell ray, retain only closed microtriangle owners and the frozen world-ray interval, and group shared-edge/diagonal duplicates by one geometric root with an explicit owner set.
4. Sort groups by `(t,h,u,v)`; closest-hit and any-hit are derived from this canonical ordering rather than traversal discovery order.
5. Validate every retained root with both `|G(h)|` and the world residual `||F(u,v,h)-(o+t d)||`, plus the represented-height residual.
6. If `G` is identically zero within a certified coefficient test, report `coplanar-degenerate` and invoke an explicit conservative boundary fallback; never silently return no hit. Denominator uncertainty similarly invokes the already declared nonlinear fallback.

### Three-way comparison

For every test ray, compute roots from:

1. P-D1.4 event candidates and their intervals;
2. corrected P-D1.3 recursive candidates and their intervals; and
3. an exhaustive all-proxy-leaf oracle over the complete safe height band.

The first two share only the leaf evaluator after their candidate lists are frozen. The exhaustive oracle additionally runs the Decimal root audit. Persist candidate owner sets, raw roots, grouped roots, closest root, fallback records, and residuals.

### Frozen analytic tests

- constant, linear, quadratic, cubic, repeated, and endpoint polynomial roots;
- no-root and nearly repeated roots;
- roots on the cell diagonal, grid edge, grid corner, and proxy edge with all closed owners;
- forward and reversed ray parameter order;
- a constructed hit for every shell; and
- an identically zero leaf polynomial that must take the declared degenerate fallback.

### P-D1.5 gate

Pass only if event, recursive, and exhaustive grouped root records agree in count, coordinates within declared binary64/Decimal tolerances, and exact owner sets; every constructed hit is retained; closest-hit identity agrees; every residual is below its scale-aware tolerance; all degeneracies/fallbacks terminate and are counted; and records regenerate deterministically. No candidate/segment/iteration cap may produce a miss.

Implementation files will be `scripts/p_d1_leaf_roots.py`, `scripts/run_p_d1_leaf_roots.py`, and `scripts/test_p_d1_leaf_roots.py`, with outputs under `experiments/p_d1_leaf_roots/`. This stage may reuse the mature polynomial and fixed-diagonal utilities in `scripts/prototype_first_order_ray.py` only after cross-checking their conventions; the new event/recursive candidate records remain the authority.

### P-D1.5 outcome — 2026-08-24

Run `p-d1-5-b8c71234fbe3` passed the frozen one-window implementation gate across 18 `(shell,ray-family)` cases:

- event, recursive, and exhaustive grouped root records all agree, including exact closed owner sets;
- the independent 80-digit Decimal root path agrees with the position-terminated binary64 path;
- all 15 constructed hits are retained and the three near-miss constructions are handled without a special assumption;
- maximum event/exhaustive coordinate difference is `1.80e-14`, maximum float/Decimal difference `8.33e-15`, maximum represented-height residual `7.11e-15`, and maximum world ray/shell residual `7.92e-16`; and
- no practical degenerate fallback occurred. The analytic coplanar case explicitly returns a conservative admitted interval instead of silently dropping the leaf.

The event path admitted 145 total cells versus 38,574 exhaustive proxy cells (`266×` fewer leaf candidates) on this subset. This large count ratio is not a speedup claim: shell setup, event generation, hierarchy predicates, memory traffic, divergence, and root cost are not timed here.

The analytic near-clustered-root test also rejected the inherited residual-based root stopping rule. P-D1.5 now partitions by derivative roots and bisects sign-changing monotone intervals until a position-width tolerance is met; a small polynomial value alone no longer terminates isolation. Five P-D1.5 tests pass, bringing the full regression suite to 36 tests.

Artifacts:

- `experiments/p_d1_leaf_roots/p-d1-5-b8c71234fbe3/`;
- `figures/p_d1_leaf_roots/leaf_roots_p-d1-5-b8c71234fbe3.{png,pdf,svg}`; and
- `scripts/p_d1_leaf_roots.py`, its runner, and tests.

## 19. Frozen P-D1.6 full-corpus correctness run — before implementation

P-D1.6 runs the unchanged P-D1.5 represented-leaf equation over all 48 P-D0 windows, three shells, and six ray families: 864 reconstructed tube/root cases. It makes the final P-D1 correctness call and still reports no CPU/GPU timing or first-order-versus-minmax pruning result.

### Inputs and comparison paths

- consume the immutable P-D1.2 rays, corrected P-D1.3 exact recursive records, and passing P-D1.4 event records at `64×64`;
- reconstruct every `(h0,h1)` and require all 864 tube triples plus source/curve hashes to match their persisted inputs;
- evaluate event and recursive candidates with the shared position-terminated binary64 leaf solver;
- evaluate every proxy leaf over the case's full valid height interval with the same binary64 solver as the empirical exhaustive oracle; and
- run the independent 80-digit Decimal solver on every polynomial admitted by the event candidate set and on every exhaustive polynomial for which the binary64 oracle reports a root. The already completed P-D1.5 run remains the frozen full-exhaustive-Decimal audit over one entire practical window.

This selective full-corpus Decimal policy is frozen for tractability, not chosen after results. Candidate completeness is established independently by P-D1.1's curve certificate plus P-D1.3/P-D1.4's exact closed coverage; Decimal checks numerical root locations, while rerunning Decimal on millions of analytically excluded polynomials would not add a new coverage argument.

### Metrics and reports

Persist per case:

- event, recursive, and exhaustive cells/polynomials;
- raw roots, canonical root groups, owner multiplicity, closest root, and constructed-target retention;
- binary64/Decimal coordinate differences and all polynomial/height/world residuals;
- coplanar, denominator, numerical, and resource fallbacks; and
- counts grouped by asset identity, resolution, source bit depth, shell, and ray family.

Figures show candidate distributions, owner/tie cases, root/residual tails, and any failures by corpus stratum. Candidate reduction is labeled as a correctness/work diagnostic, never elapsed time.

### P-D1.6 and final P-D1 gate

Pass only if all 864 cases have matching event/recursive/exhaustive canonical root groups, exact owner sets, identical closest-hit identity, every constructed target retained, binary64/Decimal agreement within the declared tolerance, scale-aware residual gates satisfied, and no unreported or miss-producing fallback. Output and input manifests must regenerate deterministically. Any failure blocks P-D2 and GPU work.

The full runner will be `scripts/run_p_d1_full_reference.py`, writing under `experiments/p_d1_full_reference/` with figures under `figures/p_d1_full_reference/`. It reuses `scripts/p_d1_leaf_roots.py`; it must not fork a second leaf equation. After this run passes, stop and write the detailed P-D2 min/max versus first-order residual-clipping ablation before changing predicates or measuring runtime.

### Initial P-D1.6 outcome: failed gate — 2026-08-24

Run `p-d1-6-1a2502ae0003` completed all 864 cases and correctly failed two gates. Importantly, event, recursive, and exhaustive binary64 grouped roots agree in every case, and all residual gates pass; the closed tube/DDA candidate chain did not omit a root relative to its stored shell interval.

All failures localize to the constructed `proxy-edge-hit` family:

- 12/144 proxy-edge targets are absent from both event and exhaustive roots; and
- 11 additional proxy-edge cases have one binary64 root but no Decimal root inside the stored interval.

The cause is upstream interval truncation, not the DDA or leaf equation. P-D1.2's `_curve_valid_intervals` still uses the inherited residual-terminated polynomial root routine. For `PH-rock05-2k-W1:S1-moderate:proxy-edge-hit:I0`, for example, the represented/Decimal leaf root is about `0.6306675822079804`, while the persisted valid interval begins at `0.630667582208012`. The true closed proxy-boundary root was therefore excluded before tube construction. This is the same clustered/low-derivative stopping defect already exposed by P-D1.5's analytic polynomial test, now appearing in shell-domain partitioning.

### Frozen P-D1.2a shell-interval correction — before implementation

Do not widen the P-D1.5 leaf interval or special-case proxy-edge rays. Correct the producer and regenerate every dependent artifact:

1. move the position-terminated derivative-partition root isolator into a shared low-degree numerical module used by both shell-valid partitioning and leaf roots;
2. return/retain a conservative root-position bracket rather than accepting a small polynomial value as a position certificate;
3. construct each valid shell interval from outward root brackets, allowing adjacent valid/invalid regions to overlap conservatively at uncertain boundary roots;
4. cross-check every shell-domain boundary root against the existing 80-digit Decimal path;
5. require all 144 constructed proxy-edge target heights to lie inside at least one reconstructed valid interval before any tube is built; and
6. increment the P-D1.2 schema, rerun its complete 15,552 traces, then regenerate P-D1.3a, P-D1.4, P-D1.5, and P-D1.6 with new source hashes/run IDs.

The correction gate requires zero denominator uncertainty in the frozen safe bands, zero constructed-target interval omissions, zero candidate/root omissions downstream, deterministic manifests, and no regression in the prior segmentation-certificate gates. The earlier P-D1.2 conclusion about directional segmentation must be recomputed but not assumed to change. Failed run `p-d1-6-1a2502ae0003` remains preserved as the negative-result record.

### P-D1.2a and regenerated-chain outcome — 2026-08-24

The shared position-terminated/Decimal-audited shell partition correction passed. The complete replacement provenance chain is:

- P-D1.2: `p-d1-2-e2749e3ff000`;
- exact P-D1.3: `p-d1-3-62f0f7416547`;
- event P-D1.4: `p-d1-4-dd1251769c51`;
- leaf P-D1.5: `p-d1-5-b7bcc9a005a6`; and
- full P-D1.6: `p-d1-6-e254f252e010`.

P-D1.2 again passes all 15,552 traces with every required hit retained, no certificate violation, and no precision-floor event. Its scientific segmentation conclusion is unchanged: `L-DIR` does not materially improve over `L-CW`. The corrected downstream P-D1.3/P-D1.4 runs again have zero candidate disagreement and maximum event/reference endpoint difference `1` ULP. The corrected P-D1.5 anchor again agrees across event, recursive, exhaustive, and full Decimal paths.

### P-D1.2 exact-hierarchy audit after P-D2.0b — 2026-08-24

P-D2.0b's tighter direct residual certificate exposed that the old hierarchy fold's single-ULP widening was not a proof after multiple rounded operations. The hierarchy producer now computes leaf and parent centered envelopes exactly over binary64 inputs before outward conversion. The hierarchy-dependent P-D1.2 sweep was regenerated as `p-d1-2-ec0dd6a036f5`; all 15,552 traces pass and the negative directional-segmentation conclusion remains unchanged. The P-D1.3–P-D1.6 tube/event/root artifacts remain authoritative because those stages do not consume hierarchy residual radii.

### Final P-D1.6 outcome — passed 2026-08-24

Run `p-d1-6-e254f252e010` passes the final P-D1 gate across 48 practical windows and 864 tube/root cases:

- event, recursive, and exhaustive binary64 canonical root groups match in every case;
- the selective full-corpus Decimal audit matches every exhaustive binary64 root group;
- all constructed hits are retained, including `144/144` proxy-edge cases;
- closest/root owner sets match, with 936 total canonical root groups;
- no practical coplanar/denominator/numerical/resource fallback occurs;
- maximum event/exhaustive coordinate difference is `2.74e-14`, maximum binary64/Decimal difference `1.65e-14`, maximum represented-height residual `1.15e-13`, and maximum world ray/shell residual `9.09e-16`; and
- the current event coverage admits 6,431 total leaf cells versus 1,851,552 all-proxy cells. This `288×` count ratio is only a correctness/work diagnostic, not a DDA speed measurement or a first-order-versus-minmax comparison.

The complete P-D0 through P-D1.6 regression suite has 36 passing tests. The failed pre-correction run `p-d1-6-1a2502ae0003` remains preserved and documented; no result was overwritten.

Primary final artifacts:

- `experiments/p_d1_full_reference/p-d1-6-e254f252e010/`;
- `figures/p_d1_full_reference/full_reference_p-d1-6-e254f252e010.{png,pdf,svg}`; and
- `scripts/p_d1_numeric.py`, `scripts/p_d1_leaf_roots.py`, and the staged P-D1 runners/tests.

Decision: P-D1 establishes the triangle-proxy shell-ray, certified tube, closed event supercover, and represented cubic leaf equation as a coherent CPU correctness contribution. It does **not** yet establish that the first-order hierarchy accelerates DDA. P-D2 must isolate min/max versus first-order residual clipping on the identical certified traversal before any GPU performance claim.

---

Related: [[Plan — P-D0 practical displacement dataset]] · [[Plan — Practical first-order DDA ray traversal]] · [[Guide — Eurographics paper draft]]
