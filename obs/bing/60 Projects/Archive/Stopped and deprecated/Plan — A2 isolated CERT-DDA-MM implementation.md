---
title: Plan — A2 isolated CERT-DDA-MM implementation
tags: [plan, ray-tracing, CUDA, DDA, minmax, baselines, triangle-proxy]
status: predictive-gate-failed-frozen
created: 2026-08-25
updated: 2026-08-25
parent: "[[Plan — Certified ray architecture comparison]]"
predecessor: "[[Plan — A1 GPU mathematical port and differential tests]]"
implementation-status: a2-0-through-a2-5-complete-predictive-gate-failed
---

# Plan — A2 isolated CERT-DDA-MM implementation

> [!abstract] Outcome
> Build the first end-to-end CUDA traversal that uses the corrected dominant-axis ray annihilators, certified lazy curve tubes, a scalar min/max hierarchy, incremental tube-supercover DDA, and exact represented leaves. Compare it on byte-identical ray/proxy/map buffers with an audited nonlinear quadtree (`NRT-QT`). A2 ends with a held-out predictive decision; it does not yet claim a full-renderer or external state-of-the-art result.

## 1. Frozen surface and arithmetic

The declared production surface is the packed binary32 triangle proxy, shell directions, texture coordinates, ray origin/direction, displacement samples, and fixed microtriangle diagonal. Host/device correctness evaluation may use binary64, but it may not substitute the older unquantized P-D1.6 surface.

Both methods use the same:

- dominant-axis ray-annihilator construction;
- supported shell interval and denominator/conditioning guard;
- affine texture mapping and scalar min/max pyramid;
- closed proxy/cell/triangle ownership rule;
- exact represented leaf cubic and four-equation world refinement;
- closest-hit and any-hit semantics; and
- explicit unresolved/fallback result.

The old normalized/rounded Frisvad frame is `FRAME-OLD`, an ablation only.

## 2. A2 method definitions

### 2.1 `CERT-DDA-MM`

For each ray/proxy candidate:

1. construct exact-annihilator `A,B,U,V,D` coefficients once;
2. partition the supported height interval at denominator, proxy-boundary, and required monotonic events;
3. generate accepted chord tubes lazily; subdivision and traversal share one work queue;
4. traverse every min/max hierarchy cell overlapped by the closed tube, retaining ties and point-only owners;
5. clip the inherited height interval against each node's scalar min/max range;
6. at an admitted fixed-diagonal leaf, isolate the exact annihilator cubic and refine/filter the packed world hit; and
7. group owners and select closest independently of discovery order.

No fixed segment cap, fixed DDA step cap, partial overflow result, zero-width chord, or planar world microtriangle is allowed. Resource pressure returns a conservative whole-candidate fallback.

### 2.2 `NRT-QT`

Use the same exact-annihilator curve and exact leaf. At each min/max quadtree node, conservatively bound the nonlinear `(u(h),v(h),h)` curve over the inherited height interval and test the child prism; order retained children by a conservative ray-time lower bound. This is the same-surface Ogaki-style architectural baseline. It must not use the historical Mode 0 leaf or unsafe nearest-side prism shortcut.

### 2.3 Control ablations

- `CERT-SCAN`: certified segments plus exhaustive closed leaf-cell scan;
- `CENTER-DDA`: identical incremental DDA with zero tube radius, correctness-failing diagnostic only;
- `FIXED-EPS`: independent fixed geometric curve flattening;
- `FRAME-OLD`: rounded-frame coefficients with the same traversal;
- `M1-HEUR`: historical implementation, clearly labeled; and
- `ARCH-HYB`: frozen pre-traversal dispatch between `CERT-DDA-MM` and `NRT-QT`, only after both base methods pass.

## 3. Ordered implementation and gates

### A2.0 — complete packed replay and oracle

Create a versioned replay containing stable IDs, packed proxy/ray data, the complete 64x64 practical displacement window, min/max pyramid, and ray interval for all 864 frozen tubes. Recompute the packed curve and supported height intervals from the packed inputs. Build an independent CPU oracle that partitions at exact curve/proxy/time/turn events, derives a certified complete UV cell footprint, exhaustively tests every fixed-diagonal microtriangle inside that footprint, and groups all hits canonically. A singular/unbounded footprint falls back to all proxy microtriangles.

