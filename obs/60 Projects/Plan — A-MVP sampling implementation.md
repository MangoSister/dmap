---
title: Plan — A-MVP sampling implementation
tags: [project, plan, phase2, sampling]
created: 2026-08-27
---

# Plan — A-MVP sampling implementation

Implementation plan for the A-MVP of [[Project — Conservative metric queries without tessellation]] (§5A, Phase 2): product sampling of emissive displaced surfaces, in C++ on the ks codebase (`code/cpp`). Like the master plan, this note is stateless and edited in place; history and reasoning go to [[Log — Conservative metric queries]]. Based on the code investigations logged 2026-08-27.

## Status

- **All phases S0–S8 complete (2026-08-28).** S0–S6 validators pass (log); S7 measurements in [[Result — A-MVP receiver irradiance study]]; S8 images and MIS check in [[Result — A-MVP rendered images]]. Headline results: far-field ladder ordering as predicted; product descent ties the product table at amplitude 0.05 and concedes 1.3–3.1× at 0.2 (coarse-level midpoint √det G, not the floor); the midpoint emitter cosine is harmful and the receiver-aware variant without it is the best sampler near-field, under occlusion, and in both rendered viewpoints; per footprint the table is ~19× smaller than the pyramid, while build and edit costs favor the hierarchy at any reuse; NEE, BSDF-only, and MIS renders converge to the same image through the tessellation-free light. The certified-propagation sub-step of S3 stays deferred. Next: the Phase 2 gate discussion (master plan §7), and the open decisions below for whatever the gate keeps.

## Goal

One displaced emissive asset, four samplers, and the measurements that decide the gate:

1. Uniform area — Ling et al. line casting (baseline, run first).
2. Emission-only — image distribution over the uv domain, PBRT-v4 style.
3. Area-only descent — pyramid weights from √det G alone.
4. Product descent — pyramid weights from emission × √det G.

Deliverables: a receiver irradiance variance study at equal sample counts (the quantitative evidence), and rendered direct-lighting images (the demo seed for the gate).

## Ground rules

- dmap code lives in `code/cpp/src`, never inside the ks submodule.
- The numpy reference (`code/python/poc`) is the correctness oracle. Golden data moves as `.npy` files: a small export script on the Python side writes them, the C++ side loads them (the npy library is already linked).
- Every phase ends with a validation task registered in `main.cpp`, driven by a toml config in `code/data/configs`, that answers one question and prints its verdict.
- All experiments are seeded and deterministic.
- Core math in double for now; float32 storage is a later cost question, priced when the numbers matter (conservativeness under float32 needs outward rounding, deferred).
- Chart convention as in the Python reference: identity chart per face, whole tile per triangle. Generalizing the parameterization is not part of the A-MVP.

## Phases

Each phase is independently testable. Dependencies are listed per phase; S1–S2, S4, and S5 are mutually independent once S0 is done.

### S0 — Build sanity

Build the scaffold, run the stub `test_weighted_area_sampling` task from its config. No new code beyond fixes needed to build clean.

- Verify: the task runs and logs; embree device creation works.

### S1 — Pointwise displaced-surface evaluation

Port the pointwise math from `dmapref` (`metric.py`, the interpolant conventions of `dense_reference.py`): displacement texture wrapper with bilinear h and analytic ∇h, base-triangle forms (P, interpolated unit normal, G0), S(u, v), G, √det G, surface normal.

- Files: `src/displaced_surface.h/.cpp`.
- Depends on: S0.
- Verify: task `validate_pointwise` — compare h, ∇h, G entries, √det G, and the normal at a seeded grid of (u, v) against golden `.npy` from the oracle, on the Phase 1 assets (cc_torus, spot × rock, cobble) at both amplitudes. Tolerance set by float64-to-float64 agreement (~1e-12 relative if both sides are double).

### S2 — Pre-tessellated displaced mesh

A builder that evaluates S on the texel grid per face and emits a ks `MeshData` (positions, texcoords, normals). This is the truth geometry and the visibility substrate; the samplers never intersect the tessellation-free representation.

