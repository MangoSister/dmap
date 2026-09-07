---
title: Project — Conservative first-order queries on displacement maps
tags: [project, displacement, conservative-bounds, taylor-model, ray-tracing, area-sampling, closest-point, tessellation-free]
status: shared-thesis-failed-ray-acceleration-reopened
created: 2026-08-16
updated: 2026-09-07
target-venue: TBD (Eurographics / SIGGRAPH Asia)
---

# Project — Conservative first-order queries on displacement maps

> [!important] Maintained paper map — 2026-09-07
> The current paper organization is **Application 1: Area/Sampling**,
> **Application 2: Ray Tracing**, and **Application 3: pending**. Use
> [[Paper — Application 2 — Tessellation-free ray tracing]] as the maintained
> ray chapter through P31. Earlier notes may number ray tracing first and record
> superseded negative architectures; preserve them for provenance, but do not
> use their numbering or conclusions as the current paper state.
> Vault navigation and historical classifications: [[Index — 60 Projects]].

> [!success] P19 residual-gated ray acceleration — 2026-09-05
> The later full-renderer P19 architecture reopens a narrow ray-only result
> without reversing the failed shared ray-plus-area thesis. A lean packed
> first-order 4x4 hierarchy is selected only for slope-dominant maps and only
> for secondary-closest/visibility traversal; primary dispatch is independent,
> and rough maps retain scalar P18. A same-executable 15-case ablation over
> three maps and five geometries measures `0.9505x` first-order/scalar
> full-frame geomean, 14/15 faster cases, and byte-exact outputs. The frozen
> artifact and claim boundary are in `docs/p19_first_order_ray_tracing.md`.
> This does not rescue first-order area sampling, certify the approximate
> shell-ray chord, or establish TFDM superiority.

> [!abstract] Initial verdict
> A coherent combined project is possible if the **joint first-order displacement hierarchy** is the paper's central contribution and ray tracing and area sampling are its first two consumers. The umbrella should be conservative geometric queries, not only intrinsic metric queries: surface area is metric, while ray intersection is an extrinsic visibility query. Certified closest-point projection is a coherent possible third query, but it is explicitly deferred until the first two applications are complete and have passed their gates. The paper succeeds only if one shared hierarchy is competitive with specialized structures for its included applications. It should not absorb the heat method, full walk-on-spheres, spectral LOD, and every other application from the metric project.