**Gate:** deterministic replay; 48 windows and 864 ray/proxy records; complete packed oracle with no dependency on inherited P-D1 candidates; analytic front/oblique/grazing/grid-corner/proxy-edge/near-miss records; all hashes persisted. Original unquantized constructed-target retention is reported diagnostically because exact boundary hits can change under packing; it is not a gate for the different packed surface.

### A2.1 — simple end-to-end device correctness

Implement corrected coefficient setup, event partition, lazy certified tubes, and an intentionally simple exhaustive tube/cell enumerator followed by the exact C13.3b leaf. Keep full work queues and emit complete per-ray owner groups.

First execute `A2.1a-BRUTE-LEAF`, one CUDA thread per replay testing all 8,192 fixed-diagonal microtriangles with the exact annihilator leaf and world refinement. This bridge must match A2.0 before segmentation or cell enumeration is introduced. Then `A2.1b-CERT-SCAN` adds events/tubes and a simple exhaustive tube-overlapped cell scan while retaining the same leaf code.

**Gate:** exact equality with A2.0 group counts, owner sets, closest identity, and `2e-9` coordinate/residual thresholds; zero silent overflow; all fallbacks explicit. No timing claim.

### A2.2 — incremental hierarchical DDA

Replace only exhaustive cell enumeration with the production incremental hierarchy traversal. Use deterministic tie handling, conservative tube/slab clipping, inherited intervals, and a queue sized from A2.1 tails. A queue overflow returns the entire ray/proxy candidate to `NRT-QT`; it never returns partial hits.

**Gate:** outputs identical to A2.1/A2.0. Report segments, tube radii, nodes, min/max fetches, DDA cells, leaves, roots, fallbacks, queue high-water mark, and duplicates per ray/prism at p50/p95/p99/max.

### A2.3 — audited same-buffer `NRT-QT`

Implement or extract the nonlinear quadtree into the same standalone replay executable. Share setup, leaf solver, inputs, outputs, launch geometry, and instrumentation. Differential-test it against A2.0 before timing.

**Gate:** same packed oracle equality and no method-dependent represented surface.

### A2.4 — isolated GPU timing and predictive decision

Benchmark setup-only, traversal-only, and combined kernels with CUDA events, discarded warmups, randomized method order, at least 30 trials and at least 10 ms accumulated work per sample. Persist GPU/driver/compiler, registers, local memory, occupancy, clocks, checksums, and raw samples. Use coherent primary, shuffled/incoherent closest, and immediate any-hit buffers.

**Predictive gate on the held-out split:** `CERT-DDA-MM <= 0.85x NRT-QT` on medium/fine curved targets, no ordinary cohort above `1.25x`, p99 segments `<=8`, p99 DDA cells `<=128`, fallback below `0.1%`, timing CV below `5%`, and exact correctness. If the gate fails, freeze the negative result before renderer integration.

### A2.5 — hybrid decision

Only if adverse large-prism cases remain predictable, train a simple threshold/rule on the development split using only projected footprint, shell interval length, map resolution, and conditioning. Freeze it before held-out evaluation.

## 4. Dataset split and required reports

Split by complete `(asset, window, shell, ray-family)` groups. Never split individual rays from one group across development and decision. Boundary analytic rays belong to correctness/stress reporting, not dispatch training.

Required outputs:

- per-case and pooled correctness JSON;
- raw timing samples and median/IQR/CV;
- time, memory, and mechanism tables by asset/resolution/shell/ray family;
- segments/ray, cells/ray, leaves/ray, and setup-cost distributions;
- win/loss heatmap and worst 128 cases;
- `CERT-DDA-MM/NRT-QT` time ratio with bootstrap confidence interval;
- ablations for tube radius, hierarchy, lazy versus fixed segmentation, annihilator versus old frame, and exact refinement; and
- a signed go/no-go result that is not rewritten after seeing held-out timing.

## 5. Reuse and isolation boundaries

