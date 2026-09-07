---
title: Plan — R1 full-renderer validation of ordered shell DDA
tags: [plan, ray-tracing, renderer, OptiX, DDA, Mode-6]
status: r1-archived-mode6-removed
created: 2026-08-30
updated: 2026-08-30
parent: "[[Plan — O2 on-device ordered shell DDA]]"
---

# Plan — R1 full-renderer validation of ordered shell DDA

> [!abstract] Purpose
> O2.4d measured a complete *isolated candidate-triangle intersection query*, not an end-to-end displaced-mesh renderer. R1 integrates the validated O2 query into `nrtdsm`, preserves the existing Mode 0 and Mode 1 paths, and measures both intersection work and whole-frame rendering under matched scene, camera, sampling, material, and lighting conditions.

## 1. Frozen method identities

No existing mode is overwritten.

| Mode | Frozen role | Implementation identity |
|---|---|---|
| 0 | primary represented-surface reference and internal baseline | Ogaki-style nonlinear rational-ray min/max quadtree traversal with exact nonlinear microtriangle leaves |
| 1 | preserved historical/evolved linear-segment renderer path | adaptive certified chord tubes, tube-supercover anchors, hierarchical min/max descent, exact nonlinear microtriangle leaves, and its existing retry/fallback policy |
| 5 | preserved P9 diagnostic | incidence-routed one/two certified chord tubes with Mode-0 fallback |
| 6 | new R1 candidate | O2 topology-preserving piecewise segments, closed grid events, uncertainty-aware event contraction, ordered leaf DDA, exact nonlinear microtriangle leaves, and certified closest-hit termination |

`-intersection-mode 1` must continue selecting the same specialized OptiX entries and `detailedSurface_generic` implementation it selects before R1. Mode 1 source is not refactored as part of R1 unless a build-only, behavior-preserving declaration is unavoidable; any such edit requires a before/after Mode-1 image hash and timing smoke test.

## 2. What changes from Mode 1 to Mode 6

Mode 6 is not “Mode 1 with a different threshold.” The ray, surface, and exact leaf representation remain shared, but the traversal certificate changes.

| Question | Mode 1 | Mode 6 / O2 |
|---|---|---|
| Why split the shell curve? | reduce a geometric chord-tube radius until capacity/tightness rules accept it | certify that the rational curve and chord have the same ordered grid-crossing topology; split at a failed separator |
| What encloses approximation error? | an explicit componentwise UV tube radius | coefficient-uncertainty-expanded event intervals and closed owner sets; no geometric tube is used by the O2 leaf schedule |
| How are cells generated? | centerline DDA supplies coarse anchors; a tube-expanded hierarchical supercover descends independently | exact ordered grid-boundary events generate a closed leaf-cell stream directly |
| How is min/max used? | at multiple mip levels during hierarchy descent | leaf min/max rejects cells already selected by ordered DDA; hierarchy is not credited for O2 ordering |
| Leaf intersection | exact nonlinear ray versus the same two represented microtriangles | exact nonlinear ray versus the same two represented microtriangles |
| Ordering/termination | maintains a closest hit, but tube/hierarchy order is not the O2 suffix certificate | monotone world-`t` segment order plus conservative suffix lower bounds permits certified front-to-back termination |
| Failure policy | existing Mode-1 retry/coalescing/exhaustive policies | whole-candidate Mode-0 fallback on unsupported span, pole/turn ambiguity, certificate refusal, capacity, or nonfinite arithmetic |

The comparison therefore isolates a traversal architecture while holding the triangle proxy, shell map, displacement lattice, and exact leaf surface fixed.

## 3. R1.0 architecture audit result

The renderer already has method-specialized G-buffer, closest-ray, and visibility-ray OptiX entries for Modes 0, 1, 4, and 5. Mode 1 currently enters `detailedSurface_generic`; Mode 5 enters `detailedSurface_bounded_piecewise`. O2.4 is currently a standalone CUDA replay and cannot be called directly from the renderer because it assumes a packed 65×65 `A2Window`, fixed local coordinates, and diagnostic output structs.

The renderer port must therefore reuse O2 mathematics, not its host/replay storage:

