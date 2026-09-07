---
title: Plan — Certified ray architecture comparison
tags: [plan, ray-tracing, displacement-mapping, triangle-proxy, DDA, Ogaki, TFDM, baselines]
status: a2-stop-gate-triggered
created: 2026-08-24
updated: 2026-08-25
parent: "[[Project — Conservative first-order queries on displacement maps]]"
predecessor: "[[Plan — First-order ray representation oracle]]"
implementation-status: a0-a2-complete-a2-performance-gate-failed-a3-a4-blocked
---

# Plan — Certified ray architecture comparison

> [!abstract] Purpose
> The completed representation oracle rejects affine first-order clipping as the source of broad ray acceleration on the fixed hierarchy. This plan isolates the remaining ray hypothesis: **can a certified piecewise enclosure of the nonlinear triangle-shell ray, traversed with a scalar min/max tube-supercover DDA and exact cubic leaves, outperform an Ogaki-style nonlinear quadtree and remain competitive with TFDM?** The candidate contains no first-order displacement payload or claim. This document freezes the mathematical surface, methods, fairness contract, staged implementation, metrics, and stop rules before traversal code changes.

## 1. What is and is not being tested

The causal architectural comparison is

$$
\text{nonlinear node/AABB traversal}
\quad\longleftrightarrow\quad
\text{certified piecewise shell ray + tube-supercover MM-DDA}.
$$

Both sides must use the same triangle proxy, shell map, displacement samples, scalar min/max mipmap, leaf diagonal, exact leaf equation, ray set, OptiX broad phase, and closest/any-hit semantics. First-order planes, residual clips, gradient payloads, Catmull–Clark surfaces, and conversion are absent.

Two questions must not be conflated:

1. **Internal causal ablation:** does changing only the traversal architecture beat the Ogaki-style nonlinear quadtree on the same represented surface?
2. **External method comparison:** how does the resulting complete method compare with TFDM under the same inputs and at matched error to a common dense oracle?

The first can support a direct speed ratio. The second requires quality–time–memory Pareto curves because TFDM's native local tangent-space construction is not automatically the same surface as the interpolated-normal shell.

## 2. Code audit and status at plan freeze

### 2.1 Reusable certified reference

The positive CPU chain is already complete:

- `p-d1-6-e254f252e010`: 864 practical cases with matching event, recursive, exhaustive, and Decimal root groups;
- exact-rational event/tube coverage and closed edge/corner ownership;
- an exact cubic leaf for the represented piecewise-affine height microtriangle; and
- scalar `MM-clip` hierarchy replay with authoritative operation counts.

This is the mathematical oracle and test-vector source. It is not a GPU timing implementation.

### 2.2 Existing nonlinear GPU path

`nrtdsm/gpu_kernels/nrtdsm_intersection_kernels.h::detailedSurface_generic_nonlinear` is the starting Ogaki-style comparator. It maps a world ray into the triangle shell, traverses the scalar min/max mipmap as a quadtree, performs nonlinear ray–child-AABB tests, orders children, and calls `testNonlinearRayVsMicroTriangle` at the leaf. Its represented leaf is compatible in form with the CPU cubic contract, but its prism interval, floating-point boundary behavior, and complete root/owner correspondence have not yet been audited against P-D1.6.

Call the audited comparator `NRT-QT`; do not call it a faithful reproduction of every optional acceleration structure in Ogaki's paper until the source audit is recorded.

### 2.3 Existing Mode 1 is not the candidate

`detailedSurface_generic` is retained as `M1-HEUR` only. It currently:

- chooses segments from midpoint chord-error heuristics with a fixed 32-segment cap;
- traverses a zero-width chord rather than the certified UV tube;
- contains fixed stack/step guards without a correctness fallback;
- tests planar world-space microtriangles at admitted leaves instead of the exact shell-space cubic; and
- has nonzero hit-mask disagreement in saved results.

Its old speed table is an engineering clue, not evidence for the certified method. It must not be silently patched and renamed. Add a separate intersection mode for the certified candidate so all three paths remain available for ablation.

### 2.4 TFDM integration status

The workspace includes the TFDM implementation and defaults to `LocalIntersectionType::TwoTriangle`, with `Bilinear` also available. It does not yet share NRTDSM's headless scene manifest, output schema, warmup/timing protocol, or ray buffers. Its visibility any-hit path must also be normalized to the same first-occluder termination policy before any any-hit timing comparison.

No current TFDM result is authoritative. The present Release build contains `nrtdsm.exe` but no verified TFDM benchmark executable.