> [!success] Initial ray-reference checkpoint — 2026-08-16
> The approved Python-first ray reference is implemented in `scripts/prototype_first_order_ray.py`. Its initial procedural gates show zero candidate omissions and hit mismatches, certified curve-tube coverage, agreement with an independent 80-digit cubic-root path, expected singular fallbacks, strong rejection on ramps, modest leaf reduction on smooth maps, and no gain on checkerboards. This is encouraging but only a partial H1/H3 result; broader saved-seed/asset validation and the representative tightness sweep still precede CUDA work. See [[Plan — Ray application method, prototype, and baselines#13.8 Initial implementation checkpoint — 2026-08-16]].

> [!success] Reproducible notebook checkpoint — 2026-08-17
> The reference now has a separate importable analysis layer and an executed notebook that generates method, node-bound, TFDM-box-relationship, and regime-sweep figures as PDF/SVG/PNG. A clean run takes about 21 seconds and the unchanged 384-ray default correctness gate still reports `FAILURES=0`. The figures are mechanism evidence only; they do not authorize a GPU-performance claim. See [[Plan — Ray application method, prototype, and baselines#13.10 Reproducible notebook and initial paper figures — 2026-08-17]].

> [!info] Step 1 experiment frozen before implementation — 2026-08-23
> [[Plan — Step 1 ray reference experiment]] now defines the representative procedural and real-map corpus, shell and ray families, persisted metric schema, plots, and G0–G4 go/no-go rules. Step 1 uses CPU recursive-quadtree traversal and exact analytic leaves; it includes no DDA, CUDA, or external-baseline timing. Only a pass of the correctness, first-order-benefit, and certification gates can authorize the narrow fixed-segmentation GPU slab ablation.

> [!success] Step 1 persistence P1 — 2026-08-23
> The versioned `step1-v1` schema, stable IDs, record/manifest validators, and deterministic JSONL/GZIP I/O are implemented with 17 passing synthetic tests. No ray predicate or traversal was changed. The active authoring aid is [[Guide — Eurographics paper draft]]; the user will write the full draft, while the guide tracks arguments, proof obligations, experiments, ablations, and evidence-safe wording.

> [!success] Step 1 aggregate persistence P2 — 2026-08-23
> The aggregate adapter now passes 26 combined tests and persisted run `p2-40a9c6e5e680` (14 procedural map/shell cases, 896 rays) with zero candidate omissions, hit mismatches, or high-precision mismatches. First-order used 2.9% fewer node tests and 13.9% fewer accepted leaf-interval tests in this small legacy suite. This is a persistence/equivalence checkpoint only: it has no per-ray tails, lazy variants, real maps, DDA, or timing. P3 is planned in detail before traversal instrumentation.

> [!success] Step 1 detailed persistence P3 — 2026-08-23
> The optional observer, per-ray/per-node streams, all-four-method detailed runner, and summary-from-rays checks pass 32 tests and the unchanged 384-ray legacy correctness gate. Run `p3-f29f4158c87f` records 896 rays with zero failures and exact agreement with P2 global totals. Global segmentation is cheap here (p95 one, max two segments), and fixed first-order retains a 13.9% leaf-test reduction. The directional lazy variant is presently negative: 5,300 directional bounds save only four linearizations and two node tests. Treat its explanatory figure as mechanism intuition until the frozen family/real-map G4 study says otherwise.

> [!success] Reproducible lifecycle P4/P5 development smoke — 2026-08-23
> The atomic lifecycle and frozen dataset loader now pass 50 total tests. Attempt `step1-512846b663fc/a0001` finalized all 896 development rays with independently verified hashes, counts, references, sequences, and zero failures. Dataset manifest `dataset-2a294309ffff` freezes 15 decision maps with audited source/grid hashes and a visually checked overview. The controlled 11,520-ray input corpus is the next evidence gate; its construction contract is frozen in [[Plan — Step 1 controlled ray generator]] before implementation.

> [!success] Controlled ray corpus D2 — 2026-08-23
> The frozen six-family generator and exact represented-surface construction now pass 61 total tests. Suite `rays-935162cc636e` contains 45 map/shell cases and 11,520 independently hash-verified rays spanning aimed, free, grazing, one-ULP boundary, origin/range, and paired near-miss conditions. Its sample sheet is an input audit only. This closes corpus construction, not the technical-contribution evidence gap: exhaustive oracle annotation and the fixed `G-MM`/`G-FO` decision run remain next.

> [!success] Exhaustive oracle O1 — 2026-08-23
> Independent truth is now frozen for every ordinary decision ray. Float run `oracle-dd072597b14a/a0001` groups 29,300 raw owner hits into 25,387 physical roots over 11,520 rays with zero ordinary constructed-root failures. The linked 80-digit run checks 100,980 cubics on 90 preselected rays and finds 255/255 matching complete root groups with zero mismatches. All 70 P1–O1 tests pass. This materially strengthens the correctness/general-corpus foundation, but it still does not establish first-order pruning benefit, GPU advantage, or baseline superiority; the next evidence gate is fixed `G-MM` versus `G-FO` on these exact oracle records.

> [!summary] Historical technical-contribution checkpoint — superseded by P-D2.0c
> **Supported at reference level:** (C1) an exact conservative first-order height/gradient hierarchy for the fixed displaced-microtriangle surface, and (C2) its conservative composition with certified rational shell-ray tubes and exact cubic leaves. **Preliminary efficiency support:** fixed first-order rejection reduces leaf work on the small mixed procedural suite while global curve certification remains cheap. **Not supported:** broad directional-lazy knot reduction. **Still unestablished for an EG paper:** real-map generality, controlled incidence/conditioning behavior, GPU cost, superiority or competitiveness against TFDM/Ogaki/RMIP/PDM/triangles/DMM, the area-sampling consumer, and the shared-system Pareto claim.

> [!warning] Full-corpus ray acceleration decision — 2026-08-24
> P-D2.0c run `p-d2-0c-47849fef3202` validates all 5,184 traversals over 864 practical cases but rejects the current arithmetic-average single-plane ray-speed hypothesis. In the primary coherent cohort, componentwise first order uses `0.9966×` the scalar nodes and `0.9674×` the scalar leaf cubics; the hybrid uses `0.9906×` and `0.9622×`. Both fail the frozen leaf and break-even gates before first-order payload cost is charged.

> [!failure] Representation upper-bound decision — 2026-08-24
> The completed [[Plan — First-order ray representation oracle]] shows that this is not primarily a poor plane-fitting problem. `HYB-OPT1` still uses `0.9841×` coherent nodes and `0.9611×` leaves. Even a recertified convex support-hull oracle, counted unrealistically as one node test, uses `0.9818×` nodes and `0.9548×` leaves and has negative break-even margin. Both frozen gates fail. Stop optimized-plane, slope-dictionary, full-resolution, and GPU work for first-order affine support clipping on this fixed topology. The certified scalar min/max-DDA ray architecture remains a separate, untested performance hypothesis.

> [!info] Active ray-architecture plan — 2026-08-24
> The user chose to finish the ray application before resuming area sampling. [[Plan — Certified ray architecture comparison]] freezes a separate comparison of certified piecewise shell-ray tube-supercover min/max DDA against an audited Ogaki-style nonlinear quadtree and TFDM. First-order clipping is excluded from the causal claim. A0 now passes: the versioned harness, randomized GPU-event protocol, TFDM GPU probes, and source-correspondence audit are operational. The next authorized action is the detailed A1 mathematical-port plan; the certified GPU mode remains gated.

> [!info] Area/product application plan frozen — 2026-08-25
> [[Plan — Area and product sampling application]] now freezes the exact A2-compatible surface derivatives, clipped-domain oracle, three distinct sampling semantics, practical corpus, baselines, metrics, and S0–S6 gates. The first implementation action is S0 only. In particular, a hierarchical proposal with an exact algorithmic PDF is not called exactly area-proportional, and repeat-until-accepted sampling is not assigned a known normalized PDF unless its area/power normalizer is separately available.

> [!success] S0 area-oracle development checkpoint — 2026-08-25
> Run `area-s0-a89dd07aafec` passes eight tests and all three signed-A2 development cases. Direct and Gram Jacobians agree to `4.13e-16`, analytic tangents agree with finite differences to `4.26e-10`, and fixed/adaptive total areas agree to `3.53e-16`. This validates the exact unnormalized-direction metric and clipped-domain oracle; it says nothing yet about first-order bound tightness or sampling performance. [[Plan — S1 conservative area bounds]] freezes the next isolated representation comparison before implementation.

> [!success] S1 conservative area-bound decision — 2026-08-25
> Full run `area-s1-full-8d3e3c63583a` passes every correctness and scientific gate over 144 signed-A2 surfaces. On 96 ordinary surfaces, `FO-COMPOSED` leaf acceptance is median/p10/minimum `0.9945/0.9658/0.7867`; cap-mass overhead over the per-surface certified `J-SCALAR` hierarchy is median/p90/maximum `1.0016x/1.0149x/1.1373x`. All 48 stress surfaces remain above `0.7506` acceptance. This establishes first-order area-cap feasibility near cell resolution, not practical speed: ordinary median acceptance is only about `0.47` at the root and tightens with descent. [[Plan — S2 area sampling and PDF audit]] freezes the next probability-correctness stage.

> [!success] S2 area-sampling PDF gate — 2026-08-25
> Run `area-s2-cac179574d19` passes all analytic and statistical checks for `UV-UNIFORM`, `FO/J-PROP`, `FO/J-NULL`, and validation-only `FO/J-REPEAT`. Maximum PDF identity error is `3.55e-15`, maximum absolute estimator z-score is `2.33`, and minimum measured `FO-NULL` acceptance is `0.7672`. The first completed computation exposed a NumPy-boolean JSON serialization bug before any case was persisted; the identical frozen streams were rerun after a regression-tested scalar-normalization fix. [[Plan — S3 area sampling quality and cost decision]] freezes the next variance/cost stage.

> [!warning] S3 development proposal-quality checkpoint — 2026-08-25
> Run `area-s3-dev-f94aa877d085` validates deterministic variance against S2 trials but rejects the current local-child `FO-PROP` on all three development surfaces: it has `30.30x`, `2.15x`, and `3.82x` the UV-uniform total-area variance. Local child caps are not descendant mass sums, so path normalization error compounds even when leaf caps are tight; `J-PROP` also loses on two cases. One predeclared repair, subtree-summed `FO/J-LEAFCDF`, is authorized with one per-instance scalar weight per node charged explicitly. This can rescue sampling quality but not the storage-free shared-sampler claim.

> [!failure] Final shared first-order project decision — 2026-08-25
> S3 run `area-s3-full-f245aff67ea7` completes 144 surfaces and closes the second application gate. On the 69 nontrivial ordinary surfaces, shared/on-the-fly `FO-PROP` has `16.342x` the UV-uniform total-area variance. The only authorized repair, `FO-LEAFCDF`, beats UV (`0.1377x`) but requires a per-instance scalar tree, is `1.4729x` `J-LEAFCDF` in geometric mean and `2.1785x` at p90, never beats dense-cell variance, and has higher storage than J at 1–16 instances. Together with the failed first-order ray gate and mixed/negative A2 timing, this rejects the proposed shared first-order performance thesis. S4 product sampling, a third query, and sampling GPU/baseline ports are not authorized as rescue attempts.

## 1. Research question

Can one compact, conservative hierarchy over displacement and its derivatives support both visibility and surface-measure queries on displaced surfaces without storing displacement-resolution tessellation?

The proposed answer is a **joint Taylor-model pyramid** over height and gradient. Each node stores a local displacement plane plus conservative remainders. The same node can be interpreted as:

- a thin tilted slab for ray–surface rejection;
- a conservative normal cone;
- an enclosure of the displaced first fundamental form;
- a bound on surface-area density; and
- a fallback min/max height interval.

The initial paper applications are:

1. conservative ray intersection using certified piecewise shell rays and hierarchical DDA; and
2. surface-area or emission-times-area importance sampling with a correct pointwise PDF.

A possible third application is certified closest-point projection using conservative branch-and-bound over the same node enclosures. It is a **post-gate extension**, not part of the current implementation schedule. Ray intersection and area/product sampling must be finished first.

## 2. One-sentence thesis

> We develop a conservative displacement hierarchy and two tessellation-free query consumers—certified nonlinear ray intersection and surface-measure sampling—and characterize where retained first-order information is and is not useful.

This was the candidate paper thesis, but the final S3 evidence rejects its required shared-performance premise. The certified ray method, exact surface differential, conservative area caps, and PDF derivations remain technically valid; first-order ray acceleration and first-order sampling advantage are not supported on the fixed topology. A future paper must narrow to a substantially improved ray architecture or formulate a genuinely different representation/query hypothesis rather than adding a third application to compensate.

This is a **representation-and-query paper**, not a collection of unrelated rendering techniques. The same conservative hierarchy may support both applications, but the evidence no longer requires first-order payloads to accelerate both; any final shared-performance claim must be earned by the area result and an honest ray memory/work accounting.

## 3. Scope

### 3.1 Included initially

- Scalar displacement over a coarse triangle proxy.
- General coarse triangle shells with shared interpolated displacement directions and per-face displacement domains.
- Shared interpolated proxy displacement directions with explicit watertight edge conventions.
- A fixed two-triangle, piecewise-affine height reconstruction per texture cell, mapped through the analytic shell $S=P+hN$; other reconstruction filters only after the core result works.
- A joint first-order bound pyramid.
- Conservative ray intersection.
- Surface-area and, if successful, emission-product importance sampling.
- GPU implementation and time–memory–quality evaluation.

### 3.2 Conditional expansion — only after the first two applications

Before the general deferred list, one application has a special status:

- **Conditional third application:** certified closest-point projection.
- It may be considered only after the ray and area-sampling applications pass their correctness and performance gates.
- Full projected walk-on-spheres remains out of scope unless closest point is successful and a conservative local-feature-size bound is also established.

### 3.3 Other deferred or excluded work

- Heat-method geodesics and general PDE solvers.
- Projected walk-on-spheres.
- Spectral or anisotropic LOD as a full application.
- Vector displacement.
- Conversion from an arbitrary smooth or dense target into the triangle-shell representation.
- A claim of universal performance superiority over RMIP, PDM, DMM, or dense tessellation.

The deferred queries remain valid future consumers of the hierarchy. They should appear only as motivation or a short outlook. Failure of an initial application should trigger a scope decision, not an automatic pivot into an unfinished third application.

## 4. Geometry contract

At runtime a proxy triangle defines the shell

$$
F(a,b,h)=P(a,b)+hN(a,b),
$$

where $P$ and $N$ are barycentrically interpolated. The target displaced surface uses a scalar residual $h(u,v)$ over this shell.

This representation is exact only when the target is a single-valued graph along the chosen displacement field. In general, projection

$$
h=\langle T-P,N\rangle
$$

does not remove tangential residual. Any external converter must therefore find a valid correspondence, refine or optimize the proxy until the residual meets an explicit error tolerance, or report the result as an approximation. Conversion is outside the active project scope; the framework begins with a valid triangle shell and displacement field.

At shared proxy edges, both incident triangles must agree on displacement direction, scalar edge samples, orientation, filtering, and quantization. This is part of the representation contract rather than optional implementation polish.

## 5. Common first-order hierarchy

For a node $\Omega$ centered at $(u_0,v_0)$ with half-extents $(s_u,s_v)$, store

$$
(h_0,g_u,g_v,r,\rho_u,\rho_v)
$$

such that, for every point in the node,

$$
\left|h-\left(h_0+g_u\Delta u+g_v\Delta v\right)\right|\le r,
$$

$$
|h_u-g_u|\le\rho_u,
\qquad
|h_v-g_v|\le\rho_v.
$$

The local plane explains first-order height variation. On smooth data the remaining height width should scale as $O(s^2)$ rather than the $O(s)$ slope range retained by scalar min/max bounds.

For the fixed two-microtriangle reconstruction, an exact conservative leaf is constructed from the two constant gradients and the four corner residuals. Parent models are built by the same conservative bottom-up fold. The general hierarchy derivation is in [[Taylor-model bound pyramid]]; the surface-specific leaf and ray derivation are in [[Plan — Ray application method, prototype, and baselines]].

### 5.1 Two physical views

The logical node has six values, but the GPU need not fetch all six for every query:

- **Ray view:** $(h_0,g_u,g_v,r)$, preferably packed or quantized.
- **Metric view:** the full $(h_0,g_u,g_v,r,\rho_u,\rho_v)$ node.

Both views must derive from the same hierarchy and conservativeness contract. Separate layouts or textures are allowed if they avoid unnecessary ray-tracing bandwidth, but duplicating two independently built acceleration structures would weaken the shared-framework claim.

### 5.2 Recovered zero-order bounds

The node immediately gives

$$
h\in h_0\pm\left(|g_u|s_u+|g_v|s_v+r\right),
$$

so existing min/max consumers can be retained as fallbacks and ablations.

## 6. Application A — conservative ray intersection

Detailed mathematical formulation, prototype gate, reuse audit, and paper baseline protocol: [[Plan — Ray application method, prototype, and baselines]].

### 6.1 Connection to the hierarchy

Let a linearized shell-space ray segment be

$$
q(s)=\bigl(u(s),v(s),h_r(s)\bigr), \qquad s\in[0,1].
$$

Against the node plane $\hat h=h_0+g_u\Delta u+g_v\Delta v$, define

$$
f(s)=h_r(s)-\hat h(u(s),v(s)).
$$

Because both the segment and the node model are affine, $f(s)$ is linear. Its extrema occur at the two segment endpoints. If

$$
f([0,1])\cap[-r,r]=\varnothing,
$$

the node cannot contain a surface intersection and may be skipped conservatively.

This is the central connection: a smooth but tilted displacement ramp produces a thin slab, whereas scalar min/max stores the entire height change across the node.

### 6.2 Curved-ray uncertainty

The world ray maps to a rational nonlinear curve in triangle-shell coordinates. The Taylor slab does **not** by itself certify replacement of that curve by a chord. Every emitted segment must carry a conservative tube bound.

If the true curve differs from the chord by

$$
|\delta u|\le\epsilon_u,\quad
|\delta v|\le\epsilon_v,\quad
|\delta h|\le\epsilon_h,
$$

then the slab test must be widened by at least

$$
\epsilon_h+|g_u|\epsilon_u+|g_v|\epsilon_v,
$$

plus any additional uncertainty required by the node-domain and parameterization bounds. A zero-width DDA along an approximate chord is not a correctness argument.

### 6.3 Proposed traversal

1. Intersect the world ray with a conservative proxy-shell bound.
2. Partition the rational shell ray at singularities and texture-coordinate monotonicity events.
3. Construct certified linear segments or curve tubes over the valid intervals.
4. Traverse the hierarchy using hierarchical DDA or a hybrid DDA/quadtree policy.
5. Reject nodes using the widened Taylor-slab test.
6. At a leaf, intersect or refine against the represented bilinear/microtriangle geometry using the original world ray.
7. Report the closest hit with correct proxy coordinates and shading derivatives.

The existing Mode 1 implementation is an engineering prototype for steps 3–6, not yet a conservative implementation. Certification, caps/fallbacks, ordering, and leaf-coordinate correctness remain work items.

## 7. Application B — surface-area and product sampling

For

$$
S(u,v)=P(u,v)+h(u,v)N(u,v),
$$

the hierarchy bounds $h$ and $\nabla h$. Combined with per-triangle base quantities, it can conservatively enclose the first fundamental form $G$ and the surface-area density

$$
J(u,v)=\sqrt{\det G(u,v)}.
$$

The initial application is hierarchical sampling proportional to surface area. The stronger renderer-facing application is product sampling proportional to

$$
w(u,v)=L_e(u,v)J(u,v),
$$

where $L_e$ is emitted radiance or another nonnegative texture-space importance field.

The hierarchy may be used for conservative rejection, adaptive integration, or hierarchical proposal construction. Whatever mechanism is chosen must evaluate the final pointwise density consistently and return the correct PDF. “Uses conservative bounds” does not automatically imply unbiased sampling.

## 8. Conditional Application C — certified closest-point projection

This application is technically coherent because it consumes the same node enclosures and conservative pruning logic. It is **not authorized to consume implementation time until Applications A and B are finished and have passed their gates**.

For a query point $x$, branch-and-bound needs a conservative node lower bound

$$
d_{\min}(x,S_\Omega)\le \min_{p\in S_\Omega}\|x-p\|.
$$

A node may be discarded when this lower bound is no smaller than the best known feasible distance. The Taylor height plane, its remainder, the node's parameter domain, and optional normal cone must be composed into a certified world-space enclosure or a rigorously related shell-space distance bound. Treating the texture-space slab as though it were directly Euclidean is not valid without controlling distortion through the shell map.

A complete closest-point query would:

1. construct conservative lower bounds for hierarchy nodes;
2. visit nodes best-first or depth-first with a current feasible upper bound;
3. solve closest point on the exact represented leaf surface;
4. return position, parameters, normal, distance, and a certificate that no pruned node was closer; and
5. compare against a closest-point query over dense tessellation and a triangle BVH.

If this primitive succeeds, projected walk-on-spheres is a possible later demonstration. It is not implied by closest point alone: PWoS also requires a conservative local-feature-size or valid-tube bound. That requirement receives a separate go/no-go decision.

## 9. Candidate technical contributions

The initial reviewer-visible contribution set is:

1. **Joint first-order displacement pyramid.** An exact two-microtriangle piecewise-affine leaf model and quadrant-aware conservative fold over $(h,\nabla h)$ that subsumes min/max height and exposes correlated first-order bounds.
2. **Query-bound derivations.** Conservative propagation to tilted slabs, normal cones, the induced metric, and surface-area density.
3. **Visibility application.** Certified piecewise shell-ray traversal using tube-widened Taylor slabs and output-sensitive hierarchical DDA.
4. **Measure application.** Tessellation-free area or emission-product sampling with correct PDFs.

For ray tracing, the first-order hierarchy and ray linearization have separate responsibilities. Certified piecewise segments bound the error from replacing the nonlinear shell-space ray by chords. The first-order node removes predictable local displacement slope from the surface uncertainty. Their composition yields the conservative node width

$$
r+|g_u|\epsilon_u+|g_v|\epsilon_v,
$$

followed by exact cubic leaf intersections. Segmentation alone is not a sufficient novelty claim, and the first-order node alone does not certify the chord approximation. The visibility contribution is their conservative composition.

The strongest paper distinction from Ogaki, TFDM, RMIP, PDM, dense triangles, and DMM is not universal ray speed. It is a surface-aligned conservative hierarchy that may reduce ray work on smooth slope-dominated maps and is also directly consumable by metric/area sampling. The intended claim is a combined correctness–time–memory–update Pareto advantage in stated regimes. The detailed baseline advantages, expected loss regimes, and non-novel ingredients are recorded in [[Plan — Ray application method, prototype, and baselines#16.4 Contribution boundary and expected advantage over each baseline]].

After the initial set is complete, **certified projection** may become an additional contribution if the hierarchy supplies tight distance lower bounds and competitive branch-and-bound performance. It should not be promised in the initial contribution list.

A traversal-aware proxy constructor may become an additional contribution only if its objectives materially improve the measured Pareto frontier. It is not part of the active scope.

## 10. Why this could be a single paper

The first two applications ask different questions but consume the same correlated local model. A conditional third consumer would use the same enclosures for projection:

| Common node quantity | Visibility use | Measure use | Conditional projection use |
|---|---|---|---|
| $h_0,g_u,g_v$ | local tilted surface plane | central metric and slope | central surface model |
| $r$ | slab thickness | height-dependent metric uncertainty | spatial-enclosure thickness |
| $\rho_u,\rho_v$ | optional normal-cone tightening | gradient and area-density uncertainty | normal cone / distance-bound tightening |
| per-triangle $P,N$ derivatives | shell mapping | induced metric | shell distortion and world-space enclosure |

The paper is coherent if experiments show that this common representation is useful to both committed consumers. It becomes “two papers stapled together” if ray tracing uses only an unrelated min/max map, area sampling uses a separate hierarchy, or the Taylor model is incidental to either result. Closest point strengthens the framework only if it reuses the same hierarchy directly and fits after the two-application paper is already viable.

## 11. Main reviewer attacks

### 11.1 “This is just a Taylor model in a mipmap”

The response must be technical and empirical: exact piecewise-affine microtriangle leaf construction, conservative fold, shared height/gradient correlation, propagation to multiple geometric quantities, GPU layout, and measured tightness and query benefits. A focused literature review of centered forms, affine arithmetic, gradient pyramids, LEADR follow-ups, and tangent-bound structures is required before claiming novelty.

### 11.2 “Use a specialized structure for each query”

Compare against specialized baselines:

- scalar min/max or RMIP-style structures for ray tracing;
- a scalar precomputed area-density pyramid, dense alias table, or tessellated sampler for area sampling.

Report total memory, build/update cost, and query time. The common hierarchy must offer a useful combined Pareto point, not necessarily win every individual column.

### 11.3 “The ray result comes from segmentation and DDA, not the hierarchy”

Ablate:

1. nonlinear traversal + min/max;
2. piecewise segments + min/max;
3. piecewise segments + Taylor slabs;
4. certified tubes + min/max supercover;
5. certified tubes + Taylor-slab traversal.

This separates the benefit of linearization, DDA, certification, and the common hierarchy.

### 11.4 “The sampling application does not need joint bounds”

Compare against directly storing or integrating $J$. Test static maps and edited maps. The framework is most defensible when base geometry, displacement, or emission changes and rebaking a final scalar importance distribution is costly or undesirable.

### 11.5 “The represented surface is ambiguous”

Fix one leaf interpolation and surface contract. Ray hits, metric evaluation, area oracle, and sampling PDFs must refer to the same mathematical surface. If ray leaves use planar microtriangles while metric leaves use smooth bilinear geometry, quantify the discrepancy and do not claim identity.

## 12. Hypotheses and kill tests

### H1 — bound tightness

On smooth and piecewise-smooth displacement, Taylor slab widths are substantially tighter than scalar min/max at the hierarchy levels actually visited by queries.

**Test:** for each node and level, compare conservative bound width with the dense true range. Include ramps, smooth rocks, sparse features, high-frequency noise, and adversarial checkerboards.

**Gate:** if the bounds are routinely too loose at useful traversal levels, stop application work and revise the node/fold.

### H2 — ray benefit survives bandwidth

Reduced node visits and leaf tests outweigh the cost of fetching and evaluating the larger ray node.

**Test:** replace current Mode 1 min/max rejection with a four-component Taylor-slab view while keeping the ray path otherwise fixed. Measure cells, leaves, bytes, cache behavior, occupancy, and end-to-end time.

**Gate:** if no intended scene regime shows a repeatable end-to-end benefit after layout and packing work, ray tracing should not be presented as a performance application of the hierarchy.

### H3 — ray traversal can be certified affordably

Curve-tube inflation does not erase the slab tightness or make DDA visit prohibitively many neighboring cells.

**Test:** adversarial rational curves near grid boundaries and singularities, followed by a sweep over shell thickness, normal variation, incidence angle, UV footprint, and texture resolution.

**Gate:** if certification consistently removes the advantage, investigate a hybrid DDA/RMIP traversal rather than weakening the correctness claim.

### H4 — area sampling benefits from the joint model

Metric/area bounds are tight enough to produce efficient proposals or rejection rates, and the shared hierarchy is competitive in memory and update cost with a specialized area-only structure.

**Test:** compare bound efficiency, samples per accepted point, variance, build/update time, and total memory against dense tessellation and scalar area-density pyramids.

**Gate:** if a specialized scalar area pyramid dominates even for relevant edit scenarios, keep area sampling as a demonstration rather than a headline contribution—or replace it with a query that truly needs joint bounds.

### H5 — one hierarchy is better than two

The integrated system has a meaningful total-memory/build/update advantage over maintaining independent visibility and sampling structures.

**Test:** measure the combined workload, not only isolated query kernels.

**Gate:** if the optimal implementation duplicates most stored data and preprocessing, reconsider the unified-paper thesis.

### H6 — conditional closest-point projection is both certified and practical

The hierarchy can provide world-space distance lower bounds tight enough for useful closest-point branch-and-bound.

**Prerequisite:** do not start this test until the ray application passes H2 and H3, the sampling application passes H4, and the combined system passes H5.

**Test:** compare certified node lower-bound tightness, nodes visited, closest-point error, query time, and memory against dense tessellation with a triangle BVH. Include points near edges, silhouettes, folds, oblique proxy shells, and locations with competing nearby sheets.

**Gate:** if the required world-space enclosure is too loose or too expensive, retain closest point as future work. Consider PWoS only after closest point passes and a separate conservative local-feature-size bound is demonstrated.

## 13. Evaluation plan

### 13.1 Shared correctness oracle

- Define the exact leaf surface and filtering convention.
- Build an independent dense reference only for validation.
- Validate hit/miss, closest-hit distance, surface position, normal, area integral, and sampling distribution.
- Include shared-edge, silhouette, grazing, near-singular, and proxy-obliquity stress tests.

### 13.2 Ray-tracing baselines

- Current Ogaki-style nonlinear traversal.
- TFDM.
- RMIP.
- PDM with step-size/error sweep.
- Dense tessellated triangle BLAS.
- DMM where available.
- Min/max DDA using the same proxy and shell rays.

Compare algorithm-only results on an identical proxy and end-to-end results at matched geometric accuracy.

### 13.3 Sampling baselines

- Uniform parameter-space sampling.
- Dense tessellated area sampling.
- Scalar precomputed area-density pyramid.
- Dense alias/CDF distribution where memory permits.
- Existing product-importance methods relevant to the final emission-sampling formulation.

### 13.4 Conditional closest-point baselines

Evaluate these only after H2–H5 pass:

- Dense tessellated surface with a triangle-BVH closest-point query.
- Brute-force evaluation of the represented leaf surface as a correctness oracle.
- Zero-order min/max/AABB branch-and-bound over the same proxy.
- Taylor-model branch-and-bound over the shared hierarchy.

### 13.5 Scene axes

- Flat through strongly curved source surfaces.
- Fine through coarse triangle proxies.
- Low through high obliquity and shell distortion.
- Thin through thick residual displacement.
- Smooth ramps, sparse features, rocks, and high-frequency/noise maps.
- Front-facing through grazing rays.
- Static maps and local/global displacement edits.

### 13.6 Reported quantities

- Query time and full-render time.
- Resident and build-scratch memory.
- Preprocessing and incremental update time.
- Node fetches, cells visited, leaves tested, segments, tube width, and fallbacks.
- GPU registers, occupancy, cache hit rates, and bandwidth.
- Bound tightness by hierarchy level.
- Hit error and false-hit/miss rates.
- Area-integral error, PDF normalization, variance, and sampling efficiency.
- Quality–time, quality–memory, and combined-workload Pareto plots.

## 14. Work plan

### Phase 0 — freeze contracts and build oracles

- Review and freeze the proposed analytic shell image of piecewise-affine texture microtriangles; use it for ray hits, metrics, area, and sampling.
- Add a dense independent ray oracle.
- Add dense numerical area/metric integration.
- Establish watertight edge and atlas conventions.
- Record current Mode 0/Mode 1 correctness and performance before changes.

**Deliverable:** reproducible correctness and timing harness.

**Current state:** the ray surface contract is frozen; the initial exhaustive analytic oracle and independent 80-digit cubic-root cross-check exist. Area/metric oracles, complete watertight conventions, broader saved-seed validation, and recorded GPU baselines remain.

### Phase 1 — CPU Taylor hierarchy and tightness gate

- Implement the exact conservative two-microtriangle leaf model specified in [[Plan — Ray application method, prototype, and baselines]].
- Implement and test the conservative fold.
- Recover min/max and validate enclosure.
- Propagate to slab and area-density bounds.
- Run H1 across representative and adversarial maps.

**Deliverable:** bound-tightness report and go/no-go decision.

**Current state:** the exact two-microtriangle leaf, quadrant-aware conservative fold, recovered min/max, dense enclosure validation, and initial procedural counts exist in Python. The representative H1 corpus and area-density propagation remain.

### Phase 2 — cheapest ray-integration test

- Add the packed four-component ray view.
- Add Taylor-slab rejection to the current Mode 1 traversal.
- Keep current segmentation initially to isolate the bound effect.
- Profile H2, including bytes and occupancy rather than only cell counts.

**Deliverable:** min/max-versus-slab ray ablation.

### Phase 3 — certified curved-ray adapter

- Derive texture-coordinate monotonicity events.
- Replace midpoint-only subdivision with a conservative curve bound.
- Implement tube/supercover traversal and conservative fallbacks.
- Remove silent segment, stack, and step truncations.
- Validate against the dense oracle before optimizing.

**Deliverable:** zero-miss certified ray application and H3 decision.

### Phase 4 — area-sampling application

- Validate metric and area-density formulas pointwise.
- Implement hierarchical area sampling first.
- Add emission-product sampling only after area-only correctness.
- Return and numerically validate exact PDFs.
- Run H4 against specialized sampling structures.

**Deliverable:** correctness, variance, memory, and update comparison.

### Phase 5 — unified layout and combined workload

- Choose packed formats and conservative quantization.
- Share build/update work between applications.
- Measure simultaneous ray-tracing and light-sampling workloads.
- Run H5.

**Deliverable:** combined system and central Pareto result.

### Phase 6 — conditional certified closest-point projection

**Hard prerequisite:** begin no work in this phase until Phase 5 is complete and the ray, sampling, and shared-system gates H2–H5 have passed. “Mostly working” does not satisfy this prerequisite; both initial applications need validated correctness and a defensible performance/memory result.

- Derive a conservative world-space distance lower bound from a Taylor node and the triangle shell map.
- Implement branch-and-bound with an exact leaf closest-point solve.
- Validate certificates and closest-point results against the independent oracle.
- Compare pruning and performance against a tessellated triangle BVH and a zero-order hierarchy.
- If H6 passes, decide whether closest point fits the paper page/time budget.
- Only then price a conservative local-feature-size bound and a possible minimal PWoS demonstration.

**Deliverable:** a go/no-go result for closest point. Full PWoS is not a deliverable of this phase.

### Phase 7 — optional traversal-aware proxy construction

Only begin this phase if the hierarchy and both initial applications survive their gates. Schedule it relative to closest point using expected paper value per week; neither optional phase may delay completion and evaluation of Applications A and B.

- Generate candidate triangle shells under explicit representation-error constraints.
- Measure obliquity, integrability defect, shell conditioning, residual thickness, and Taylor remainders.
- Test whether these quantities predict segment count, cell visits, slab tightness, and runtime.
- Optimize proxy choice using the predictive subset.

**Deliverable:** either a real end-to-end representation contribution or a documented negative result kept out of the main claims.

### Current progress record — 2026-08-16

#### Completed in the active scope

- Removed Catmull–Clark from the active representation, method, and claims. The runtime surface is a general analytic triangle shell carrying a fixed piecewise-affine texture microtriangulation.
- Froze the ray surface contract and derived the rational shell ray $q(h)=(U/D,V/D,h)$, proxy/ray/turning events, exact cubic leaf equation, and singular fallback policy.
- Implemented `scripts/prototype_first_order_ray.py` as a pure Python/NumPy CPU reference.
- Implemented the exact first-order leaf, quadrant-aware minimax fold, recovered min/max view, and dense hierarchy-enclosure validation.
- Implemented Bernstein-bounded second-derivative curve tubes, adaptive segmentation without a silent cap, recursive tube-aware min/max traversal, and recursive Taylor-slab traversal.
- Added exhaustive candidate-leaf superset checks and closest-hit comparison against every analytic microtriangle.
- Added an independent 80-digit `Decimal` derivative-partition root solver for cubic-oracle cross-validation.
- Added grid-line, grid-corner, microtriangle-diagonal, proxy-edge, on-surface, inside-shell, grazing, randomized, UV-turning, isolated tangency, multiple-root, multiple-hit, and denominator-near-range cases.
- Added an exact independent-height conservative AABB, a Bernstein-enclosed first-order correlated AABB, proxy-footprint-aware box traversal, and dense per-level box-enclosure validation to isolate the relationship to TFDM-style conservative boxes.
- Added `scripts/first_order_ray_core.py` as a stable notebook-facing API without moving or duplicating the certified predicates.
- Added `scripts/first_order_ray_figures.py`, an executed `notebooks/first_order_ray_visualization.ipynb`, a pinned Conda/OpenBLAS environment, and a one-command PowerShell runner. The notebook exports four initial figure families in PDF, SVG, and PNG.
- Recorded complete failing rays and segment data in JSON when a mismatch occurs.
- Kept existing CUDA paths untouched; no GPU result is being inferred from the CPU reference.

#### Evidence obtained so far

- The current default suite covers 384 ray/case pairs across ramp, smooth, and checker maps on flat and twisted shells: zero candidate omissions, zero hit mismatches, certified sampled tube coverage, and the expected singular fallbacks.
- The default suite independently checks 6,144 leaf polynomials through the 80-digit root path with zero discrepancy.
- A seven-map post-fix corpus covers 448 rays over constant, ramp, smooth, smoothed-noise, impulse, checker, and noise maps: zero traversal failures and zero discrepancies across 7,168 independently checked leaf polynomials.
- A stricter $0.01$-texel tube target exercised adaptive subdivision, reaching 2.44 mean segments per active noisy/twisted ray in the measured case without a mismatch.
- Initial CPU work counts show roughly 16–18% fewer node visits on ramps and roughly 20% fewer leaf tests on smooth maps. Checkerboards tie min/max. Noise can visit slightly more internal nodes while still reaching fewer leaves.
- A 640-ray TFDM-box relationship ablation has zero enclosure violations and candidate omissions. First-order AABB volume is about 30% smaller on a twisted ramp, but its coarse smooth/noisy boxes can be 1.4–2.9× larger. Corresponding box-traversal gains are small or negative, while the direct Taylor slab retains the clearer ramp/smooth leaf-work benefit. This supports the direct slab—not a generic first-order AABB—as the active ray hypothesis.

- The executed notebook's five-map, 24-ray-per-map mechanism sweep has zero candidate omissions and hit mismatches. A clean execution takes about 21 seconds; the post-refactor default 384-ray/6,144-high-precision-polynomial regression also remains at zero failures.

These results support the mechanism but do not establish GPU speed. They also show the regime dependence expected from the formulation: the plane is valuable when slope is predictable and the residual is small; high-frequency disagreement makes the first-order node degrade toward or occasionally cost more than min/max.

#### In progress before any CUDA authorization

- Replace the fixed floating-point event-root ownership pad with explicit isolating intervals.
- Expand the high-precision check over saved randomized seeds and all adversarial families.
- Run per-level bound/work sweeps on selected existing procedural shells and real displacement-map crops.
- Persist reproducible CSV/JSON tables and automatic minimized failure plots.
- Complete the focused prior-art audit for centered-form/Taylor displacement pyramids, gradient hierarchies, and certified curved-ray traversal.
- Decide H1 and H3 from distributions, not aggregate examples.

#### Not started by design

- CUDA Taylor-node storage, packing, traversal, and timing.
- Full tube-supercover hierarchical DDA on the GPU.
- Application B metric/area oracle and sampler.
- Conditional closest-point or PWoS implementation.

The next implementation decision is deliberately narrow: only after the remaining Python and literature gates pass should Phase 2 port the four-component ray view and slab test while holding the ray segmentation fixed. That ablation must establish whether tighter rejection survives extra fetch bandwidth before porting the complete certified curved-ray adapter.

## 15. First feasibility checkpoint

Before expanding the project, answer four questions:

1. Are Taylor slabs materially tighter than min/max on the intended assets?
2. Does tighter rejection improve ray time after the larger fetch cost?
3. Can conservative curve tubes remain narrow enough for useful DDA traversal?
4. Do joint metric bounds improve area sampling relative to a specialized scalar hierarchy?

The combined paper should be committed only after at least the first three are positive and the fourth shows either a performance, memory, or update advantage.

### Third-application scheduling gate

Closest-point projection is considered only after the two-application system is complete:

- **Ray completion:** certified hit correctness, no silent traversal truncation, H2/H3 results, and the intended baseline comparison.
- **Sampling completion:** validated density/PDF, H4 results, and the intended specialized baselines.
- **Framework completion:** shared layout/build/update path and H5 combined-workload measurement.

Until these conditions hold, closest point and PWoS remain notes and derivations only; they do not receive implementation or evaluation time.

## 16. Tentative paper organization

1. Introduction and the zero-order limitation of displacement hierarchies.
2. Related work.
3. Surface and query model.
4. Joint Taylor-model hierarchy and conservative fold.
5. Derived slab, normal, metric, and area bounds.
6. Application A: certified ray intersection.
7. Application B: area/product sampling.
8. Optional Application C: certified closest point, only if H6 passes and the page budget permits it.
9. Implementation and packed GPU representation.
10. Evaluation and ablations.
11. Limitations and future queries.

Likely supplemental material:

- fold proof;
- affine/interval propagation details;
- rational-curve and tube derivations;
- watertightness conventions;
- complete correctness stress tests.

## 17. Working titles

- *Conservative First-Order Queries on Displacement Maps without Tessellation*
- *A First-Order Displacement Hierarchy for Visibility and Surface Measure*
- *Beyond Min–Max: Conservative Visibility and Measure Queries on Displaced Surfaces*

Avoid “intrinsic geometry” in the title if ray intersection remains a headline application, because visibility is not an intrinsic metric query.

## 18. Open questions

- Is the six-value node novel in this exact displacement-pyramid setting after a focused literature search?
- Can the ray view be packed into one texture fetch without unacceptable quantization widening?
- Should hierarchy traversal be DDA, best-first, or a ray-span-dependent hybrid?
- What is the tightest inexpensive tube bound for the rational shell curve?
- Can the metric determinant be bounded tightly enough without expensive affine-form expansion at query time?
- Is area sampling the strongest second application, or does a different query make more essential use of the gradient remainders?
- What edit model best exposes the advantage of a shared factorized hierarchy over precomputed final scalar distributions?
- Should the exact represented leaf be bilinear height over the shell or two planar displaced microtriangles?
- Should traversal-aware proxy construction remain future work, or does it later produce a separate publishable result?
- Can a node's shell-space Taylor slab produce a sufficiently tight certified world-space distance lower bound for closest-point pruning?
- If closest point succeeds, can local feature size be bounded conservatively without introducing a second expensive acceleration structure?
- Does a third application strengthen the framework story enough to justify its implementation and page cost after the first two are complete?

## 19. Immediate next actions

1. Treat the completed representation oracle as the final no-go for optimized planes and finite affine support-strip dictionaries on the fixed square min/max-mipmap topology.
2. Do not implement `K=2/4` dictionaries, new first-order ray payloads, full-resolution replay, or a GPU port of this representation.
3. Treat A0 run `ray-a0-smoke-d9bbfd971b2f` as the completed harness/readiness gate, not as a performance result.
4. Treat A1.0 and all current-binary C1–C3 runs recorded in [[Plan — A1 GPU mathematical port and differential tests]] Section 16 as passing isolated mathematical checkpoints only. C3c.1 confirms no range-bin inflation from the recorded C3c.0 coefficient widening; C3c.2 finds no practical parent `t` turn.
5. `C8-CHORD-CERTIFICATE` through `C13.1b-TIGHT-EQUATION-BRACKETS` pass; see [[Plan — A1 GPU mathematical port and differential tests]] Sections 18–32. Execute only the frozen C13.2 native-FP64 versus certified compensated-FP32 cost decision next. Keep `t` turns disabled by default. The 4,096-bin scans and exhaustive C10 loop remain correctness scaffolding, not final runtime solvers or timing evidence; do not change Mode 1 or implement the optimized/full `CERT-DDA-MM` traversal before its ordered gates.
6. A2 is complete and its frozen performance gate failed despite exact correctness. Preserve `ray-a2-4-timing-d9125676ceb0` as the signed negative/mixed result; do not weaken the gate or proceed to A3/A4 paper timing. Resume the area/product-sampling application next.
7. S0–S2 pass. Execute only items 1–3 of [[Plan — S3 area sampling quality and cost decision]] next. Full-corpus variance, product sampling, and GPU timing remain blocked until deterministic development variances agree with the audited Monte Carlo trials.
7. Keep RMIP, PDM, dense-triangle/DMM comparison, closest point, PWoS, and adaptive/nonconvex ray bounds deferred until their preceding gates pass.

Do not begin closest-point or PWoS implementation during these actions. Preserve only the derivations and experiment sketch needed to revisit H6 after Phase 5.

## 20. Decision log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-16 | Initialize a combined project around the joint first-order hierarchy. | Ray tracing can consume the local height plane and remainder as conservative slab bounds, while area sampling consumes the induced metric bounds. |
| 2026-08-16 | Reframe from “metric queries” to “conservative first-order queries.” | Area is intrinsic/metric; ray visibility is not. The broader name describes both honestly. |
| 2026-08-16 | Limit the initial applications to ray intersection and area/product sampling. | Two deep applications give a clearer paper than importing every metric-query application. |
| 2026-08-16 | Keep traversal-aware proxy construction optional until the common hierarchy passes its gates. | It is expensive and should not precede evidence that the central representation works. |
| 2026-08-16 | Add certified closest-point projection as a conditional third application. | It coherently reuses the same conservative node enclosures and branch-and-bound machinery. |
| 2026-08-16 | Finish ray intersection and area/product sampling before starting the third application. | The time limit favors completing and validating the two-application paper before expanding its scope. |
| 2026-08-16 | Keep full PWoS beyond a second gate. | Closest point alone is insufficient; PWoS also needs a conservative local-feature-size or valid-tube bound. |
| 2026-08-16 | Plan the ray application around the analytic shell image of piecewise-affine texture microtriangles. | It makes the exact Ogaki-style cubic leaf and the induced metric refer to the same mathematical surface; current Mode 1 planar world microtriangles become an approximation ablation. |
| 2026-08-16 | Use a Python correctness/tightness prototype before modifying CUDA. | The curve-tube and Taylor-slab proofs can be falsified quickly against exhaustive exact leaf roots. |
| 2026-08-16 | Remove Catmull–Clark from the active method and claims. | The hierarchy and all planned queries operate on general triangle shells; source-surface conversion is a separate problem. |
| 2026-08-16 | Complete the initial Python ray reference and keep CUDA gated. | Initial safety/work-count results and the first independent high-precision checks are positive, but broader saved-seed/asset H1/H3 evidence is still missing. |
| 2026-08-16 | Use the quadrant-aware minimax parent fold. | Child-relative residuals are affine, so exact child-corner ranges are conservative and materially tighter than a full-parent triangle-inequality bound. |
| 2026-08-16 | Define the first-order ray role as surface rejection, not ray approximation. | Certified segments bound nonlinear-ray error; the Taylor plane removes predictable displacement slope. The tube-widened slab composes the two without conflating them. |
| 2026-08-16 | Reject “segments plus DDA” as the standalone novelty claim. | Piecewise curved-ray approximation, DDA, cubic leaves, min/max pyramids, and affine/Taylor arithmetic all have prior art in isolation. |
| 2026-08-16 | Frame performance as regime-dependent and evaluate a combined Pareto claim. | RMIP, PDM, dense triangles, and DMM are strong specialized competitors; the central opportunity is shared visibility and measure support, not universal fastest ray tracing. |
| 2026-08-16 | Treat TFDM conservative box estimation as direct prior art. | TFDM already propagates UV, base position, normals, and min/max height through affine arithmetic to conservative AABBs; our candidate extension is stored height–UV correlation, gradient bounds, and the direct tube–slab consumer. |
| 2026-08-16 | Keep the direct tube–slab test as the ray hypothesis, not first-order AABBs. | The CPU ablation shrinks ramp boxes but often widens coarse smooth/noisy boxes; direct surface-aligned slabs show the more consistent pruning opportunity. |
| 2026-08-17 | Keep the notebook as a thin explanatory layer over the command-line correctness harness. | This gives reproducible paper-oriented figures without duplicating certified algorithms or relying on hidden notebook state; CPU figures remain mechanism evidence until the GPU/baseline gates pass. |
| 2026-08-23 | Do not authorize the GPU ray port after the first frozen 45-case decision run. | Active rays are exact and first-order slabs reduce coherent exact tests by 30.9%, but only 9/27 coherent cases pass the predeclared static-tightness gate and ordinary denominator-root fallback is 2.61–2.93%. |
| 2026-08-23 | Make complete plane-compensated hierarchical DDA traversal—not bound tightness—the next ray decision. | The first-order plane can turn a certified shell-ray chord into an affine residual ray and conservatively clip node parameter intervals; only measured end-to-end traversal can show whether the extra representation cost is worthwhile. |
| 2026-08-24 | Stop the present first-order ray-acceleration branch after P-D2.0c. | The exact full-corpus replay passes correctness, but coherent FO/HYB leaf ratios are `0.9674/0.9622` and projected overhead budgets are negative even before payload cost. P-D2.1, the GPU layout replay, and external ray performance baselines are not justified for this formulation. |
| 2026-08-24 | Initially make the area/metric application the next project gate. | The ray method remains a certified tessellation-free query contribution, but the current single-plane representation does not establish first-order performance value. |
| 2026-08-24 | Defer area briefly for a bounded optimal first-order oracle audit. | The stored parent slope is only an arithmetic mean of child slopes, not an optimal fit. An upper-bound study can distinguish a weak fitting policy from a fundamental lack of first-order ray headroom without committing to GPU work. |
| 2026-08-24 | Stop affine first-order ray-representation work after the full oracle. | An optimized hybrid retains `0.9611×` coherent leaves, while an ideal convex support hull retains `0.9548×`; both have negative break-even margins and fail their frozen gates. Better plane fitting and small slope dictionaries cannot supply the missing broad coherent headroom on this topology. |
| 2026-08-24 | Finish the ray application through a separate scalar architecture gate before returning to area. | The certified piecewise shell-ray plus min/max-DDA architecture was never isolated from the failed first-order predicate. A same-surface Ogaki-style ablation and quality-matched TFDM comparison can test it without reopening the affine-support no-go. |
| 2026-08-24 | Pass A0 and keep the result infrastructure-only. | Both TFDM GPU probes pass, the source correspondence is recorded, all 1,000 NRTDSM timing samples are present, and trial CVs are below 0.4%; however, the certified same-surface candidate does not exist yet, so the smoke timing ratio is not scientific evidence. |
| 2026-08-25 | Stop the scalar ray-performance branch after A2. | The complete packed-surface GPU candidate is exact and strongly wins front rays, but fails the frozen held-out ordinary, moderate/stress-shell, and grazing timing gates against same-buffer `NRT-QT`. Under the predeclared protocol, OptiX integration and TFDM paper timing are blocked; return to area/product sampling rather than selecting favorable ray cohorts. |

## 21. Frozen Step 1 contribution conclusion — 2026-08-23

We now have a defensible technical conclusion, but not yet a paper-level performance conclusion.

**Supported:** the conservative first-order hierarchy, its tube-widened slab predicate, and exact analytic leaf composition form a coherent ray-query method. On the frozen CPU reference corpus, every active traversal reproduces the exhaustive oracle. With the same certified ray segments and exact leaves, the first-order predicate reduces exact leaf/segment tests by `24.3%` over all 45 cases and `30.9%` over the coherent 27-case cohort; coherent node tests fall by `10.3%`. Four analytic/procedural maps, the synthetic file-backed R00 map, and three practical Poly Haven maps pass the per-map work criterion.

**Not supported:** the broad representation claim that first-order residual widths are routinely at least 25% tighter than scalar min/max at useful visited levels. Only `9/27` coherent cases pass that frozen condition, principally the two planar maps and the sine bump. The traversal can still reject more because the tilted plane preserves directional height–UV correlation even when the scalar residual interval itself is not narrower; that is a narrower, ray-specific mechanism claim.

**Blocking:** the current segment builder falls back on any denominator root in the full height range. Ordinary fallback is `169/5760 = 2.93%` without grazing and `207/7920 = 2.61%` with grazing, so G1/G3 fail despite zero observed misses. Segment counts themselves are small (`p99=4`, `max=6` over all ordinary active rays).

**Current decision:** do not claim superiority over TFDM, Ogaki, RMIP, PDM, triangles, or DMM, and do not begin a GPU performance port yet. The next ray task, if retained, is a preplanned validity-aware singularity partition followed by the same frozen-corpus rerun. Separately, the static-tightness no-go must remain disclosed. The area/product-sampling application is still essential: it decides whether the shared first-order framework has enough breadth for an EG paper even if ray performance remains conditional.

The paper-facing continuation is now frozen in [[Plan — Practical first-order DDA ray traversal]]. It replaces residual-width percentage as the final call with correctness-matched full traversal time. Its candidate algorithm is residual-space slab clipping inside a conservative tube-supercover hierarchical DDA; the current recursive CPU counts remain only pre-port evidence.

Safe paper language now:

> Our CPU reference establishes conservative composition of certified rational shell-ray tubes with a stored first-order displacement slab. At identical segmentation and exact leaves, the slab reduces candidate work on a frozen mix of procedural and real maps, while exposing two limitations: scalar residual intervals are not broadly tighter than min–max, and the current full-range singularity fallback is overly conservative. GPU performance and external-baseline superiority remain open.

## 22. P-D2.0c project decision — 2026-08-24

P-D1 now provides the positive ray result: a certified nonlinear triangle-shell ray, tube-supercover traversal with closed ownership, and exact cubic leaves reproduce the exhaustive represented-surface oracle across the 864-case practical corpus. P-D2.0c provides the negative representation-performance result: the present first-order and hybrid residual clips save less than four percent of coherent leaf work and fail the frozen predictive break-even test.

The earlier Step 1 reductions were real but not representative of the complete downstream traversal distribution. They were concentrated in selected maps and ray regimes where min/max left removable work. The complete corpus contains many front-facing and grid-corner cases already near irreducible one-leaf behavior, so a more expensive first-order node has almost no room to win. Boundary-stress and near-miss reductions remain useful explanatory evidence, not a basis for selecting the favorable cases after the gate.

The current technical-contribution assessment is therefore:

- **Ray application:** sufficiently substantial as a methods/correctness contribution if written around certified nonlinear shell-space traversal and exact represented leaves; not supported as a first-order acceleration result.
- **First-order hierarchy:** conservative and reusable in principle, but its paper-level practical value is not yet established.
- **Area/metric application:** still decisive for the shared paper, but temporarily deferred until the bounded optimal-plane/support-envelope ray audit finishes.
- **Paper viability:** unresolved. A strong area result can yield a coherent shared-query paper whose ray consumer contributes certified visibility and whose area consumer demonstrates the first-order performance benefit. A weak area result means the umbrella should be narrowed rather than adding a third application to compensate.

Superseding safe paper language:

> We derive and validate a tessellation-free triangle-proxy shell intersection method based on certified piecewise enclosures, closed hierarchy ownership, and exact cubic leaves. The current first-order residual clip preserves correctness but fails our predeclared coherent-work gate; we therefore do not claim ray-speed superiority from it and evaluate the hierarchy's practical value through surface-measure sampling.

## 23. Representation-oracle project decision — 2026-08-24

The full upper-bound run `p-d2-r0-05a8b7b3f3c4` and report `p-d2-r0-report-af2d251f38cb` close the final planned attempt to rescue first-order ray clipping by changing the stored plane. All eight variants remain exact on 864 cases. Optimized fitting substantially tightens median static residual and tube widths, confirming that the arithmetic-average slope was not optimal. That improvement does not translate into useful coherent traversal reduction: `HYB-OPT1` retains `96.11%` of scalar leaf work, and the zero-facet-cost support-hull oracle retains `95.48%`.

This makes the causal conclusion sharper than P-D2 alone. The weak ray result is not merely an artifact of the current average slope; the fixed hierarchy has too little removable coherent work for any convex affine support representation over the same node domains. Stress, grazing, and near-miss rays do benefit, but a specialized accelerator for those regimes would be a new project hypothesis.

The paper contribution ledger is now:

- **Positive:** certified nonlinear triangle-proxy shell traversal, closed tube/event ownership, and exact cubic leaves without displacement-resolution tessellation.
- **Negative but informative:** conservative first-order affine clipping—including optimized and idealized support variants—does not justify a broad ray-acceleration claim on the frozen corpus.
- **Still open:** whether the scalar min/max-DDA architecture is competitive with Ogaki/TFDM, and whether first-order metric information decisively benefits area/product sampling.

Do not write that first order accelerates ray tracing. It is safe to write that an optimal-plane and convex-support upper-bound ablation rules out plane selection as the missing performance mechanism for this topology.

The next ray work is defined in [[Plan — Certified ray architecture comparison]]. It asks whether certified tube-supercover min/max DDA is practically valuable without first-order clipping. This is a new architectural hypothesis with new gates, not a reinterpretation of P-D2. The current heuristic Mode 1 benchmark remains non-authoritative until the candidate targets the exact shell-space cubic surface and passes the P-D1.6 differential oracle.

---

## 24. S3 full project decision — 2026-08-25

The area branch now has a complete causal chain. S0 establishes the exact surface differential and area oracle. S1 establishes conservative and leaf-tight first-order area caps. S2 establishes correct proposal/null/repeat probability semantics. S3 shows that these facts do not imply a practical shared sampler: local cap decisions produce a poor global distribution, while the repaired leaf-CDF needs per-instance scalar weights and loses the specialized p90, dense-quality, and storage comparisons.

The current contribution ledger is therefore:

- **Supported:** a certified nonlinear triangle-shell ray method; exact cubic represented leaves; exact metric/area evaluation; conservative first-order height/gradient and area-cap composition; correct sampling PDFs and a useful bound-versus-distribution ablation.
- **Rejected by frozen gates:** broad first-order ray acceleration, same-surface certified DDA speed superiority, shared first-order area-sampling advantage, and the combined two-query Pareto thesis.
- **Intentionally stopped:** product sampling, closest point, area GPU work, and conditional external area baselines.

This is enough theory and negative evidence for a strong dissertation chapter or methods/limitations report. It is not presently a convincing positive Eurographics full-paper performance story. Continuing toward EG requires a new, planned hypothesis with a plausible advantage over a specialized baseline; it should not merely add applications to the existing hierarchy.

## 25. Post-A2 hybrid upper-bound decision — 2026-08-25

R0 run `ray-r0-hybrid-oracle-3dd5b9a1a015` closes the last bounded rescue attempt for the current ray architecture. A perfect zero-cost family dispatcher would choose certified DDA only for front rays and NRT for the other ordinary families. Its bootstrap median is `0.9182x` NRT with only `4.738 ns/ray` savings, failing the predeclared `0.90x`, `8 ns/ray`, and two-family gates before any dispatch cost.

The project now has no authorized performance branch. First-order clipping, certified DDA alone, shared area sampling, the area leaf-CDF repair, and the ray hybrid have each failed their frozen comparator gate. External baseline ports would no longer answer a live positive hypothesis. Further work should begin only after formulating a materially new method whose advantage can be shown by a cheap upper-bound study.

Related: [[Plan — Ray application method, prototype, and baselines]] · [[Project — Conservative metric queries without tessellation]] · [[Taylor-model bound pyramid]] · [[The induced metric of a displaced surface]] · [[Obliquity and the integrability defect]] · [[Surface measure and sampling on implicit displaced surfaces]] · [[Min-max mipmap and conservative bounds]]

## 26. Topology-preserving shell-ray DDA decision — 2026-08-30

The separately planned zero-radius topology hypothesis is complete at T0; see [[Plan — Topology-preserving shell-ray DDA]]. The full run `ray-topology-t0-d891da21b944` passes every correctness gate across 864 practical rays and 54 analytic case/resolution pairs. Practical intervals require p50/p95/p99/max `1/2/3/3` topology-preserving chords, with a median of five crossed cells per chord. This supports compact shell-space segmentation, exact front-to-back event ordering, and ordinary DDA cell ownership without carrying a traversal-time UV radius.

It does not reopen the ray performance branch. Among the 149 practical segments where the old tube admits extra cells, the topology representation removes `14.29%` at the median, below the frozen `25%` continuation threshold. T1 runtime-certificate design and T2 CUDA integration are therefore stopped. The contribution ledger gains a clean discrete segmentation/correctness result, not a supported acceleration result.

Current paper boundary:

- topology-preserving shell-ray segmentation is a technically valid possible method component;
- no result yet shows that its runtime certificate is cheaper than NRT-QT's shared nonlinear node equations;
- no claim against TFDM, RMIP, PDM, dense triangles, or DMM is authorized; and
- the existing negative conclusion for the shared first-order two-query EG thesis is unchanged.

## 27. Separator-sign runtime-certificate decision — 2026-08-30

[[Plan — T1 separator-sign shell-ray certificate]] tests the remaining per-cell-cost hypothesis after topology T0. The full run `ray-separator-t1-c8ca43b64c6c` passes every correctness gate with zero practical fallback and zero required leaf omission. Direct leaf DDA uses `6025` structural min/max fetches versus A2 NRT-QT's `34055`, a strong `0.1769×` opportunity count.

The complete frozen continuation gate nevertheless fails. Practical p95 segmentation is `7` rather than `≤4`, separator sign predicates are `0.3349×` NRT node visits rather than `≤0.25×`, and near-miss min/max survivors are `1.6404×` NRT leaf candidates rather than `≤1.50×`. Pooled survivors remain close at `1.0688×`, showing that the weakness is localized rather than a universal candidate explosion.

The project therefore gains a positive root-free topology-certificate derivation and a useful causal diagnosis, but no new performance claim. A future event-bracketing method must be posed as a new hypothesis that decouples height tightness from segmentation; it cannot reinterpret this T1 failure.

## 28. T1.5 optimistic GPU-envelope decision — 2026-08-30

The raw segment-count gate was deliberately challenged with a measured necessary-condition experiment; see [[Plan — T1.5 measured separator-DDA performance]]. Run `ray-t15-envelope-f619b4ad5265` replays CPU-certified T1 leaf/height schedules in a CUDA kernel, performs leaf min/max rejection and the common exact cubic leaf tests, and compares against same-buffer NRT-QT. It excludes all schedule construction, `11561` sign predicates, and `768` splits, so it favors the candidate.

Correctness and measurement stability pass. All candidate/NRT hit-status checksums match, all outputs are regular, there are no T1 survivor omissions or practical fallbacks, and timing CVs are below `0.66%`. The result is mixed but fails the frozen continuation gate: held-out ordinary is `0.9557×` NRT rather than `≤0.80×`, S1-moderate is `1.0762×`, and oblique hits are `1.7789×`. S2-stress (`0.7845×`) and front hits (`0.3118×`) confirm real favorable regimes, not a general advantage.

The full on-device separator/segmentation port remains stopped. However, T1.5 merged cells lexicographically and enumerated every hit, so it did not test the front-to-back early termination enabled by topology-preserving DDA. The closest-hit question is reopened—without weakening any prior gate—under [[Plan — T1.6 ordered closest-hit shell DDA]]. Its first requirement is a CPU oracle that measures ideal versus separator-delayed early exit before another CUDA implementation.

## 29. Ordered closest-hit O0 decision — 2026-08-30

[[Plan — T1.6 ordered closest-hit shell DDA]] corrects the T1.5 query contract by preserving certified DDA order and applying conservative closest-hit termination. Run `ray-ordered-o0-972a8b4bd571` passes all closest-hit, closed-owner, reverse-order, and miss-ray correctness checks across 864 practical rays.

The correction validates the core intuition but rejects the current bounds. On 144 held-out ordinary hit rays, exact T0 event spans would reduce exact leaf-triangle tests from `592` to `320` (`0.5405×`), establishing large front-to-back potential. Current separator-derived spans reduce them only to `550` (`0.9291×`) and retain `15.4%` of the ideal saving. Overlapping brackets delay the frontier by a median two and p95 four leaf-triangle tests relative to the ideal schedule. Front and oblique families save no leaf work; grazing saves 15.5% and near-miss 5.1%.

Therefore, ordered CUDA traversal is not authorized with the current separator spans. The precise remaining ray hypothesis is event-local height contraction—not more DDA engineering. A derivative-bound or one-step interval-Newton bracket can be tested cheaply against the same ideal limit before any device port. Until that test passes, the ray contribution remains certified topology and a causal bound/termination analysis rather than supported acceleration.

## 30. Event-local contraction decision — 2026-08-30

The separately frozen O0.5 run `ray-event-o05-d1747e69d975` repairs the bound that stopped O0. One interval-Newton step contracts 4,777 certified event brackets with only 48 uncontracted/empty events, zero independently audited Decimal-root omission, median width ratio `0.0001754`, and p95 `0.0062387`. On held-out ordinary hits, `IN1` reduces exact leaf tests from `592` to `320` (`0.54054×`) and exactly matches the IDEAL event-span work, retaining all ideal early-termination saving.

This does not reverse the negative conclusion for first-order displacement clipping: `IN1` is a ray-event mechanism built from the rational shell ray and grid boundary polynomial. It does show that topology-preserving segmentation plus constant event-local contraction can make front-to-back DDA materially sharper than the original separator-only schedule. O1 ordered CUDA timing is authorized.

## 31. Ordered closest-hit CUDA envelope decision — 2026-08-30

Run `ray-o1-closest-b35fd77510db` is the first positive same-surface GPU result for the corrected closest-hit query. Across all frozen cohorts, both ordered IN1 schedules and a fair front-to-back NRT-QT baseline match Mode 0 closest-hit checksums with zero unresolved/overflow output. On held-out ordinary rays, the candidate takes `56.15 ns/ray` versus `104.75 ns/ray` (`0.5360×`, `1.87×` faster). S1 and S2 ratios are `0.5784×` and `0.5201×`; every held-out ordinary family is below NRT, including grazing at `0.7249×`.

The result changes the ray outlook but not yet the final paper claim. O1 excludes segmentation, event construction, interval-Newton contraction, and schedule storage/transfer from candidate timing. It proves that ordered contracted shell-space DDA has sufficient GPU headroom to justify paying those costs; it does not prove that the full method retains the advantage. The next gate is a full on-device method with explicit construction-cost ablations and fallbacks. External TFDM/RMIP/Ogaki comparisons and renderer-level claims remain blocked until that gate passes.

## 32. O2.1 device interval-Newton decision — 2026-08-30

Run `ray-o21-in1-6e70fd371f28` ports event-local `IN1` to directed binary64 CUDA arithmetic and passes all differential gates over 4,777 event groups. It has zero Decimal-root omission, zero CPU-bracket disjointness, exact fallback agreement, and only negligible outward width inflation (p95 `1.000000000046×`, maximum `1.000000481×`). The same 48 closed corner groups remain uncontracted.

The port exposed and fixed an important boundary policy. Approximate proportionality of two binary64 boundary polynomials is not an exact simultaneous-root proof; a one-ulp overlap between their Newton images can exclude both nearby roots if intersected. The device method therefore retains the original bracket for multi-axis closed groups and applies tight `IN1` only to single-axis events. This preserves all owners and matches the Decimal reference.

O2.2 device chord-event and separator-stream construction is now authorized. Full end-to-end performance and external-baseline claims remain pending.