- build the same packed shell/ray values from the live OptiX query;
- choose a wrapped 64-cell local window and transform proxy texture coordinates into O2 local coordinates;
- read the live displacement texture/min-max data on demand instead of copying 4,225 height samples per candidate;
- translate local O2 cell owners back to wrapped global texel owners; and
- report the hit through the existing displaced-surface OptiX attribute contract.

## 4. R1.1 dispatch and preservation gate

Add `IntersectionMode_OrderedShellDDA = 6` and dedicated Mode-6 entry names for:

- G-buffer primary displacement/shell mapped surfaces;
- path-tracing closest displacement/shell mapped surfaces; and
- path-tracing visibility displacement/shell mapped surfaces.

The dedicated entries are a GPU resource boundary. They must not add Mode-6 registers/local state to Mode 0 or Mode 1. The initial entry may call Mode 0 as a scaffold, but it must be labeled scaffold and cannot produce a Mode-6 result or timing claim until the O2 device routine is connected.

R1.1 passes when the renderer builds, Modes 0/1 retain their selected program names, and Mode 6 renders through its own program names. Record relevant OptiX/NVCC resource reports where available.

## 5. R1.2 correctness-first device port

### 5.1 Supported fast domain

The first port accepts a candidate only when:

- the displacement map is square and power-of-two;
- the wrapped proxy span fits a 64×64 leaf window on both axes;
- the live height range and ray interval are finite and nonempty;
- O2 constructs one supported monotone component (the currently validated O2.4 scope); and
- all fixed event/segment/cell capacities pass.

Every refusal executes Mode 0 for that candidate and increments a categorized fallback counter. This restriction matches the packed O2/P8 domain rather than silently generalizing it. Later work may remove the 64-cell window after the renderer differential passes.

### 5.2 Surface and cell identity

For a local window origin `(x0,y0)`, live texture coordinates are converted by

\[
u_L=(uW-x_0)/64,\qquad v_L=(vH-y_0)/64,
\]

with wrap-aware unwrapping. O2 cell `(i,j)` maps to global lattice corners `(x0+i,y0+j)` through the renderer's existing wrap convention. Fetch the four level-zero heights used by Mode 0/Mode 1 and test the same diagonal split and exact nonlinear microtriangle routine. Canonical ownership is `(global y, global x, local triangle)` after wrap normalization.

### 5.3 Traversal and termination

Port the passing O2.4c sequence without semantic shortcuts:

1. robust dominant-axis rational coefficient construction with directed coefficient enclosures;
2. supported/monotone height interval construction;
3. separator-certified adaptive segmentation;
4. grouped closed grid events and outward `IN1` contraction/inflation;
5. unique leaf cells in front-to-back order;
6. live leaf min/max rejection and exact two-microtriangle tests; and
7. strict conservative suffix world-`t` early termination.

Visibility rays may stop at the first certified occluder only after the closest-hit implementation passes. Until then, visibility uses the same closest-hit result contract.

## 6. R1.3 renderer correctness experiment

Use Mode 0 as the operational reference. Mode 1 is shown as a preserved diagnostic, not used to define truth.

For every frozen geometry/displacement case, render matched primary geometry AOVs first:

- hit/miss mask;
- object/world position or ray `t`;
- proxy barycentrics/UV;
- geometric and shading normal; and
- canonical owner/debug ID when available.

Then render matched path-traced beauty images with the same camera, environment map, material, resolution, SPP, seed policy, and bounce limit. Include at least flat/curved/twisted proxy geometries and smooth, high-frequency, directional, stochastic, and seam-stressing displacement maps already present in the practical gallery/test set.

Correctness gates:

- zero systematic seam or crack pixels;
- zero unsupported Mode-6 result that bypasses Mode-0 fallback;
- exact hit/miss agreement except explicitly audited boundary-tie pixels;
- finite hit coordinates/normals for every Mode-6 hit;
- AOV error within the existing Mode-0/represented-surface tolerances; and
- a saved per-case Mode-0/Mode-1/Mode-6 side-by-side PNG folder.

Any mismatch is debugged at the first-hit/AOV level before beauty rendering or timing.