### 2.5 Why the saved table is non-authoritative

`scripts/baseline_compare.py` launches Mode 0 then Mode 1 once per case, takes no discarded warmup measurements, treats Mode 0 rather than P-D1.6 as truth, and compares the heuristic planar-leaf path. The table nevertheless gives a useful regime prior: Mode 1 wins on the fine curved/sphere examples and loses on large flat/twisted prisms. The new experiment must preserve those adverse cases rather than selecting only the wins.

## 3. Frozen represented surface

For barycentric coordinates $(a,b)$ on one proxy triangle, define

$$
P(a,b)=P_A+a(P_B-P_A)+b(P_C-P_A),
$$

$$
N(a,b)=N_A+a(N_B-N_A)+b(N_C-N_A),
$$

and the shell map

$$
F(a,b,h)=P(a,b)+hN(a,b).
$$

The texture map $(u,v)$ is the fixed affine interpolation of the triangle's texture coordinates. Each displacement texel is split along the frozen `TL–BR` diagonal into two affine height microtriangles. On a leaf,

$$
d(u,v)=A u+B v+C.
$$

The represented displaced surface is exactly

$$
X(a,b)=F(a,b,d(u(a,b),v(a,b))).
$$

Both `NRT-QT` and the certified candidate must intersect this surface. Texture filtering, wrap mode, diagonal, map bit depth, height scale/bias, culling, shared-edge ownership, and ray interval conventions are identical.

The shell is accepted only where the inverse is well conditioned over the displacement interval. Invalid/folded prisms must use an explicit conservative fallback or be identified as outside the supported surface contract; they may not silently miss.

## 4. Nonlinear shell ray

For a world ray $R(t)=O+tD$, choose two independent directions $e_0,e_1$ perpendicular to $D$. Points on the ray satisfy

$$
e_i\cdot(F(a,b,h)-O)=0,\qquad i\in\{0,1\}.
$$

For fixed $h$, this is a $2\times2$ linear system in $(a,b)$ whose matrix is affine in $h$. Cramer's rule gives

$$
a(h)=\frac{A_2h^2+A_1h+A_0}{D_2h^2+D_1h+D_0},\qquad
b(h)=\frac{B_2h^2+B_1h+B_0}{D_2h^2+D_1h+D_0}.
$$

Because texture coordinates are affine in $(a,b)$,

$$
u(h)=\frac{U(h)}{D(h)},\qquad v(h)=\frac{V(h)}{D(h)},
$$

with quadratic numerators and denominator. World-ray distance $t(h)$ is evaluated from $F(a(h),b(h),h)$ and must be monotone on every accepted traversal interval.

The interval is partitioned at all relevant real roots of $D(h)$, texture-coordinate derivative numerators, proxy-boundary events, and any additional condition required by the P-D1 certificate. Roots are represented by outward intervals; exact root values are never used as unsafe floating endpoints.

## 5. Certified piecewise traversal guide

On a valid interval $I=[h_0,h_1]$, let $\ell_I(h)$ be the chord between $(u(h_0),v(h_0))$ and $(u(h_1),v(h_1))$. The P-D1 Bernstein/interval construction supplies conservative componentwise error bounds

$$
|u(h)-\ell_{I,u}(h)|\le\epsilon_u(I),\qquad
|v(h)-\ell_{I,v}(h)|\le\epsilon_v(I).
$$

The GPU candidate traverses the complete tube

$$
T_I=\ell_I(I)\oplus[-\epsilon_u,+\epsilon_u]\times[-\epsilon_v,+\epsilon_v],
$$

not only its centerline. Descent subdivides an interval only when required by the certificate, ordering, or a bounded resource policy. The chord is a traversal guide; it never replaces the exact surface.

At a hierarchy node $Q=Q_{uv}\times[h_{\min},h_{\max}]$:

1. reject if the certified tube cannot overlap $Q_{uv}$;
2. intersect the inherited $h$ interval with the node's scalar min/max height interval;
3. retain closed boundary ownership for every overlapped child;
4. order retained work by a conservative lower bound on ray distance; and
5. at a leaf, solve the exact represented equation.

Substituting the rational ray into $h=d(u,v)$ yields

$$
G(h)=hD(h)-A U(h)-B V(h)-C D(h)=0,
$$

a cubic. The closest admissible root supplies the true $(u,v,h,t)$ and shading attributes. Approximate chord coordinates are never reported as the hit.

## 6. Frozen method matrix