Reuse mathematical kernels from `ray_a1_math_probe.cu`, the native practical-map loader, min/max construction, and the audited Mode 0 traversal logic where its predicates match this contract. Create a separate A2 executable and mode IDs; do not patch historical Mode 1 in place. Only after A2 passes should the same code move into a new NRTDSM intersection mode and TFDM-normalized rendering harness.

## 6. Paper-writing checkpoint

Before A2 timing, the draft may describe the surface equation, exact ray-annihilator planes, certified tube invariant, closed ownership rule, exact leaf equation, packed correctness methodology, and frozen evaluation protocol. It must not yet say “faster,” “competitive,” or “state of the art.”

## 7. Executed result (2026-08-25)

All ordered correctness gates pass on the immutable packed replay:

- `A2.0`: `ray-a2-0-replay-144f75125df7`, 48 practical windows and 864 rays;
- `A2.1a`: `ray-a2-1a-brute-f5b98686339e`, all 7,077,888 represented leaf triangles;
- `A2.1b`: `ray-a2-1b-cert-scan-6c214364fdd2`, exhaustive certified tube-cell scan;
- `A2.2a`: `ray-a2-2a-cert-dda-70e15a492843`, incremental candidate-set equivalence;
- `A2.2b`: `ray-a2-2b-hier-dda-mm-f5c34f9b3b6d`, adaptive-start DDA plus min/max descent;
- `A2.3`: `ray-a2-3-nrt-qt-ef2448723ee6`, same-buffer interval `NRT-QT`; and
- `A2.5`: `ray-a2-5-hyb-stream-e8b2283abbb2`, streaming one-segment fast path with whole-candidate NRT fallback.

Every path reproduces the packed oracle's owners and closest hits within the frozen `2e-9` gate. The streaming candidate has zero fallback on this corpus, one segment for every ray, p99 40 min/max nodes, p99 26 exact microtriangle tests, and a maximum hierarchy queue of eight.

The frozen RTX 5090 timing run is `ray-a2-4-timing-d9125676ceb0`. It uses the immutable split manifest hash `0df21c493466e6524ed77e30466af0c03e6095a89f0fe8d9ca2793b1c76054f4`, 20 warmups, 50 randomized-order trials, and at least 10 ms accumulated kernel work per sample. The candidate's local-memory footprint fell from 10,144 B/thread in the materialized correctness scaffold to 880 B/thread in the streaming path; `NRT-QT` uses 848 B/thread.

| Frozen cohort | candidate / `NRT-QT` median | candidate ns/ray | `NRT-QT` ns/ray | Result |
|---|---:|---:|---:|---|
| all rays | 0.798 | 205.48 | 255.58 | candidate wins |
| decision all | 0.979 | 93.84 | 95.06 | approximately tied |
| decision ordinary | 1.261 | 99.04 | 80.37 | candidate loses |
| decision front | 0.383 | 11.56 | 30.86 | strong candidate win |
| decision oblique | 1.279 | 34.92 | 27.32 | candidate loses |
| decision grazing | 1.624 | 177.98 | 113.45 | strong candidate loss |
| decision near miss | 1.049 | 61.12 | 59.47 | approximately tied/slight loss |
| decision moderate shell | 1.457 | 89.29 | 60.02 | candidate loses |
| decision stress shell | 1.272 | 135.75 | 103.73 | candidate loses |

All checksum and correctness gates pass and every timing CV is below 5%, but the predeclared performance gate fails. Therefore A2 does **not** support a general claim that certified DDA is faster than nonlinear quadtree traversal. It supports the narrower causal statement that the architecture can be substantially faster in favorable front-facing/coherent regimes, while nonlinear traversal is preferable for grazing and several low/moderate-work cohorts.

The decision split has now been observed. Any incidence/footprint dispatcher fitted after this result is exploratory and may not be reported as held-out evidence. A hybrid may be developed, but its confirmatory result requires a new immutable corpus or renderer workload frozen before tuning. External TFDM/RMIP/PDM/DMM claims remain untested.

---

Related: [[Plan — Certified ray architecture comparison]] · [[Plan — A1 GPU mathematical port and differential tests]] · [[Guide — Eurographics paper draft]]