## 7. R1.4 matched performance experiment

Measure two scopes separately:

1. **Intersection scope:** GPU time and counters for candidate-prism intersection work, split by primary, closest, and visibility ray families.
2. **Whole-frame scope:** G-buffer plus complete path-tracing frame time/FPS at equal resolution, SPP, bounce depth, materials, and environment lighting.

For Modes 0, 1, and 6 record:

- frame time distribution after warmup, median and p95;
- candidate-prism invocations and fallbacks by reason;
- segments, events, DDA cells, min/max fetches, exact leaf/root tests, and early stops;
- primary/closest/visibility launch-ray counts;
- registers, local bytes, stack/continuation-stack settings where reported; and
- image/AOV hashes proving the timed configuration matches the correctness configuration.

Use at least 20 warmup frames and 50 measured paired trials with deterministic randomized method order. Report intersection time and whole-frame time; neither may be substituted for the other.

## 8. Interpretation and go/no-go rules

R1 answers a question that O2.4d did not: whether the method is practically competitive inside the full displaced-mesh renderer. It does not erase the isolated-query result.

- If Mode 6 is faster than Mode 0 in both intersection scope and whole-frame median on multiple practical cases with no correctness regression, authorize broader map/scene expansion and external TFDM/RMIP comparison.
- If intersection is slower but whole-frame is statistically tied, retain O2 as a correctness/architecture result and report the cost as amortized, not accelerated.
- If Mode 6 is slower in both scopes, the ray-performance conclusion is negative; do not search for favorable SPP/shading configurations to hide intersection cost.
- Mode 1 remains reported separately so readers can see whether topology-event segmentation improves on the older tube-supercover route even when neither beats Mode 0.

No universal speed claim is allowed from a subset of rays, an uploaded schedule, or a replay-only kernel.

## 9. Implementation order

1. R1.1: add isolated Mode-6 enum, dispatch, and Mode-0 scaffold; build and prove Mode-1 preservation.
2. Extract the O2 device-only coefficient/interval/event core into a renderer-safe header while keeping the standalone differential executable building.
3. Add live local-window mapping and a differential that compares live inputs/cells/hits against captured P8/O2 records.
4. Connect exact live leaf tests and closest-hit reporting; pass captured-query differential.
5. Run full renderer AOV correctness and produce comparison PNGs.
6. Enable closest-hit suffix termination, then visibility early-out; rerun correctness.
7. Run intersection and whole-frame timing only after all previous gates pass.

## 10. Immediate draft boundary

The O2.4d numbers must be described as *isolated candidate-triangle query timing*. They are predictive evidence that event construction is expensive, not a measured renderer FPS conclusion. Until R1.4 completes, write neither “the full renderer is slower” nor “renderer integration amortizes the cost.”

## 11. R1.2 live-adapter checkpoint (frozen before implementation)

The renderer adapter will not relax the validated O2 grid domain. The O2.4 event stream assumes a canonical lower-left triangular 64×64 window (`i+j<=64`). For each live proxy, the adapter:

1. unwraps each texture-coordinate axis relative to vertex A, exactly as the P8 packer does;
2. selects an integer 64-cell window origin that contains the three proxy vertices;
3. tries the four axis-reflection combinations and accepts one only when all transformed vertices satisfy `u_L>=0`, `v_L>=0`, and `u_L+v_L<=1` within a binary32-scale tolerance;
4. stores the reflection bits so O2 cell owners map back to the correct global wrapped texels; and
5. falls back for every noncanonical proxy instead of allowing O2's triangular owner filter to discard cells.

The first connected version is deliberately **no-stop**: it traverses every ordered O2 cell, applies live level-zero min/max rejection, and calls the renderer's existing exact nonlinear two-microtriangle test. Suffix early termination is enabled only after no-stop Mode 6 agrees with Mode 0 on captured queries and renderer AOVs. This separates cell-enumeration correctness from termination correctness.

The shared O2 schedule receives `(heightMinimum,heightMaximum)` directly; it never allocates or copies the standalone 65×65 `A2Window` in an OptiX invocation. Its fixed 5,072-byte diagnostic schedule is acceptable for the correctness checkpoint but is explicitly temporary: after the differential passes, schedule materialization must be replaced by a compact/streaming renderer form before performance timing.