### 6.1 Internal same-surface methods

| ID | Method | Purpose |
|---|---|---|
| `CPU-ORACLE` | P-D1.6 exhaustive/event/Decimal reference | Correctness only |
| `NRT-QT` | Audited nonlinear ray–min/max-node quadtree with exact cubic leaves | Ogaki-style internal baseline |
| `CERT-DDA-MM` | Certified chord tubes + hierarchical scalar min/max DDA + exact cubic leaves | Primary candidate |
| `ARCH-HYB` | Cheap per-prism dispatch between `CERT-DDA-MM` and `NRT-QT` | Large-prism safeguard |
| `M1-HEUR` | Existing zero-width heuristic segments + planar world leaves | Historical ablation only |

`ARCH-HYB` may use only pre-traversal features: projected texture footprint, shell interval length, map resolution, and certificate conditioning. Its rule is trained on a frozen development split and then locked before the decision split. It may not inspect displacement-dependent traversal results from the held-out case.

### 6.2 External first-tier methods

| ID | Method | Comparison role |
|---|---|---|
| `TFDM-2T` | Workspace TFDM with two-triangle leaf, target mip sweep | Primary TFDM baseline |
| `TFDM-BILIN` | Workspace TFDM with bilinear leaf/Newton solve | Optional higher-quality TFDM point |
| `DENSE-ORACLE` | Dense shell-surface mesh / CPU exact reference | Quality only, never a speed baseline |

RMIP, PDM, dense hardware triangles, and DMM remain required for a final broad paper evaluation, but they are second-tier work. They are not implemented until the first-tier architecture gate passes. This prevents spending the schedule on expensive external ports for a candidate that does not beat the two baselines already present.

## 7. Fairness and quality normalization

### 7.1 Internal causal contract

`NRT-QT`, `CERT-DDA-MM`, and `ARCH-HYB` receive byte-identical ray/proxy/map inputs and must return the same represented root. They use the same broad-phase AABB, ray flags, closest/any-hit policy, target mip, and shading-disabled timing kernel. Any mismatch is a correctness failure, not a tunable quality point.

### 7.2 TFDM contract

Run two complementary views:

1. **Equal input/resolution:** same base triangles, vertex positions/normals/UVs, height samples, bit depth, scale/bias, target mip, ray batch, and broad scene. This measures implementation behavior but does not imply identical geometry.
2. **Equal quality:** sweep target mip/local intersection type and compare every point to `DENSE-ORACLE`. Plot time and memory against miss/false-positive rate, position/depth p99, normal p99, and silhouette error. Compare methods only at overlapping quality levels.

TFDM's native construction must not be modified to mimic our shell. Conversely, differences caused by its local surface model must be reported as quality differences, not labeled traversal errors.

### 7.3 Ray semantics

Freeze three workloads:

- primary/closest hit;
- shadow/any hit with immediate first-occluder termination for every method; and
- incoherent secondary closest hit from a persisted path-tracing ray buffer.

Use a ray-buffer microbenchmark for isolated traversal and a separate full OptiX/path-tracing benchmark. Do not compare full renderers until their ray counts, any-hit semantics, shading, NEE, and accumulation are normalized.

## 8. Dataset and workload manifest

Reuse the audited P-D0 assets and native decoders. The first-tier decision corpus contains:

- practical maps from Brick, Coast/Sand/Rocks, Cobble, Concrete, Dirt, Leather, Rock `1K/2K/4K`, Rocky Terrain, Rocky Trail, and Sand;
- procedural affine ramp, smooth bump, sparse peaks/cracks, and checker/noise stress maps;
- full `1K/2K/4K` maps for timing and the 48 frozen native windows for exhaustive correctness;
- low/medium/high displacement amplitude relative to proxy edge length and shell reach; and
- low/medium/high normal divergence.

Scene/proxy families are frozen before timing:

1. one large two-triangle plane: long-DDA adverse case;
2. one coarse twisted/curved proxy: conditioning and silhouette stress;
3. a medium curved grid: expected target regime;
4. a closed sphere-like mesh with thousands of proxy triangles;
5. one licensed UV-mapped object; and
6. repeated instances to measure hierarchy sharing and broad-phase behavior.

Ray families include front, oblique, grazing/silhouette, grid/proxy-boundary, near miss, and incoherent secondary rays. Each scene stores the exact ray buffer and a content hash. Camera/ray selection is frozen without observing candidate timing.

Use a development/decision split by complete `(asset, scene, ray-family)` groups, not by individual rays. The dispatch threshold and resource sizes are chosen on development only.