- Files: `src/displaced_tessellation.h/.cpp`; wire as a config asset or a task-side builder.
- Depends on: S1.
- Verify: task `validate_tessellation` — summed triangle area converges to the metric area integral as resolution refines, matching the Phase 0 experiment 2 numbers (~0.3–1.3% at 0.05×edge at leaf scale); export `.obj` for visual inspection.

### S3 — Taylor pyramid core

Port `TaylorPyramid` (exact bilinear leaf, interval-hull fold) and the emission mipmap (per-cell mean and max of the emission map). Propagation ports only what sampling needs: the midpoint √det G estimate per cell, and optionally the certified √det G interval. The full affine ∩ interval propagation (`affine.py`, `node_bounds.py`) is needed only for the certified-weight variant and for B/C later; port it as a separate sub-step, not as a blocker.

- Files: `src/taylor_pyramid.h/.cpp`, `src/emission_pyramid.h/.cpp`.
- Depends on: S0 (build), S1 (base forms for propagation).
- Verify: task `validate_pyramid` — node arrays at every level match golden `.npy` from `dmapref` exactly (same fold order, double math); a conservativeness spot check (dense samples inside random cells at every level stay within the node's h and gradient bounds).

### S4 — Ling baseline

The line sampler (uniform sphere direction, uniform offset in the perpendicular square over the asset's bounding box, slab clipping), embree all-hits casting via an intersection filter over the S2 mesh, and the estimators: area, ∫E dA, and irradiance at a receiver.

- Files: `src/line_sampling.h/.cpp`.
- Depends on: S0; S2 for displaced targets.
- Verify: task `validate_line_sampling` — three checks in order: (1) the estimator constant on a flat quad of known area and on an icosphere (this catches the offset-domain and line-double-counting factors); (2) displaced-surface area converges to the S2 triangle-area sum at the expected 1/√M rate; (3) uniformity by total-variation distance between per-triangle hit counts and per-triangle areas, the paper's own metric.

### S5 — Bilinear patch port

`BilinearPatchMesh` and `BilinearPatch` in dmap style: area Sample/PDF with a `DistribTable2D` emission distribution and the exact pointwise Jacobian pdf; embree geometry as two triangles (flat rectangles only; the GARP intersector only if non-planar patches become necessary).

- Files: `src/bilinear_patch.h/.cpp`.
- Depends on: S0.
- Verify: task `validate_bilinear_patch` — sample histogram matches the emission image (total-variation distance on a grid); Monte Carlo estimate of ∫E dA through the pdf matches the direct texel sum; on a constant-emission rectangle the pdf is 1/area.

### S6 — Descent samplers

Hierarchical sample warping over the pyramid, with three weight variants (analysis 2026-08-28, log):

1. **Area-only**: weight = √det G midpoint estimate per child.
2. **Product**: emission mean × √det G midpoint estimate.
3. **Product × geometry term** (receiver-aware): variant 2 times cosθ_r⁺/r² estimated at the child's spatial center (midpoint height along the center normal). The emitter cosine is a toggle (`emitter_cosine`) and S7 measured it as harmful: a midpoint-normal point estimate misweights rough cells at coarse levels, so leave it off until a cone-aware bound from the certified propagation exists. Weights must be deterministic functions of (node, receiver) so the pdf query can re-walk them.

All variants add an ε floor so every emissive cell has positive probability. Descend with one categorical draw per level; sample (u, v) inside the leaf; pdf_uv is the product of branch probabilities over the leaf cell area; the exact area pdf is pdf_uv / √det G(u, v) via S1. The emission-only sampler (master plan baseline 2) is a direct `DistribTable2D` over the emission map — the honest PBRT-style baseline — and the same table machinery over per-texel E·√det G gives the **product table** (master plan baseline 5), the non-hierarchical adversary. Certified-zero pruning and certified weights are out of S6's scope: they need the certified propagation port (S3's deferred sub-step) and join in Phase 3 unless the gate wants them earlier.

- Files: `src/descent_sampler.h/.cpp`.
- Depends on: S1, S3.
- Verify: task `validate_descent` — four checks: (1) Monte Carlo integrals of several known integrands through the pdf agree with dense-quadrature references (unbiasedness), for every variant including receiver-aware at a few receiver positions; (2) sample histogram against pdf on a coarse grid; (3) on a flat patch with an emission texture, the product sampler's distribution matches the S5 sampler; (4) sample-side pdf equals query-side pdf re-walked from (u, v) (the MIS contract), exactly.

### S7 — Receiver irradiance study (the A-MVP experiment)

`test_weighted_area_sampling` becomes the experiment: receivers on a sphere of directions at several distances (near field to far field), unoccluded irradiance from the emissive displaced surface, relative MSE versus sample count at equal sample counts against a converged reference, for the ladder: uniform (Ling), emission-only table, product table, area-only descent, product descent, receiver-aware descent. Sweep texture × amplitude. One occluded configuration (shadow rays against the S2 mesh) to confirm the ordering survives visibility. Equal-time comparisons matter for the receiver-aware variant (its weights cost ~5–10× a table's binary search per sample); report both.

Expected shape, stated before measuring (log 2026-08-28): uniform < emission-only < {product table ≈ product descent} in the far field; receiver-aware descent pulls ahead in the near field; a product descent notably worse than the product table is a bug in the weights, not a finding (Phase 1 tightness at 1–8-texel cells says the midpoint model tracks the true product closely).

Alongside variance, the cost table against the product table baseline: build time and memory per unique (triangle, tile) pair as the scene defines them, a tile-reuse sweep (1×, 10×, 100× faces per tile), and rebuild cost per amplitude/geometry edit (the pyramid's is zero). The whole-tile-per-triangle convention must not inflate these numbers; the honest accounting is per footprint.

- Depends on: S4, S5, S6.
- Verify: built into the experiment — every estimator must agree with the reference within confidence intervals (any disagreement is a correctness bug, not a finding). The variance ordering and the cost table are the result, recorded in [[Result — A-MVP receiver irradiance study]].
- Recorded, not yet implemented — per-node weight caching: bake the branch probabilities for the receiver-independent variants (which collapses their per-sample cost to table-speed lookups), and bake the receiver-independent node data (midpoint √det G, spatial center, midpoint normal, emission sum) for the receiver-aware variant, leaving only the per-receiver arithmetic in the sample loop. The S7 equal-time numbers carry the no-caching caveat: the measured 6–8× draw-cost gap against the tables is mostly this, not the hierarchy.

### S8 — Emitter light and rendered images (gate demo seed)

A displaced-emitter `Light` implementation (descent sample → solid-angle pdf conversion; pdf-of-direction via the hit's (u, v) on the S2 visibility mesh, mirroring the mesh-light lookup), integrated into `small_pt` with MIS. Two or three viewpoints, equal-sample MSE and FLIP against a converged reference, product descent versus the baselines.

- Depends on: S2, S6, S7 (for trusted samplers).
- Verify: NEE-only, BSDF-sampling-only, and MIS renders converge to the same image (the standard MIS correctness check); then the comparison images are the deliverable. Done: [[Result — A-MVP rendered images]]. The light lives in `src/displaced_emitter_light.{h,cpp}` and mirrors the ks light contract; folding it into ks's `LightSampler`/`small_pt` proper is engine integration, deferred until a rendering deliverable needs multi-light scenes.

## Dependency summary

S0 → S1 → S2, S1 → S3 (with S3's certified-propagation sub-step optional), S0 → S4 (S2 for displaced targets), S0 → S5, S1+S3 → S6, S4+S5+S6 → S7, S2+S6 → S8. S4 and S5 can proceed in parallel with S1–S3.

## Open decisions (settle when reached, record in the log)

- Leaf-level (u, v) sampling rule inside the chosen cell: uniform, or bilinear warp by corner weights. Start uniform; the leaf is one texel.
- Whether certified weights and certified-zero pruning (safe subtree skipping proved by certified spatial bounds or cones; see the log, 2026-08-28) join the ladder or wait for Phase 3. Either way they require porting the certified propagation (S3's deferred sub-step). S7 added a second consumer: the emitter-cosine factor needs a cone-aware bound to be useful at all.
- Level-thresholded emitter cosine (recorded 2026-08-28, not implemented): apply the midpoint-cosine factor only at fine levels (cells of ~1–4 texels), where Phase 1's cone data says the normal spread is small and a midpoint normal is a fair summary; drop it at coarse levels, where S7 measured it as harmful. Cheap to add (replace the `emitter_cosine` bool with a level threshold; weights stay deterministic, pdf re-walk unchanged) and cheap to measure with the S7 harness. Expected win: the per-texel cosine variation far-field on rough content, which no receiver-independent sampler can target; expected null result at amplitude 0.05. Sweep the threshold over levels 1–3 — if even level 3 hurts, the cone-aware bound is needed sooner.
- Coarse-level product weights: the midpoint √det G composition concedes 1.3–3.1× variance to the product table at amplitude 0.2 (S7). The **mechanism is now measured** ([[Result — Pyramid channel study]]): the midpoint model builds ∇h∇hᵀ from a single gradient and so discards the cell's gradient covariance, which is exactly its roughness, and it therefore underestimates the cell's true mean √det G in every cell from 16-texel cells up — by up to 2× on cobble at amplitude 0.2, with the bias tracking the S7 gap monotonically across all three configurations. Three candidates:
    - **Moment channels (recorded 2026-08-30, not implemented).** Every term of the metric formula is at most quadratic in (h, ∇h), so E[G] is exact from seven base-independent moments — E[h], E[h²], E[∇h], E[∇h∇hᵀ] — which fold by averaging, exactly, with no compounding. Measured, they cut the estimator error from 2× low to at most 1.43× high, the overshoot guaranteed in sign by concavity of √det on the positive definite cone. They beat the midpoint from 8-texel cells up and overshoot below that on rough content, so the better estimator is level-dependent, and moments buy nothing at all below 4-texel cells. Unlike the fused bake, this keeps base independence, so tile reuse, per-instance amplitude and free edits all survive. Costs up to seven channels, on the structure whose per-footprint memory is already the weak column.
    - **Per-node sums of per-texel E·√det G** (the fused bake, per-face). Tightest possible, and the only candidate that also captures the correlation between emission and slope, which moments do not: the weight multiplies an emission integral by an area estimate, where the true target is the integral of the product. Surrenders base independence.
    - **Certified interval midpoints.** Untested, and unlike the two above it has no argument behind it — the interval is 3–10× wide at coarse cells, so its midpoint is dominated by looseness rather than by the true mean. Cheap to test with `node_bounds.py` before investing.
    - **Ruled out:** using the cell's true mean gradient instead of the gradient-hull midpoint. Measured, it leaves rock unchanged and makes cobble worse. The problem is the nonlinearity, not the slope.
  Note that estimator error is not variance. Whether any of these closes the S7 gap needs the S7 harness, which is the cheap next measurement.
- Descent variance under direct pyramid construction (deferred 2026-08-30, not measured). `DescentSampler` now takes a `PyramidBuild`, so both constructions can be measured with no further code; the fold stays the default. Direct enumeration changes only `h0` among the channels the sampler reads, and nesting bounds that change by `r_fold - r_direct`, measured at a tenth of its bound; the leaf allocation cannot move at all, since the last descent branch reads level-0 nodes, which are identical in both builds. So the expected answer is no measurable change in variance, and exp09 says it cannot close the coarse-weight gap either, since that gap is discarded gradient covariance and the gradient channels are identical. Worth one S7 rerun to confirm, not worth blocking on.
- Per-node weight caching (S7 section above): implement before any equal-time claim goes in the paper.
- float32 storage with outward rounding: now has measured stakes — the pyramid is ~24× the table's memory per footprint in double, ~12× in float32 (S7 cost table, remeasured 2026-08-30 under the eight-channel node: 383 KB against 16 KB). Two levers cut against that and should be priced together: precision per channel family (bound channels need outward rounding, estimate channels do not), and storing a channel only at the levels where it pays, since level 0 alone holds three quarters of all nodes.

Related: [[Project — Conservative metric queries without tessellation]] · [[Log — Conservative metric queries]] · [[Result — Phase 1 Taylor pyramid]] · [[Taylor-model bound pyramid]]