### 11.1 First live differential

On `curved_surface × disp_cobble.png` at 1920×1080, the initial no-stop port differed from Mode 0 at 2 of 2,073,600 primary pixels; all 550,224 common hits were bit-exact in world position. A diagnostic that bypassed only the leaf min/max comparison removed Mode 6's one false negative. The other differing pixel is a hit in Mode 1, Mode 6, and the dense triangle oracle, so it is an audited Mode-0 boundary false negative rather than a Mode-6 miss.

The production comparison therefore restores leaf min/max with a directed `64*FLT_EPSILON*max(1,|h|)` outward pad. This pad covers the distinct binary32 live-window/reflection and texture-extrema arithmetic paths; it does not enlarge the DDA owner set. The full differential must be rerun with the diagnostic switch off.

The correctness scaffold's first resource report is intentionally not a performance result: the displacement Mode-6 closest/visibility entries use 7,904 bytes of direct stack and 688 bytes of direct spills, versus 376/228 bytes for Mode 0. The materialized 5,072-byte `OutputB` schedule must be removed before R1 timing.

### 11.2 Compact-schedule checkpoint

The renderer now uses a 1,584-byte `CompactSchedule` instead of O2's 5,072-byte diagnostic `OutputB`. It retains the ordered 64-cell stream and aggregate counters but does not retain 48 full event records. The implementation reuses O2's interval construction, grouping, simultaneous-event proof, separator signs, interval-Newton contraction, uncertainty inflation, closed-owner enumeration, and adaptive split decisions; only the diagnostic materialization is removed. The standalone O2 fused/full probes continue to compile from their original output contract.

On all four frozen 1920x1080 one-spp AOV cases, compact Mode 6 is byte-identical in both geometry and shading-normal buffers to the preceding materialized no-stop Mode 6:

| Case | Geometry | Shading normal |
|---|---:|---:|
| flat quad / brick | exact | exact |
| twisted quad / rock | exact | exact |
| curved surface / cobble | exact | exact |
| sphere / terrain | exact | exact |

For the displacement closest/visibility entries, direct stack falls from 7,904 B to 5,184 B. Direct spills change from 688 B to 732 B, so this is a necessary but incomplete resource recovery. A single uncached-comparison curved/cobble smoke frame changes from 1,182.6 ms to 884.9 ms total (path 740.8 to 536.3 ms). These are one-frame engineering observations, not R1.4 timings; compact Mode 6 remains far slower than the current Mode-0 smoke frame before early stopping.

## 12. R1.2 suffix-termination plan (frozen before implementation)

The closest-hit optimization must use the already validated O2.4c condition, not merely stop at the first DDA-ordered leaf. Grid-event order is curve order, but a cell can contain an exact surface root anywhere inside its certified height interval. Therefore, after an exact hit at world-ray parameter `t_hit`, Mode 6 may stop only if every remaining cell has conservative lower bound strictly greater than `t_hit` plus tolerance.

For a remaining cell height interval `H`, bound

\[
t(H)=\frac{T(H)}{\|d_r\|^2 D(H)},
\]

where `T` is the cubic world-ray numerator, `D` is the quadratic shell denominator, and `d_r` is the object-space ray direction. Directed interval Horner evaluation and interval division produce the cell lower bound. If the denominator interval touches zero, the bound is `-infinity`; this disables stopping rather than risking a miss.

Implementation policy:

1. keep the compact ordered cell schedule and exact nonlinear leaf tests unchanged;
2. evaluate the minimum lower bound over the unvisited suffix only after a leaf produces a new closest hit, avoiding a persistent 65-double suffix array in the OptiX stack frame;
3. stop only when `suffixLower > t_hit + 64*DBL_EPSILON*max(1,abs(t_hit))`;
4. count scanned suffix cells and accepted early stops in profiling builds;
5. first enable this for primary/closest queries while visibility still follows the same closest-hit contract; and
6. require byte identity against compact no-stop Mode 6 on all four frozen AOVs before enabling a visibility-specific first-occluder exit.