## 9. Metrics

### 9.1 Correctness and quality

- false-negative and false-positive hit counts;
- closest-root and owner-set mismatch;
- absolute/relative ray-$t$, world-position, $(u,v,h)$, and normal error: p50/p95/p99/max;
- exact leaf-polynomial and world-space residual;
- constructed edge/corner hit retention;
- fallback reason/count and unsupported-prism count; and
- silhouette/Hausdorff error for method-native TFDM comparisons.

### 9.2 Mechanism

Per ray and per candidate prism, persist p50/p95/p99/max for:

- shell intervals, denominator/event partitions, certified segments, and tube radii;
- hierarchy nodes, min/max fetches, DDA cells, duplicate boundary cells, descents, and exact leaves;
- cubic solves and roots tested;
- candidate-prism invocations;
- stack/queue high-water mark and fallback; and
- `ARCH-HYB` dispatch choice and prediction error.

### 9.3 Performance and memory

- isolated intersection ns/ray, ns/active ray, ns/candidate prism, and Mray/s;
- primary closest-hit, any-hit, and secondary closest-hit times;
- full fixed-ray-count frame/path time, separately from isolated traversal;
- preprocessing, full-map update, and localized update time;
- resident hierarchy, auxiliary, GAS, scratch, and executable-specific bytes;
- register count, local-memory spills, occupancy, branch efficiency, warp divergence, L2 hit rate, DRAM bytes, and texture-cache transactions; and
- time split among prism/setup, coefficient/event/certificate generation, DDA, min/max predicate, cubic leaves, and OptiX reporting.

All distribution summaries report the pooled result plus per-asset/per-ray-family values. A mean alone is insufficient.

## 10. Timing protocol

1. Build Release binaries from one recorded commit/configuration for all methods.
2. Record GPU, driver, CUDA, OptiX, compiler flags, clocks, and power/thermal state.
3. Use GPU events around the isolated kernel/OptiX launch; wall-clock process startup is never a traversal measurement.
4. Perform at least 20 discarded warmup launches, then at least 100 measured launches per ray buffer or enough repetitions for a stable confidence interval.
5. Randomize method order per trial and preserve the order/seed.
6. Report median and IQR across trials; bootstrap the paired speed ratio by complete workload group.
7. Lock clocks when available, otherwise record achieved clocks and reject thermally drifting trials.
8. Disable shading for isolated measurements. For full frames, normalize shading, NEE, denoising, accumulation, and actual ray counts.

## 11. Ordered implementation stages and gates

### A0 — harness and baseline audit

Implement no new traversal. First:

- create one versioned ray-buffer/scene/height-map manifest consumed by NRTDSM and TFDM adapters;
- add warmup, randomized repetitions, GPU-event timing, actual ray counts, and content-addressed output;
- build and smoke-test TFDM `TwoTriangle` and `Bilinear` modes;
- record source correspondence and code differences for `NRT-QT` versus Ogaki; and
- reproduce a small saved result only as a harness check.

**A0 gate:** identical persisted input hashes, correct output schema, deterministic replay, normalized ray semantics, and stable repeated timing with coefficient of variation below `3%` on the smoke workload. Failure stops before candidate code.

### A1 — GPU mathematical port and differential tests

Port only the P-D1 coefficient, outward event, chord-certificate, tube-supercover, exact cubic, and ownership primitives. Test each device primitive against persisted CPU vectors before composing them.

No hard cap may silently truncate work. A bounded stack/queue must have a conservative overflow fallback, and every fallback is counted.

**A1 correctness gate:** over all 864 frozen practical cases and dedicated analytic boundary/conditioning cases:

- zero candidate, root, closest-root, owner, and constructed-target omissions;
- zero unreported overflow/resource loss;
- every GPU interval encloses the CPU interval within the recorded outward tolerance; and
- every final hit satisfies the leaf/world residual thresholds frozen from P-D1.6.

Any failure blocks timing and must be fixed at the producing stage, with dependent artifacts regenerated.

### A2 — isolated same-surface GPU architecture replay

Implement `CERT-DDA-MM` and compare it with audited `NRT-QT` on identical ray/prism buffers, without OptiX broad phase or shading. Keep `M1-HEUR` as a labeled historical ablation. Train and freeze `ARCH-HYB` dispatch on the development split.

**A2 predictive gate on the held-out decision split:** correctness remains exact and:

1. `CERT-DDA-MM` is at most `0.85×` `NRT-QT` time on the medium/fine curved target cohort;
2. pooled ordinary coherent time is at most `0.90×`;
3. at least 8 of 12 practical asset identities improve;
4. `ARCH-HYB` has no ordinary per-asset regression above `1.10×`; and
5. the p99 resource fallback rate is zero, with any rare declared conditioning fallback included in time.

The large two-triangle plane and deliberately folded/near-singular shells are reported stress cohorts, not removed. If `CERT-DDA-MM` loses there, `ARCH-HYB` must demonstrate that the loss is predictable without held-out traversal results.

Failure stops full OptiX integration and TFDM timing. The ray paper remains a correctness/method result.

### A3 — complete NRTDSM/OptiX integration

Add a new certified intersection mode; do not overwrite Mode 1. Measure primary, any-hit, secondary, and full-frame workloads with identical broad phase and exact surface.

**A3 gate:** `ARCH-HYB` or `CERT-DDA-MM` retains at least a `10%` pooled isolated-intersection win and a `5%` full closest-hit launch win over `NRT-QT`, wins a majority of ordinary scene/asset groups, and introduces no correctness failure or unexplained `>10%` ordinary regression. Report candidate-prism invocation changes separately from inner traversal.

### A4 — TFDM first-tier comparison

Build the normalized TFDM adapter and run `TFDM-2T`, with `TFDM-BILIN` as an optional quality point. Produce equal-input and equal-quality plots.

**A4 paper-facing gate:** the candidate must be on the quality–time–memory Pareto frontier and satisfy at least one of:

1. at matched oracle quality, it is no slower than `0.85×` the best applicable first-tier non-tessellated baseline on the primary curved-proxy cohort; or
2. it remains within `1.15×` while providing a material, measured quality or memory/update advantage that TFDM does not match.

No universal fastest claim is required. Report flat, grazing, high-frequency, and large-prism failures explicitly.

Passing A4 authorizes a separate plan for RMIP, PDM, dense triangles, and DMM. It does not authorize those implementations automatically.

## 12. Required ablations

If A2 passes, retain these ablations:

1. exact nonlinear quadtree versus certified DDA at identical leaves;
2. centerline-only DDA versus certified tube supercover: correctness and added cells;
3. heuristic midpoint segmentation versus analytic certified segmentation;
4. pre-segment-all versus node-local/lazy certificate generation;
5. exact cubic leaf versus current planar world-microtriangle leaf;
6. independent segment restart versus traversal-state reuse;
7. `CERT-DDA-MM` versus `ARCH-HYB`;
8. dispatch features and held-out prediction error; and
9. target mip/quality sweep shared with TFDM.

The ablations identify where time is gained. None may reintroduce first-order clipping as the cause.

## 13. Paper decision and allowed claims

Before A2, safe language is only:

> We have a CPU-certified formulation of piecewise nonlinear shell-ray coverage and exact displaced-microtriangle intersection; its GPU performance is untested.

After A2/A3, an internal claim is allowed only if the frozen gates pass:

> Replacing nonlinear per-node ray–box tests with certified tube-supercover DDA reduces same-surface intersection time in the stated proxy/curvature regimes.

Only after A4 may the paper compare with TFDM. “Faster than all baselines,” “universally faster,” and any claim about RMIP/PDM/DMM remain prohibited until those methods are implemented and quality matched.

If A2 fails, do not weaken the gate or select only curved wins. Retain the certified ray construction as a correctness contribution and return to the area/metric application. If A2 passes but A4 fails, report an internal Ogaki-style architectural improvement without claiming state-of-the-art displaced-surface tracing.

## 14. Outputs and stop point

Every stage writes a content-addressed directory with:

- configuration, source, binary, input, and asset hashes;
- raw per-trial timings and exact ray counts;
- per-ray/per-prism mechanism records;
- correctness/quality records and failure cases;
- automatic gate results; and
- PNG/PDF/SVG plots plus rendered G-buffer comparisons.

Required first-tier figures are:

1. same-surface candidate versus `NRT-QT` paired time by regime;
2. time breakdown and segments/cells/leaves tails;
3. the large-prism crossover and `ARCH-HYB` dispatch boundary;
4. TFDM/candidate quality–time Pareto;
5. time–memory/update Pareto; and
6. representative depth, normal, silhouette, and difference images.