Resource acceptance is measured as well as correctness: if reconstructing the robust bound context or scanning the suffix increases stack/spills enough to erase traversal savings, retain no-stop as the reference and redesign the bound evaluation rather than accepting an unmeasured optimization.

### 12.1 Robust-rescan result and rejection

The first implementation rebuilt `O24RobustContext` after each improved hit and interval-bounded every remaining cell. It is byte-identical to compact no-stop Mode 6 on all four frozen geometry and shading-normal AOVs, but fails the resource and timing gates:

- closest Mode-6 stack/spills rise from `5,184/732 B` to `5,416/820 B`;
- curved/cobble one-frame G-buffer time rises from `302.8` to `465.8 ms`;
- primary rays skip about `0.27` cells/invocation after evaluating about `2.56` cell bounds/invocation; and
- closest rays skip about `0.92` cells/invocation after evaluating about `1.14` cell bounds/invocation.

The early stop occurs on approximately `2.8%` of primary and `7.4%` of closest candidate invocations. This version is retained only as a correctness diagnostic and is not the production Mode-6 termination path.

### 12.2 Monotonic-frontier replacement plan

O2 already certifies one world-`t`-monotone height component and stores its sign. Therefore the earliest possible root in the unvisited cell union does not require an interval quotient over every cell:

- for increasing `t(h)`, let `h_frontier` be the minimum lower height of every unvisited cell;
- for decreasing `t(h)` traversed in reverse height order, let `h_frontier` be the maximum upper height; and
- the suffix lower bound is a directed interval evaluation of `t(h_frontier)`.

The compact schedule will retain only the robust coefficient intervals for cubic `T`, quadratic `D`, the ray-length-squared interval, and the monotonic sign (136 B total). After an improved exact hit, traversal scans only scalar cell endpoints, evaluates one interval rational frontier, and applies the same strict tolerance. A denominator interval touching zero disables the stop. This preserves the O2.4c conservative condition while replacing robust-context reconstruction plus many polynomial interval evaluations by one endpoint evaluation.

Acceptance gates remain: exact AOV identity to no-stop Mode 6 on all four cases, standalone O2 probes still build, stack/spills no worse than the rejected robust rescan, and an actual smoke-time reduction relative to compact no-stop on at least the curved/cobble target before visibility termination is enabled.

### 12.3 Monotonic-frontier result and production choice

The monotonic-frontier implementation is byte-identical to compact no-stop Mode 6 on curved/cobble, but also fails the resource/timing gate. Its curved/cobble one-frame G-buffer is `379.1 ms`, versus `302.8 ms` for compact no-stop. Carrying the 136-byte robust time context raises the visibility entry from `5,184/732 B` stack/spills to `5,448/860 B` even though visibility does not invoke the stop. The lower arithmetic cost is not enough to offset larger per-invocation state and the low stop incidence.

Production Mode 6 therefore returns to the 1,584-byte compact no-stop schedule for R1.4. It retains certified front-to-back cell order, but does not claim early termination. Both early-stop variants remain recorded as negative ablations. A future streaming generator could couple event generation and leaf testing without materializing the schedule; that is a new architecture experiment, not part of the frozen R1 comparison.

## 13. R1.4 matched full-renderer result

The frozen counter-free benchmark uses the RTX 5090, 1920x1080, 20 warmup frames, 50 measured GPU-event samples, deterministic randomized run order (`seed=20260830`), identical scene/camera/map/material settings, and separate Mode 0, Mode 1, and Mode 6 programs. Raw samples, logs, hashes, renders, and the generated report are in `.tmp/r1_full_benchmark/`.

| Case | Mode | Total p50 / p95 (ms) | G-buffer p50 / p95 (ms) | Path p50 / p95 (ms) |
|---|---:|---:|---:|---:|
| quad / brick | 0 | 8.067 / 8.290 | 1.552 / 1.590 | 6.254 / 6.434 |
|  | 1 | 170.566 / 291.767 | 11.008 / 11.534 | 159.388 / 280.227 |
|  | 6 | 7.249 / 7.471 | 1.636 / 1.806 | 5.308 / 5.506 |
| twisted / rock | 0 | 7.305 / 7.411 | 1.458 / 1.485 | 5.592 / 5.665 |
|  | 1 | 113.704 / 120.931 | 13.348 / 13.585 | 100.082 / 107.288 |
|  | 6 | 6.439 / 6.930 | 1.541 / 1.808 | 4.558 / 5.056 |
| curved / cobble | 0 | 23.422 / 23.711 | 3.552 / 3.601 | 19.603 / 19.810 |
|  | 1 | 134.266 / 135.328 | 24.770 / 25.513 | 109.073 / 110.061 |
|  | 6 | 598.520 / 603.876 | 124.795 / 127.504 | 473.574 / 478.293 |
| sphere / terrain | 0 | 5.217 / 5.607 | 1.491 / 1.657 | 3.431 / 3.793 |
|  | 1 | 58.798 / 61.854 | 12.937 / 15.356 | 44.716 / 48.394 |
|  | 6 | 164.675 / 171.748 | 57.733 / 62.766 | 106.271 / 112.158 |

The flat and twisted proxies exceed O2's supported 64-cell canonical live window and execute whole-candidate Mode-0 fallback. Their `0.90x` and `0.88x` total ratios are separate-process noise/layout controls, not O2 speedups. The active cases give the conclusion:

- curved/cobble Mode 6 is `25.55x` Mode 0 total, `35.13x` in G-buffer, and `24.16x` in path tracing;
- sphere/terrain Mode 6 is `31.57x` Mode 0 total, `38.73x` in G-buffer, and `30.98x` in path tracing;
- Mode 6 is also `4.46x` Mode 1 total on curved/cobble and `2.80x` on sphere/terrain; and
- active Mode-6 timing is stable enough for the negative conclusion (`0.66%` curved total CV and `2.46%` sphere total CV).

Mode 1 remains preserved. A final one-spp curved/cobble rerender is byte-identical to the pre-R1 Mode-1 artifact in both geometry (`084DA243...A410C9`) and shading-normal (`1E81BEA4...2AFE6`) AOVs. `-intersection-mode 1` still selects the historical certified tube-supercover implementation; `-intersection-mode 6` selects O2.

Correctness remains the earlier no-jitter AOV/oracle result: compact Mode 6 has no observed false negative in the four frozen 1080p cases; the few Mode-0 disagreements on curved/sphere boundaries were adjudicated in favor of Mode 1/Mode 6 by the dense represented-surface oracle. The benchmark's final jittered AOVs are retained but do not replace that controlled correctness audit.

### R1 claim boundary

R1 does **not** support a competitive ray-tracing performance claim for the current O2 renderer architecture. It establishes a working tessellation-free topology-event segmentation and ordered shell-space leaf-DDA path with exact nonlinear leaves, but schedule/event construction and GPU local state dominate. The external TFDM/RMIP performance comparison is not justified until a materially different streaming architecture closes this internal `25-32x` gap to Mode 0.

The appropriate paper use is a negative/diagnostic ablation if the ray application remains in the project. Do not present flat/twisted fallback timings as O2 wins, do not claim early termination, and do not generalize the isolated O2.4d candidate-query result to full rendering.

## 14. Renderer restoration after the R1 decision

After reviewing the negative full-renderer result, Mode 6 was removed from the active `nrtdsm` renderer on 2026-08-30. The removal covers its intersection-mode enum, OptiX entry points, hit-program registration and selection, stack-size accounting, renderer adapter, dedicated O2 renderer header, and Mode-6-only CMake diagnostic. `-intersection-mode 6` is now rejected explicitly instead of falling through to another implementation.

Mode 1 remains the default (`g_intersectionMode = 1`) and retains its specialized primary, closest, and visibility OptiX entries and certified tube-supercover implementation. A clean Release rebuild succeeds. A post-removal one-spp curved/cobble smoke render is byte-identical to the preserved pre-removal Mode-1 geometry (`084DA243...A410C9`) and shading-normal (`1E81BEA4...2AFE6`) AOV references.

The standalone O2 probes, R1 benchmark script, raw benchmark outputs, and this report remain as archived research evidence. They are not compiled into or selectable by the active renderer.