**Current stop point:** A0 through A2 are complete. The certified streaming candidate and same-buffer `NRT-QT` both reproduce the packed oracle on all 864 rays. Frozen timing run `ray-a2-4-timing-d9125676ceb0` passes checksums, correctness, and timing-stability checks but fails the predeclared performance gate: candidate/`NRT-QT` is `1.261x` on held-out ordinary rays, `1.457x` on the moderate shell, `1.272x` on the stress shell, and `1.624x` on grazing rays. It does win front rays at `0.383x` and the pooled non-decision-balanced corpus at `0.798x`. Under Section 11's frozen stop rule, A3 OptiX integration and A4 TFDM paper timing are blocked. See [[Report — A2 same-surface GPU comparison]].

## 15. A0 execution record — 2026-08-24

Authoritative run: `ray-a0-smoke-d9bbfd971b2f`, stored under `experiments/ray_architecture_a0/`. Configuration hash: `d9bbfd971b2f16ea252763a43c8279bb57ee61ef300b61b2a59c1211fdb54661`.

### 15.1 What was implemented

- NRTDSM now accepts separate warmup and measured frame counts and writes versioned per-frame GPU-event timing JSON. Warmup frames are discarded, and measured primary-launch ray counts are explicit.
- The A0 runner persists a content-addressed configuration, input/source/binary hashes, deterministic randomized trial order, raw JSONL records, summary, and automatic gate result.
- TFDM now has a non-timing GPU initialization probe for both `TwoTriangle` and `Bilinear`; the CUDA 13 context-creation call was updated so the Release target builds.
- The source audit records the upstream Ogaki algorithmic correspondence: quadratic-rational canonical/texture ray coefficients, nonlinear texture-space AABB tests, scalar min/max traversal, and cubic microtriangle leaves. It separately records the GPU/OptiX, displacement-only, and isolated-G-buffer differences from the paper configuration.
- The historical Mode 1 kernel was not changed. No certified candidate traversal was implemented in A0.

### 15.2 Frozen smoke result and gate

The run used 20 discarded warmups, 100 measured launches per trial, five trials, and seed `20260824`. Each timed method therefore has 500 GPU-event samples and `1,036,800,000` launched primary rays across the five `1920×1080` trials.

| Check | Result |
|---|---:|
| TFDM `TwoTriangle` GPU initialize/select probe | pass |
| TFDM `Bilinear` GPU initialize/select probe | pass |
| NRT-QT/source correspondence audit | pass |
| P-D1.6 correctness-source link (`864` cases) | pass |
| Exact timing sample counts | pass |
| M1-HEUR trial-median CV | `0.261%` |
| NRT-QT trial-median CV | `0.384%` |
| Frozen CV threshold | `3%` |
| **A0 gate** | **pass** |

The smoke medians were `1.1724 ms` for M1-HEUR and `1.3470 ms` for NRT-QT. These values validate repeatable harness operation only. They are one quad/test-map workload, compare different represented leaves, and contain no certified candidate; their ratio is **not** paper evidence and must not be reported as a method speedup.

### 15.3 Remaining obligations before timing claims

A0 does not establish GPU root/owner equivalence, a faithful performance reproduction of all Ogaki paper options, normalized TFDM traversal timing, or any advantage of `CERT-DDA-MM`. A1 must first differentially validate every device mathematical primitive against persisted P-D1.6 vectors. Only A2 is permitted to make a same-surface architecture comparison.

### 15.4 A1 planning handoff

Before device code changes, freeze a separate A1 implementation note containing:

1. the exact CPU-to-GPU coefficient and interval data schema;
2. test vectors for ordinary, grazing, denominator-root, edge/corner-owner, tangent, and overflow cases;
3. outward-rounding policy and per-primitive acceptance tolerances;
4. the implementation order from coefficient evaluation through events, chord certificates, tube supercover, cubic leaves, and ownership;
5. resource bounds plus a conservative overflow fallback; and
6. one automatic differential gate that blocks composition and timing on the first omission.

---

Primary-source anchors: [Ogaki 2023 DOI](https://doi.org/10.1145/3610548.3618199) · [TFDM project and paper](https://perso.telecom-paristech.fr/boubek/papers/TFDM/) · [RMIP paper](https://iliyan.com/publications/RMIP/RMIP_SigAsia2023.pdf) · [PDM paper](https://diglib.eg.org/items/d4d007c7-28be-4efa-a958-8b39eedd1007)

Related: [[Plan — P-D1 certified tube-supercover DDA reference]] · [[Plan — Practical first-order DDA ray traversal]] · [[Plan — First-order ray representation oracle]] · [[Guide — Eurographics paper draft]]
