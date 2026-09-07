# Plan — Mode 1 performance recovery

**Date:** 2026-08-30  
**Status:** P0–P7 complete; P8 renderer-derived replay passed; P9 renderer integration active  
**Scope:** recover the performance of the certified piecewise shell-ray traversal before making a paper-level comparison with Mode 0, NRT-QT, TFDM, or RMIP.

## 0. Progress update — P0 implementation and measurement

P0 now has separate SBT-selected Mode-0/Mode-1 entries for primary, closest, and visibility rays. A production build compiles out all traversal counters and inner timestamps; a profiling build reduces exact work counters by ray family. The specialized entries reduce compiler-attributed direct stack from the old combined entry's roughly `5.3 KB` to `1.864 KB` for M1 and `376–520 B` for M0. M1 still uses `128` registers and `268–272 B` of direct spills, so resource pressure remains material.

Counter-free production timing used 16 warm-up frames and 30 measured frames at 1920×1080:

| scene | pass | Mode 0 median | M1 median | M1 / M0 |
|---|---:|---:|---:|---:|
| twisted quad + rock | G-buffer | `1.450 ms` | `21.673 ms` | `14.9x` |
| twisted quad + rock | path trace | `5.582 ms` | `844.318 ms` | `151.3x` |
| curved surface + cobble | G-buffer | `3.534 ms` | `30.858 ms` | `8.7x` |
| curved surface + cobble | path trace | `19.693 ms` | `199.082 ms` | `10.1x` |

The timing CVs are `0.6–3.7%`, so the multi-fold gaps are not measurement noise. Removing counters and isolating entry points is useful but does not remove the algorithmic cost.

Final-frame profiling counters separate DDA anchors from hierarchy nodes and include all ray families. Values below are per candidate-prism invocation:

| scene / family | method | segments | DDA steps | hierarchy nodes | leaf cells |
|---|---:|---:|---:|---:|---:|
| twisted / primary | Mode 0 | `0` | `0` | `21.38` | `0.59` |
| twisted / primary | M1 | `4.64` | `24.08` | `15.51` | `0.99` |
| twisted / closest | Mode 0 | `0` | `0` | `31.97` | `1.03` |
| twisted / closest | M1 | `8.18` | `41.47` | `39.73` | `20.64` |
| twisted / visibility | Mode 0 | `0` | `0` | `28.99` | `0.92` |
| twisted / visibility | M1 | `7.59` | `40.32` | `29.39` | `2.36` |
| curved / primary | Mode 0 | `0` | `0` | `9.48` | `0.29` |
| curved / primary | M1 | `0.92` | `4.09` | `4.16` | `0.54` |
| curved / closest | Mode 0 | `0` | `0` | `10.26` | `0.35` |
| curved / closest | M1 | `0.90` | `3.57` | `6.35` | `1.03` |
| curved / visibility | Mode 0 | `0` | `0` | `8.60` | `0.26` |
| curved / visibility | M1 | `0.86` | `3.60` | `5.12` | `0.78` |

Candidate-prism invocation counts are essentially identical between methods, so DDA does not reduce top-level OptiX work. It does reduce hierarchy nodes in curved-cobble, but certificate/segmentation cost and extra leaf work dominate. In twisted-rock, secondary rays also create many segments and substantially more closest-ray leaf work.

The new timestamp profiler detects and rejects implausible OptiX timer intervals instead of reporting them as cycles. Mode 0 intervals are stable; roughly half of M1 traversal aggregates are rejected because the large M1 local state corrupts or invalidates long-lived timing accumulators. Consequently, M1 stage percentages are diagnostic lower-bound evidence only. Accepted samples place certificate/segmentation at roughly `61–76%` on curved-cobble. Exact work counters and counter-free pass timings are authoritative; fine stage timing should use controlled compile-time ablations or Nsight rather than long-lived in-kernel timestamps.

## 1. Current conclusion

The current result does **not** show that DDA is ineffective. It shows that the implementation pays much more than the cost of a DDA step:

1. construct rigorous rational-curve certificates using binary64 and directed interval arithmetic;
2. subdivide every surviving curve until its componentwise UV tube radius is at most `0.3` leaf texel;
3. retain as many as 32 full segment records and 32 subdivision tasks per candidate proxy;
4. run DDA only to obtain coarse anchors;
5. expand every anchor by the tube radius, clip hierarchy nodes, fetch min/max values, and descend;
6. reconstruct four world-space displaced corners and test two triangles at every surviving leaf; and
7. repeat some root, node, and leaf work independently for adjacent segments.

DDA accelerates only item 4. It cannot compensate for excessive segment generation, expensive certificates, local-memory traffic, repeated hierarchy work, or costly leaves.

There is still a credible performance path. The strongest evidence is the curved-cobble case: certified M1 performs fewer recorded cell/node tests than Mode 0 (`48.56` versus `55.87` per active primary pixel), yet is slower. Culling is therefore not the only problem; the implementation cost per candidate is too high. The twisted-rock case additionally exposes over-segmentation and extra traversal work.

Parity with all baselines is not yet predictable. The immediate goal is to determine whether a hierarchy-coupled segmentation and a lean GPU implementation can bring M1 close to Mode 0. Only then should the first-order residual hierarchy be added and compared with the same traversal using min/max.

## 2. Frozen evidence

These are one-frame engineering measurements, not paper timings. They are useful for diagnosis only.

| scene | metric | Mode 0 | certified M1 | M1 / M0 |
|---|---:|---:|---:|---:|
| twisted quad + rock | path pass | `118.052 ms` | `908.385 ms` | `7.69x` |
| twisted quad + rock | total frame | `210.293 ms` | `1071.797 ms` | `5.10x` |
| twisted quad + rock | throughput | `17.57 Mray/s` | `2.28 Mray/s` | `0.13x` |
| curved surface + cobble | path pass | `110.296 ms` | `212.917 ms` | `1.93x` |
| curved surface + cobble | total frame | `242.797 ms` | `382.361 ms` | `1.57x` |
| curved surface + cobble | throughput | `18.80 Mray/s` | `9.74 Mray/s` | `0.52x` |

Primary-ray work counters are:

| scene | method | candidate invocations / active px | segments / active px | mixed anchor+node tests / active px | leaf cells / active px |
|---|---:|---:|---:|---:|---:|
| twisted + rock | Mode 0 | `2.00` | `0.00` | `42.77` | `1.17` |
| twisted + rock | M1 | `2.00` | `9.28` | `79.19` | `1.98` |
| curved + cobble | Mode 0 | `5.89` | `0.00` | `55.87` | `1.68` |
| curved + cobble | M1 | `5.89` | `5.39` | `48.56` | `3.20` |

The segment counter is accumulated over all candidate proxy invocations. Thus twisted-rock averages about `4.64` segments per candidate invocation, whereas curved-cobble averages about `0.92`; it is not correct to interpret both numbers as segments for one curve.

The current OptiX entry function reports `128` registers, `5336` bytes of direct stack, and `300` bytes of direct spills. This is a serious warning, but it cannot yet be attributed wholly to M1: the same runtime-selected intersection entry point contains both nonlinear Mode 0 and M1, and the compiler report covers the combined entry point. Method-specialized programs are required to measure each method's actual resource footprint.

## 3. Cost model

Use the following model rather than “DDA steps” alone:

\[
T \approx N_{\mathrm{inv}}
\left(
C_{\mathrm{proxy}} + C_{\mathrm{coeff}} +
N_{\mathrm{cert}} C_{\mathrm{cert}} +
N_{\mathrm{seg}} C_{\mathrm{seg}} +
N_{\mathrm{anchor}} C_{\mathrm{anchor}} +
N_{\mathrm{node}} C_{\mathrm{node}} +
N_{\mathrm{leaf}} C_{\mathrm{leaf}}
\right) + C_{\mathrm{resource}}.
\]

`C_resource` includes reduced occupancy from the 128-register footprint, local-memory accesses generated by the large direct stack, spill traffic, divergence, and instruction-cache pressure. DDA reduces only `N_anchor`; a larger certified tube can increase the number of neighboring roots and hierarchy nodes around each anchor.

## 4. Static audit findings

### 4.1 Proxy and coefficient setup

- Texture-transform decomposition, effective displacement scale, and base height are invariant per geometry instance but are expressed in the per-candidate intersection function.
- Transformed proxy UVs, the inverse UV-to-barycentric map, triangle edge half-planes, and the UV AABB are invariant per proxy but are reconstructed per invocation.
- The rigorous coefficients are built in binary64 for every ray/proxy pair. The current dominant-axis projection is correct but creates many double temporaries. An equivalent scalar-triple-product formulation may use fewer operations and less live state; it must be derived and checked before replacement.
- The entry programs choose Mode 0 or M1 through a runtime branch. This combines two large implementations in one OptiX intersection entry and prevents clean resource attribution.

### 4.2 Certificate construction and segmentation

- M1 begins with the full displacement-height band of the proxy. The old side-prism interval was removed because it did not conservatively return all relevant roots.
- A tube is first clipped only to the proxy's UV **AABB**, not to the triangular proxy domain. Curves can therefore be certified and split in AABB corners that cannot belong to the proxy.
- The split condition is

  \[
  \max(r_u / \Delta u_{\mathrm{leaf}}, r_v / \Delta v_{\mathrm{leaf}}) > 0.3.
  \]

  This is not required for correctness: a larger certified tube remains conservative. It is a fixed eager-flattening heuristic intended to reduce later work. It can spend many certificate evaluations and create many segments before knowing whether the hierarchy would have rejected the corresponding region cheaply.
- Every binary split recomputes interval coefficient conversion, rational second-derivative numerator polynomials, endpoint intervals, and Bernstein ranges. Several of these quantities are invariant for the ray/proxy pair or reusable between parent and child.
- `finalSegs[32]` occupies at least `1024` bytes and `tasks[32]` approximately `384` bytes before compiler temporaries. The active hierarchy also uses two 16-float depth arrays. This is incompatible with a lean common path.
- Splits are always at the height midpoint. This is robust but not necessarily cost-optimal when curvature or the denominator is strongly nonuniform.

### 4.3 DDA and hierarchy traversal

- DDA enumerates the centerline's coarse cells. Neighbor expansion by the componentwise tube radius then generates a conservative supercover.
- A 64-bit ownership mask removes duplicate roots **within one segment**. The mask is reset for every segment, so adjacent segments may descend the same roots and leaves again.
- The start LOD is chosen by the fixed rule `footprint <= 4 * cellScale`; this has not been calibrated against measured certificate, node, and leaf costs.
- The current `numAabbTests` mixes DDA anchors and hierarchy-node tests. It is not an apples-to-apples cell count against Mode 0.
- The proxy triangle-square classification is recomputed at every hierarchy node even when an ancestor square is already known to be fully inside the proxy. The existing classifier distinguishes outside, overlapping, and inside, so the inside state can be inherited.
- A texture min/max fetch and an interval-height clip occur at every admitted node. Different segments can repeat these reads.
- DDA is not automatically best for every footprint. For a short segment or a wide tube with a small root bounding box, scanning the bounded root rectangle can require less control flow than centerline DDA plus neighbor expansion.
- Leaves are not traversed in a certified near-to-far world-ray order. The implementation has no conservative per-segment world-`t` interval, so a known hit cannot prune unrelated later segments aggressively.

### 4.4 Leaf reconstruction and intersection

Each admitted leaf currently performs:

- four height texture samples;
- four UV-to-proxy-barycentric conversions to build displaced corners;
- two world-space Möller–Trumbore triangle tests; and
- another UV-to-barycentric conversion for every accepted triangle hit.

The UV-to-barycentric conversion recomputes the same two UV edges, determinant, and reciprocal each time. Those are proxy invariants. Adjacent leaf cells also share height corners, although caching must be designed carefully because a large per-thread cache would recreate the local-memory problem.

The current leaf test uses the entire OptiX ray interval rather than a segment-specific conservative `t` interval. This remains a true intersection with the represented surface, but can retest or accept the same leaf from multiple segments and prevents segment-level pruning.

### 4.5 Ray types and reporting

- Existing traversal counters are written only for the G-buffer primary trace.
- Path-tracing radiance and visibility rays call the intersection code with no counters. Consequently, primary-ray segment and node counts cannot explain the `908 ms` path pass.
- Visibility rays need any hit, not the closest hit. The current intersection function still completes its local nearest-hit search before reporting an intersection to the any-hit program. A ray-type-specialized, near-to-far certified traversal could report and terminate as soon as the first exact occluder is found.

### 4.6 Instrumentation defects

- Frozen timings use one measured frame and zero warm-up frames.
- `cyclesTrav` currently includes certificate generation and separately timed leaf tests, but omits the DDA and hierarchy descent between them. Its label “post-setup work” is therefore incorrect.
- The current counters provide averages only; there are no p50/p95/p99/max distributions or duplicate-work counts.
- Per-leaf `clock64()` calls perturb the profiling build. Production timing must be obtained from a counter-free build.
- Nsight Compute 2026.1 is installed and can be used for register occupancy, local loads/stores, spill traffic, texture behavior, divergence, and stall analysis. Nsight Systems is not currently available, but it is not required for the first audit.

## 5. Planned implementation stages

No later stage starts until the preceding measurement or correctness gate passes.

### P0 — trustworthy measurement and method isolation

1. Create separate Mode-0 and M1 OptiX intersection entry points/program groups so the hot method is selected through the SBT, not a runtime branch inside one entry point.
2. Keep two builds:
   - a production timing build with no traversal counters or inner `clock64()` calls; and
   - a profiling build with compile-time sampled counters.
3. Time at least 16 warm-up frames followed by 30 measured frames. Record p50, p95, mean, standard deviation, and coefficient of variation for G-buffer, radiance, and visibility work separately.
4. Add sampled per-ray-family counters:
   - proxy invocations;
   - proxy-domain intervals;
   - certificate calls, accepted certificates, and splits;
   - segment count and maximum tube radius by LOD;
   - DDA anchors;
   - unique and repeated roots within and across segments;
   - hierarchy nodes, min/max fetches, and inside-proxy inherited nodes;
   - unique and repeated leaf cells;
   - exact triangle tests;
   - segments and nodes pruned by an existing hit; and
   - fallback reason.
5. Run Nsight Compute on one small, deterministic launch for each method and ray family. Capture achieved occupancy, registers, local load/store bytes, spill instructions, branch efficiency, warp-stall reasons, texture hit behavior, and instruction count.

**P0 output:** a stage-cost table and ray-family distributions that can identify whether certificates, repeated traversal, leaves, or visibility rays dominate.

### P1 — certified proxy-domain interval before segmentation

The exact shell ray has rational coordinates

\[
u(h)=U(h)/D(h),\qquad v(h)=V(h)/D(h),
\]

where `U`, `V`, and `D` are quadratic. Each proxy barycentric coordinate is therefore another quadratic numerator divided by `D`. After proving a constant denominator sign on an interval, solve or conservatively isolate **all** roots of the three barycentric numerators, partition the height band, and retain only subintervals whose shell ray is inside the closed proxy triangle.

This replaces neither the tube certificate nor leaf ownership. It only removes height intervals that cannot belong to the candidate proxy. Unlike the historical side-prism code, it must retain every root and every valid disjoint interval.

#### P1.1 concrete formulation and conservative failure policy

Write the canonical barycentric coordinates as

\[
b(h)=B(h)/D(h),\qquad c(h)=C(h)/D(h),\qquad
a(h)=A(h)/D(h),\qquad A(h)=D(h)-B(h)-C(h),
\]

where `A`, `B`, `C`, and `D` are quadratic polynomials in ascending power order. On an interval where `D` has a certified constant sign, the closed proxy-domain condition is

\[
\operatorname{sign}(D)A\ge0,\qquad
\operatorname{sign}(D)B\ge0,\qquad
\operatorname{sign}(D)C\ge0.
\]

The implementation partitions the height band at verified enclosures of every real root of `A`, `B`, and `C`. Signs are certified on the regions between root enclosures; every root enclosure itself is retained conservatively and merged with neighboring retained regions. This preserves edge-touching rays and turns root-rounding uncertainty into extra work rather than a missed hit.

The first implementation deliberately falls back to the original full height band when any of the following occurs:

- `D` may contain a root in the height band;
- a polynomial is non-finite or effectively indeterminate;
- a near-double/tangent root cannot be enclosed and classified safely;
- a supposedly root-free region does not obtain a certified sign; or
- the fixed output capacity is exceeded.

The maximum event count is small: three quadratic barycentric numerators contribute at most six root neighborhoods and seven between-root regions. A capacity of 16 intervals is sufficient with room for conservative splitting. The isolated P1.3 ablation must leave the existing certificate threshold, segment representation, DDA start-LOD rule, hierarchy descent, and leaf test unchanged.

Also derive a conservative world-ray interval

\[
t(h)=T(h)/D(h)
\]

for every retained interval. Use it to order intervals, restrict exact leaf tests, prune intervals behind the current closest hit, and support early termination for visibility rays.

**First isolated ablation:** current full height band versus all-root triangular-domain intervals, with the old segmentation and traversal otherwise unchanged.

#### P1.2–P1.4 implementation and isolated result (2026-08-30)

The reference solver is implemented in `scripts/proxy_domain_intervals.py` and checked by `scripts/test_proxy_domain_intervals.py`. The suite includes connected, disjoint, empty, denominator-root, tangent-root, and 500 randomized cases with 2,001 dense height samples each. Together with the existing DDA/certificate tests, all 22 tests pass. The CUDA implementation is in `nrtdsm/gpu_kernels/mode1_proxy_domain.h` and is invoked before the existing certificate task stack. It uses binary64 arithmetic for the first conservative version and falls back to the original full height band on unresolved cases.

Frozen one-spp geometry AOVs are bit-identical before and after P1 on both twisted-rock and curved-cobble: zero mask, position, or normal-buffer changes. Profiling reports:

| scene / ray family | certified | empty | intervals/invocation | segments before → after | DDA before → after | hierarchy before → after | leaves before → after |
|---|---:|---:|---:|---:|---:|---:|---:|
| twisted / primary | `100.00%` | `47.74%` | `0.54` | `4.64 → 2.28` | `24.08 → 11.87` | `15.51 → 15.20` | `0.99 → 0.99` |
| twisted / closest | `99.86%` | `37.77%` | `0.65` | `8.18 → 4.11` | `41.47 → 20.87` | `39.73 → 39.07` | `20.64 → 19.74` |
| twisted / visibility | `99.81%` | `43.32%` | `0.57` | `7.59 → 3.72` | `40.32 → 19.81` | `29.39 → 29.09` | `2.36 → 2.33` |
| curved / primary | `99.93%` | `33.69%` | `0.85` | `0.92 → 0.49` | `4.09 → 2.32` | `4.16 → 3.72` | `0.54 → 0.50` |
| curved / closest | `99.79%` | `18.02%` | `1.42` | `0.90 → 0.50` | `3.57 → 2.43` | `6.35 → 5.28` | `1.03 → 0.95` |
| curved / visibility | `99.85%` | `25.68%` | `1.14` | `0.86 → 0.46` | `3.60 → 2.26` | `5.12 → 4.38` | `0.78 → 0.70` |

Counter-free 16-warm-up/30-measured timing gives:

| scene | pass | pre-P1 p50 | P1 p50 | change | P1 p95 |
|---|---|---:|---:|---:|---:|
| twisted-rock | G-buffer | `21.673 ms` | `15.970–16.048 ms` | `−26.0–26.3%` | `16.33–16.36 ms` |
| twisted-rock | path | `844.318 ms` | `796.135–798.681 ms` | `−5.4–5.7%` | `803.438–819.219 ms` |
| curved-cobble | G-buffer | `30.858 ms` | `24.173 ms` | `−21.7%` | `24.918 ms` |
| curved-cobble | path | `199.082 ms` | `142.713 ms` | `−28.3%` | `144.388 ms` |

P1 therefore passes the isolated `5%` acceptance gate and causes no key-case regression. It is a real execution optimization, not the first-order representation claim. The result also localizes the remaining cost: proxy clipping removes roughly half of segmentation/DDA work, but twisted secondary hierarchy and leaf work barely changes. The straightforward implementation increases the production M1 direct stack from roughly `1.86 KB` to `2.31 KB`, remains at 128 registers, and emits binary64 warnings. Reducing root-solver arithmetic/state is therefore a later fast-path target; do not attribute the full surviving gap to DDA.

The twisted path series has one repeatable high-work first measured frame in both the pre-P1 and P1 runs (`1011.5 ms` versus about `994 ms`), so its full-series CV remains about `4.7%` even though p50 and p95 repeat. Curved-cobble reaches `0.70%` CV. Publication timing must either use fixed-work ray-family launches or a protocol that explicitly models per-frame stochastic workload; the current robust medians are engineering evidence, not final paper statistics.

After the isolated measurement, proxy-domain clipping was changed to consume the already available binary64 dominant-axis barycentric numerators used by the tube certificate, rather than reconstructing a second rounded float/orthonormal-frame curve. This keeps the proof and certificate on one curve definition, preserves the frozen AOV bit-for-bit, reduces twisted closest-ray leaf tests from `19.74` to `18.39` per invocation in the sampled run, and gives a further small `0.26%` path-time improvement. It does not reduce the current `2.31 KB` direct stack.

#### P1.5 world-ray interval plan

For base point `P_A`, base edges `E_B=P_B-P_A`, `E_C=P_C-P_A`, director `N_A`, and director differences `F_B=N_B-N_A`, `F_C=N_C-N_A`, define

\[
q_A(h)=\langle P_A-O,R\rangle+h\langle N_A,R\rangle,
\quad q_B(h)=\langle E_B,R\rangle+h\langle F_B,R\rangle,
\quad q_C(h)=\langle E_C,R\rangle+h\langle F_C,R\rangle.
\]

With canonical barycentric numerators `B(h)`, `C(h)` and denominator `D(h)`, the world-ray parameter is

\[
t(h)=\frac{T(h)}{\langle R,R\rangle D(h)},\qquad
T(h)=q_A(h)D(h)+q_B(h)B(h)+q_C(h)C(h).
\]

`T` is cubic. On every proxy-domain interval, convert `T` and `D` to Bernstein form, bound their ranges, and perform outward-rounded interval division after the existing constant-sign denominator proof. This deliberately prefers a cheap conservative range to solving the quartic numerator of `t'(h)`.

Implementation gates:

1. **P1.5a reference only:** randomized dense-oracle tests must show every sampled `t(h)` inside the reported range, including negative denominators, narrow intervals, and near-endpoint extrema.
2. **P1.5b count-only GPU:** compute per-final-segment `t` ranges and report potential skips against current `rayTmin`/best hit without changing traversal or leaf-test bounds. Reject the feature if ranges are too loose to skip useful work.
3. **P1.5c closest-hit ablation:** process segments by increasing conservative `tLower`, skip a segment only when `tLower >= currentHit`, and restrict exact leaf tests to the intersection of the segment's conservative `t` interval and the OptiX ray interval. Preserve the frozen AOV and compare nodes/leaves/time.
4. **P1.5d visibility specialization:** after P1.5c proves the bound, give visibility rays a separate template path that reports the first exact in-range hit immediately. Any-hit termination makes global near-to-far order unnecessary for correctness, although near-first order can reduce time-to-first-hit.

Every substep is timed separately. Do not combine the world-`t` bound, visibility specialization, and a new segmentation policy in one measurement.

#### P1.5 result (2026-08-30)

`scripts/world_t_intervals.py` implements the cubic-over-quadratic reference bound and `scripts/test_world_t_intervals.py` adds affine, negative-denominator, singular-denominator, and 1,000 randomized dense cases. The combined proxy-domain/world-`t`/DDA/certificate suite has 26 passing tests. The CUDA implementation is in `nrtdsm/gpu_kernels/mode1_world_t.h`.

Count-only profiling showed that the bound is useful primarily for secondary rays:

| scene / ray family | segments outside OptiX ray interval | additional segments behind current hit |
|---|---:|---:|
| twisted / primary | `0.67%` | `0.00%` |
| twisted / closest | `49.67%` | `5.21%` |
| twisted / visibility | `47.51%` | `6.02%` |
| curved / primary | `8.44%` | `0.00%` |
| curved / closest | `34.08%` | `0.35%` |
| curved / visibility | `28.48%` | `1.91%` |

The accepted implementation therefore computes world-`t` bounds only for closest and visibility rays. Primary rays retain the P1 proxy-domain path without the cubic bound. Whole segments are skipped only when their outward-rounded range is disjoint from the OptiX ray interval or begins behind the current closest hit. Leaf tests deliberately retain the global ray interval.

On twisted secondary rays, this changes closest DDA/hierarchy/leaves from `20.87/39.07/18.39` to `9.38/20.73/17.31` per candidate invocation. Complete visibility profiling, including terminating-hit invocations, reports `9.80/19.69/2.93`; the earlier wrapper-only profiler omitted successful visibility calls and must not be used as the absolute denominator. On curved, closest becomes `1.60/3.80/0.79`, and complete visibility is `1.78/4.35/0.91`.

Counter-free p50 timing of the accepted P1+P1.5 path is:

| scene | pass | original M1 | accepted P1+P1.5 | cumulative change | Mode 0 | remaining M1/M0 |
|---|---|---:|---:|---:|---:|---:|
| twisted-rock | G-buffer | `21.673 ms` | `15.889 ms` | `−26.7%` | `1.450 ms` | `10.96×` |
| twisted-rock | path | `844.318 ms` | `744.028 ms` | `−11.9%` | `5.582 ms` | `133.3×` |
| curved-cobble | G-buffer | `30.858 ms` | `24.070 ms` | `−22.0%` | `3.534 ms` | `6.81×` |
| curved-cobble | path | `199.082 ms` | `135.723 ms` | `−31.8%` | `19.693 ms` | `6.89×` |

The P1.5 incremental path change relative to P1 alone is `−6.30%` on twisted and `−4.90%` on curved. Frozen geometry AOVs remain bit-identical.

Two attempted extensions were rejected:

- Restricting exact leaf triangles to a segment's `t` range changed 18 curved-cobble primary pixels, including 13 hit/miss values and a `0.112` maximum position shift. The current conservative supercover admits a leaf from more than one neighboring segment, so a segment range is safe for whole-segment traversal pruning but is not yet an exclusive leaf-ownership interval.
- Visibility-specific first-hit local termination improved the curved path by only `1.24%` and twisted by `0.13%`. It was removed under the `5%` isolated acceptance rule.

Near-to-far segment ordering remains a P4 scheduling experiment. Its observed current-order closest-prune opportunity is small on curved and moderate on twisted, so it should not delay the larger P2 segmentation/stack work.

### P2 — hierarchy-coupled, cost-driven segmentation

Replace “flatten every curve to `0.3` leaf texel” with lazy segmentation:

1. certify one chord tube over a retained proxy-domain interval;
2. estimate traversal cost at the current hierarchy scale;
3. keep the tube if its predicted supercover/node cost is cheaper than two child certificates plus their predicted traversal;
4. split only when the predicted saving clears a measured margin; and
5. merge adjacent accepted segments whenever their combined certificate passes the same cost test.

The certificate remains rigorous at every size. The decision to split is a performance choice, not a correctness tolerance.

Test four policies in replay before choosing GPU control flow:

| policy | purpose |
|---|---|
| fixed `0.3` leaf texel | current reference |
| one unsplit certified tube | lower bound on certificate/segment overhead, upper bound on tube traversal |
| LOD-relative radius threshold | simple hierarchy-aware baseline |
| measured cost-driven split/merge | proposed policy |

For split placement, compare midpoint, curvature-weighted midpoint, denominator/derivative event points, and the next hierarchy-boundary event. Choose based on total measured work, not segment count alone.

The preferred final formulation is that hierarchy descent requests refinement: the surface/ray hierarchy is also the segmentation oracle. This is both a performance strategy and the coherent technical contribution envisioned by the project.

#### P2 concrete experiment plan after P1.5

P2 starts with a controlled policy sweep. Proxy-domain intervals, rigorous tube construction, world-`t` pruning, DDA, hierarchy admission, and exact leaf intersection remain fixed; only the decision to split an already certified interval changes. This isolates segmentation policy from the other execution improvements.

First add profiling-only counters for certificate calls, certificate recomputations after UV-AABB clipping, accepted final segments, midpoint splits, UV-AABB task rejects, maximum live task-stack size, and maximum final-segment count. Then evaluate:

| label | split threshold in leaf texels | purpose |
|---|---:|---|
| `E0.3` | `0.3` | current policy and frozen reference |
| `E0.6` | `0.6` | modest relaxation |
| `E1.0` | `1.0` | one leaf-scale tube |
| `E2.0` | `2.0` | deliberately coarse segmentation |
| `UNSPLIT` | effectively infinite | certificate-overhead lower bound and traversal-work upper bound |

For every policy, report certificate, segment, DDA, hierarchy, and leaf counts per candidate-prism invocation for primary, closest, and visibility rays. Check diagnostic fallbacks and frozen geometry AOVs before timing. Counter-free timings use the same warmup/measured-frame protocol as P1/P1.5.

Fit the diagnostic model

\[
T \approx c_0 N_{\mathrm{inv}} + c_c N_{\mathrm{cert}} + c_d N_{\mathrm{DDA}}
  + c_h N_{\mathrm{node}} + c_l N_{\mathrm{leaf}}
\]

to estimate whether a split pays for its two child certificates. The first online policy will estimate a tube's expanded root footprint from its UV chord and radii, using an anchor count proportional to

\[
\left\lceil |\Delta u|/s \right\rceil
+ \left\lceil |\Delta v|/s \right\rceil + 1,
\]

where `s` is the candidate hierarchy-cell scale. Split only when the predicted reduction in DDA/node/leaf work exceeds the measured child-certificate cost by a safety margin. Child certificates computed for the decision must be reused if the split is accepted.

Acceptance rules: preserve the frozen AOVs and zero unresolved fallbacks; keep a policy only if it improves path time by at least `5%` without regressing the other key scene by more than `2%`. If a relaxed fixed threshold or `UNSPLIT` wins, use the simpler policy and do not overstate hierarchy coupling. If all relaxed policies lose, optimize certificate state/materialization before adding a more elaborate split oracle. P3 begins only after this sweep establishes the observed segment and task distributions.

#### P2 fixed-policy result (2026-08-30)

Profiling-only counters now distinguish certificate calls, post-AABB recomputations, accepted segments, midpoint splits, UV-AABB rejects, and peak live/final state. The instrumentation build passes the 26-test conservative certificate/domain/world-`t`/DDA suite. All threshold policies produce geometry AOVs bit-identical to `E0.3` in both key scenes. Curved-cobble's normal AOV is also bit-identical. Twisted-rock changes only two normal pixels at `E0.6/E1.0` and three at `E2.0/UNSPLIT`, with identical hit positions; this is retained as a tie-ordering audit item rather than silently described as bit-exact shading identity.

At `E0.3`, twisted closest rays average `7.65` certificate calls for `4.11` accepted segments; curved closest rays average `1.59` calls for only `0.50` accepted segments because `0.96` tasks per invocation are rejected by the proxy UV AABB. The scenes therefore expose different costs: twisted-rock is split-heavy, while curved-cobble spends much of its certificate work proving that intervals are outside the proxy box.

Relaxing to `E1.0` reduces twisted closest certificate calls by `47%`, segments by `44%`, DDA steps by `37%`, and leaf tests by `45%`, although hierarchy tests rise by `15%`. An unsplit tube is not viable: twisted closest hierarchy tests rise from `20.73` to `475.17` and leaf tests from `17.31` to `282.26` per invocation. This establishes that segmentation is necessary and brackets the useful fixed-threshold region to roughly `0.6–1.0` for the current traversal.

Counter-free `16`-warmup/`30`-sample medians relative to the frozen `E0.3` P1/P1.5 result are:

| threshold | twisted G-buffer | twisted path | curved G-buffer | curved path |
|---:|---:|---:|---:|---:|
| `0.6` | `−11.41%` | `+0.33%` | `−0.22%` | `−1.76%` |
| `1.0` | `−6.85%` | `−0.74%` | `+0.27%` | `−1.91%` |
| `2.0` | `+10.03%` | `+1.13%` | `+1.73%` | `−0.98%` |

No fixed policy passes the `5%` path-time acceptance gate, so `0.3` remains the frozen default for now. The large reduction in dynamic work but small secondary timing change is evidence that the always-materialized `32`-task and `32`-segment arrays, 128-register kernel, and local-memory spills dominate the current secondary path. Do not spend the next iteration on a more elaborate split oracle in the same storage architecture. First remove that fixed common-path state; then repeat the policy choice because its cost balance will have changed.

### P3 — remove segment materialization from the common path

After P2 reveals the actual segment distribution:

- add a register-resident fast path for one or two segments;
- send only uncommon hard intervals to a bounded slow path;
- reuse parent endpoint and denominator evaluations when a split occurs;
- preconvert invariant interval coefficients once per ray/proxy invocation;
- precompute rational second-derivative numerator coefficients once; and
- evaluate whether rational Bernstein/de Casteljau subdivision is cheaper than recomputing interval polynomials.

The earlier streamed-segment experiment is not decisive for this stage: it streamed the old eager segments before the later root-ownership and compact-traversal changes. A fused hierarchy-coupled fast path is a different architecture and must be tested anew.

Only after the rigorous path is stable, test a float certificate fast path with a proved rounding-error guard and binary64 fallback. Never replace the certificate with an unchecked heuristic.

#### P3 concrete implementation plan after the P2 sweep

1. Add a profiling-only histogram of invocations producing `0`, `1`, `2`, and `3+` final segments at thresholds `0.6`, `1.0`, and `2.0`. Report the histogram separately for primary, closest, and visibility rays and exclude empty proxy-domain invocations when quoting fast-path coverage.
2. Implement a bounded rigorous classifier that stores at most two certified segments. It may accept an unsplit retained interval or its two midpoint children. If a retained interval requires deeper subdivision, if more than two nonempty segments are needed, or if any proof/resource condition is unresolved, classify the whole candidate-prism invocation for the existing exhaustive conservative fallback **before** traversing or reporting any fast segment.
3. Replace the common `finalSegs[32]` and `tasks[32]` allocation with two segment slots and a small fixed classifier state. Keep the old materialized path only as an isolated comparison build; the proposed production slow path is whole-invocation exhaustive fallback so the common intersection program does not reserve the large arrays.
4. Reuse a child certificate computed by the classifier; never recompute it during traversal. Apply the existing world-`t` rejection and DDA/hierarchy/leaf code unchanged to each admitted fast segment.
5. Measure direct stack bytes, spills, registers, fast-path coverage, fallback rate, work counts, and counter-free time for thresholds `0.6`, `1.0`, and `2.0`. Compare against both frozen `E0.3` and the same threshold in the materialized implementation.
6. Accept only with frozen geometry correctness, no unexplained shading-normal regression, and at least `5%` path improvement in one key scene without more than `2%` regression in the other. If fast-path coverage is too low or exhaustive fallbacks erase the stack saving, move to a stackless streamed dyadic iterator; do not restore the 32-entry arrays to the common path.

#### P3.1 coverage result and revised bounded ablation

The initial two-slot classifier is not general enough for twisted-rock. At threshold `1.0`, it covers only `47.99%` of active primary, `29.74%` of active closest, and `21.78%` of active visibility invocations. The existing whole-invocation fallback is a conservative brute-force scan of all represented leaves in the proxy triangle, so routing this much work to it would be predictably catastrophic.

A finer `0/1/2/3–4/5–8/9–16/17+` histogram identifies a better bounded ablation. At threshold `1.0`, an eight-segment cap covers `100%` of twisted primary and visibility active invocations, `99.83%` of twisted closest invocations, and at least `99.94%` of every curved-cobble family. Four slots cover only about `80%` of twisted secondary active invocations. At threshold `0.6`, eight slots cover only `97.76%` of twisted closest active invocations; at `2.0`, eight slots cover all observed work but the materialized implementation's thicker-tube timing already regresses twisted-rock.

Therefore the first storage ablation is `finalSegs[8]`, `tasks[16]`, and threshold `1.0`. The observed maximum live task count at this threshold is `13`, so 16 task slots preserve the measured non-fallback cases. Relative to the 32/32 layout, this removes 24 segment records and 16 task records—approximately `960` bytes of fixed local array payload per invocation—while expected new exhaustive fallback is around `0.1%` of twisted closest candidates. This is a capacity/storage experiment, not yet the final hierarchy-coupled segmentation algorithm. Accept it only after compiler stack/spill measurements, bitwise geometry comparison, shading-normal audit, fallback counts, and counter-free timing.

#### P3.2 bounded-storage result and next setup optimization

The cap-8 ablation is rejected. Its measured secondary direct stack falls from `2,648` to `1,752` bytes, but only `1,195` of `1,173,294` twisted closest invocations (`0.102%`) falling back raises closest leaf tests from `9.55` to `539.71` per invocation. The conservative whole-invocation fallback scans every represented leaf in the proxy triangle; therefore even apparently excellent `99.8%` bounded coverage is insufficient for this fallback architecture.

The cap-16/task-16 ablation introduces no new twisted fallbacks, preserves all twisted work counts, and is bit-identical to the materialized threshold-`1.0` geometry and normal AOVs. It reduces profiled secondary direct stack from `2,648` to `2,008` bytes. Nevertheless, counter-free path time is `+0.97%` on twisted and `+0.21%` on curved relative to the same threshold with 32/32 arrays. Relative to frozen threshold `0.3`, it is `+0.22%` on twisted and `−1.70%` on curved. It fails the `5%` gate and the source returns to the conservative 32/32 capacity.

This rejects fixed array capacity as the primary secondary-ray bottleneck. The next isolated target is redundant setup on certified-empty proxy domains. The current code constructs float canonical/texture coefficients, double UV certificate coefficients, and the cubic world-`t` numerator before acting on the proxy-domain result, even though `38–40%` of twisted secondary and `18–24%` of curved secondary candidate invocations are domain-empty.

Plan for the early-domain ablation:

1. construct only the dominant-axis double barycentric numerator/denominator coefficients needed by the proxy-domain solver;
2. solve the proxy domain and return immediately on a certified-empty result after committing profiling counters/timing;
3. only for nonempty or conservative-fallback domains, construct float canonical/texture coefficients used by exact leaves, double UV numerator coefficients used by the tube certificate, triangle-domain helpers, and the world-`t` numerator;
4. remove dead ray-`t` lambdas/dot products that have no consumers;
5. retain segmentation threshold `0.3` and 32/32 arrays for the isolated comparison; and
6. require bit-identical geometry/normal AOVs and at least `5%` path improvement in one key scene without more than `2%` regression in the other.

This ablation does not change segmentation, bounds, DDA, hierarchy admission, or exact leaf intersection. A win is attributable solely to avoiding per-candidate work after a certified empty-domain query.

#### P3.3 early-domain result and domain fast-classifier plan

The early-domain implementation passes all 26 tests, preserves every work/fallback counter, and produces bit-identical geometry and normal AOVs in both key scenes. Counter-free path time is `+0.88%` on twisted and `−1.49%` on curved relative to the frozen implementation, so deferring post-domain setup alone fails the `5%` gate. Keep it only as an isolated base for the next domain-solver experiment; if the combined experiment fails, restore the frozen ordering.

The proxy-domain solver already certifies the denominator sign over the complete height band, but it then solves and sorts all numerator roots before checking the three barycentric numerator signs. Add a constant-sign fast classifier before root isolation:

- with sign-stable `D`, if any of `A=D−B−C`, `B`, or `C` has a certified nonzero sign opposite `D` over the complete band, return a certified empty result;
- if all three numerator signs are either zero or equal to the sign of `D`, return the complete height band as one certified interval; and
- otherwise run the existing all-root solver unchanged.

The Bernstein-range sign test and its binary64 error guard already exist in `mode1_proxy_domain.h`; this experiment reorders existing rigorous logic rather than adding a heuristic. Add explicit `fast-empty`, `fast-full`, and `general-root-solver` counters, mirror the classification in the CPU reference/tests, and measure coverage before timing. Accept the combined early-domain/fast-classifier path only with identical intervals in reference tests, bit-identical AOVs, unchanged traversal work, and the standard `5%` timing gate.

#### P3.4 domain fast-classifier result (rejected)

The constant-sign classifier is mathematically sound and passes 27 tests. It avoids general root isolation for `64–74%` of twisted candidates and `32–52%` of curved candidates. It also proves more curved candidates empty than the old root-neighborhood construction (`49.3%` versus `33.7%` for primary, `31.7%` versus `18.0%` for closest, and `39.6%` versus `23.9%` for visibility) because a strict opposite sign in any one barycentric numerator makes roots of the other numerators irrelevant. Geometry and normal AOVs remain bit-identical and downstream DDA/hierarchy/leaf counts do not change.

Nevertheless, counter-free twisted path time is `+0.47%` and curved path time is `+14.75%` relative to frozen P1/P1.5. Relative to the early-domain-only build, curved is `+16.49%`. The likely GPU explanation is warp divergence and duplicated sign work: curved warps contain a substantial mixture of fast and general lanes, so the warp executes both branches, while general lanes repeat the full-band sign tests before root isolation. Root isolation itself is cheaper than its source-level complexity suggested.

Reject and revert both the fast classifier and the below-gate early-domain reordering. Do not add more per-lane branches around small domain/setup operations. The next optimization must reduce arithmetic precision/state or instruction footprint on the common certificate path, and must be designed with a rigorous fallback rather than an unchecked float replacement.

#### P3.5 invariant certificate preparation plan

Inspection of `mode1_certified_tube.h` shows that the rigorous certificate already converts binary64 coefficient centers to outward float intervals and performs all interval arithmetic with directed binary32 rounding. A proposed “float certificate” would therefore duplicate the existing implementation. The actual redundancy is that every segment certificate:

1. widens the same nine `U/V/D` coefficients to intervals; and
2. reconstructs the same two cubic rational second-derivative numerator polynomials.

These values depend only on the ray/proxy rational curve, not on segment endpoints. Twisted closest rays repeat them `7.65` times per invocation at threshold `0.3`.

Add a `PreparedCurve` containing interval `U/V/D` coefficients and precomputed cubic second-derivative numerators. Build it once after the proxy-domain solve. `computePrepared(h0,h1,prepared)` then performs only the segment-dependent denominator Bernstein range, `U/V` cubic Bernstein ranges, endpoint divisions, and conservative secant-radius calculation. Preserve the existing `compute()` as a wrapper that prepares and evaluates once, so the arithmetic contract stays centralized.

The operation order inside coefficient widening and `rationalSecondNumerator` must remain identical; therefore the prepared and old certificate should be bit-identical for every segment. Validate with the existing 26-test suite plus GPU AOV/work/fallback comparison. Measure compiler registers, stack/spills, and counter-free timing. Accept only if the standard `5%` gate is met; otherwise revert rather than retaining extra long-lived prepared state.

#### P3.5 invariant certificate preparation result (accepted)

The prepared certificate preserves the frozen geometry and normal AOVs bit-for-bit on twisted-rock and curved-cobble. Certificate, segment, DDA, hierarchy, leaf, and fallback counts are unchanged, so this is an arithmetic-reuse optimization rather than a change in traversal decisions.

The compiler reports a secondary direct-stack increase from approximately `2,648` to `2,712` bytes and direct spills from `616` to `672` bytes at `128` registers because the prepared cubics remain live across the segmentation loop. Despite that resource cost, counter-free `16`-warmup/`30`-sample timing is:

| case | frozen path p50 | prepared path p50 | change | prepared p95 | prepared CV |
|---|---:|---:|---:|---:|---:|
| twisted-rock | `744.028 ms` | `744.986 ms` | `+0.13%` | `767.460 ms` | `1.44%` |
| curved-cobble | `135.723 ms` | `122.406 ms` | `-9.81%` | `123.804 ms` | `0.64%` |

Keep the optimization. It clears the `5%` target-case gate on curved-cobble and introduces no material regression on twisted-rock. The result also refines the bottleneck diagnosis: repeated invariant certificate algebra was exposed on curved rays, while twisted secondary rays remain dominated by segment-induced hierarchy and leaf work. The next experiment must reduce materialized segment state and repeated traversal together; merely precomputing more values would extend live ranges and aggravate the stack/spill problem.

#### P3.6 streamed certificate/traversal plan

The current implementation first materializes as many as 32 accepted `HSeg` records and only then traverses them. Replace this two-phase layout with a depth-first producer/consumer loop: when a task is accepted, immediately traverse that certified segment and update the invocation-local closest hit. Preserve the task split order, exact leaf test, closed supercover, world-`t` pruning, and final OptiX report order. Do not report an intersection until all tasks finish; if any certificate or traversal capacity condition occurs, discard the partial local hit and run the existing whole-invocation exhaustive fallback. This makes recovery behavior identical to the materialized path.

The isolated hypothesis is that eliminating `finalSegs[32]` reduces local-memory traffic and shortens the lifetime overlap between certificate state and traversal state. Measure compiler registers/stack/spills first, then bitwise AOVs and exact work counters, and finally counter-free timings. A merely smaller stack is not sufficient—the cap-16 experiment already showed that storage reduction alone does not guarantee speed. Accept only with at least a `5%` path-time improvement on twisted-rock or curved-cobble and no unexplained regression above `2%` on the other case.

#### P3.6 streamed certificate/traversal result (rejected)

The implementation used a one-record staging slot, consumed each accepted certificate immediately, resumed the same depth-first task stack, and retained the invocation-wide 32-segment fallback boundary. It passes the CPU suites and is bit-identical to prepared materialization for geometry, hit mask, and shading normals in both key scenes. Every authoritative certificate, segment, DDA, hierarchy, leaf, and fallback count is unchanged across primary, closest, and visibility rays.

The compiler result initially looks attractive: specialized secondary direct stack falls from `2,712` to `1,720` bytes, crossing the `2 KB` architecture milestone. Registers remain `128`, while direct spills rise from `672` to `704` bytes. Counter-free timing shows why stack size cannot be used as a proxy for speed:

| case | prepared path p50 | streamed path p50 | change | streamed p95 | streamed CV |
|---|---:|---:|---:|---:|---:|
| twisted-rock | `744.986 ms` | `795.817 ms` | `+6.82%` | `811.524 ms` | `1.05%` |
| curved-cobble | `122.406 ms` | `125.648 ms` | `+2.65%` | `127.547 ms` | `0.70%` |

Reject and revert streaming. Interleaving makes the prepared certificate state survive across DDA traversal, repeatedly crosses the certificate/traversal control-flow boundary on multi-segment rays, and increases direct spills. The severe twisted regression and smaller curved regression are consistent with that mechanism. Keep the two-phase prepared/materialized implementation. The next target is dynamic work shared by adjacent materialized segments—especially duplicate root/node/leaf ownership—without interleaving the two phases.

#### P4.1 exact cross-segment leaf-duplication measurement plan

Before adding a production cache, measure how often materialized segments repeat the same leaf coordinate. This is a particularly clean redundancy test: `testCertifiedLeaf` receives the same world ray and the full prism `t` range for every admission, so testing leaf `(ix,iy)` twice in one candidate-prism invocation repeats the identical two exact microtriangle tests. The segment's world-`t` bound is currently used only to reject or closest-prune the segment before traversal; it is not used to narrow the exact leaf test.

Add a profiling-only exact list of 64 packed `(ix,iy)` keys per invocation. For every certified-path leaf admission, linearly check the keys, count unique and repeated tests, and report tracker overflow. Do not change or skip any leaf test in this measurement build, and do not count exhaustive-fallback scans. Report duplication per ray family and scene as `repeated / (unique + repeated)`, plus the fraction of invocations that overflow the 64-key tracker.

If repeated leaves are below `10%`, reject leaf caching and measure root/node duplication instead. If they are material, test a small safe cache: a key match may skip the repeated exact test, while a collision must be treated as a miss. Compare a direct-mapped or short-associative cache with no false-positive skips. Accept only if work falls as predicted, AOVs remain bit-identical, compiler local state remains controlled, and counter-free path timing improves by at least `5%` on the target case without a regression above `2%` elsewhere.

#### P4.1 exact cross-segment leaf-duplication result (rejected)

The 64-key exact tracker overflows on only `0.0132%` of twisted closest invocations and effectively never elsewhere. Repeated fractions are low:

| case / family | unique leaves per invocation | repeated leaves per invocation | repeated fraction |
|---|---:|---:|---:|
| twisted / primary | `0.97` | `0.02` | `2.47%` |
| twisted / closest | `2.49` | `0.02` | `0.96%` |
| twisted / visibility | `2.90` | `0.03` | `0.89%` |
| curved / primary | `0.49` | `0.01` | `1.38%` |
| curved / closest | `0.60` | `<0.01` | `0.23%` |
| curved / visibility | `0.86` | `<0.01` | `0.26%` |

Reject a production leaf cache. Its key storage and lookup cost cannot plausibly be repaid by less than `3%` redundant work. The measurement also reveals why twisted closest reports `17.31` total leaf tests per invocation while only about `2.51` certified-path tests are tracked: 33 segment-capacity fallbacks each launch a nearly full represented-triangle scan, and their rare but enormous tail dominates the aggregate leaf count.

#### P4.2 rare overflow-coalescing plan

Do not enlarge `finalSegs[32]`, because fixed local capacity has already shown poor common-path cost. Instead, when accepting a 33rd segment would overflow:

1. retain stored segments `0..30`;
2. form one height interval spanning stored segment 31, the current accepted certificate, and every pending task interval;
3. compute one rigorous prepared certificate over that union interval and apply the same proxy-box clipping/recomputation contract;
4. overwrite slot 31 with this thicker conservative tube, clear the pending task stack, and traverse the usual 32 materialized records; and
5. if the union certificate refuses or its traversal exceeds an existing bounded resource contract, use the unchanged exhaustive fallback.

This is safe because the replacement tube encloses the union of every curve portion omitted from the first 31 records; including gaps or later box-rejected portions only adds false positives. It changes only rare overflow invocations and adds no array storage. Add coalescing-attempt/success/refusal counters. Validate AOVs and confirm that successful cases remove segment-capacity fallbacks and collapse their leaf-test tail. Time only after the work reduction is observed; accept under the standard `5%`/`2%` gate.

#### P4.2 preliminary result and cold-path isolation plan

All 33 twisted and all 84 curved capacity events in the sampled frames produce rigorous union certificates. Three closest invocations in each scene later exceed the existing 8-by-8 root-ownership contract and still fall back. Twisted closest leaf work nevertheless falls from `17.31` to `3.86` per invocation (`-77.7%`), with bit-identical geometry and normals. Counter-free twisted path p50 falls from `744.986` to `127.831 ms`, but p95 remains `717.690 ms` and CV is `75.3%`; the remaining rare fallback tail makes the series bimodal. Curved path p50 regresses `4.97%` relative to prepared materialization despite negligible dynamic-work change.

The inline coalescer therefore cannot be accepted yet. First extract the union-certificate computation into a non-inlined device helper using global POD task/segment types. This should keep its temporary intervals and instruction body out of the common path. Require identical AOVs, coalescing/work counters, and lower common-path compiler resources. Re-time both scenes. Only if curved returns within the `2%` no-regression gate should the next experiment replace the last 8-by-8 failure with bounded direct root-box enumeration for coalesced tubes.

#### P4.2 cold-path isolation result (accepted, tail work remains)

The non-inlined helper preserves the exact coalescing outcomes and bit-identical AOVs. OptiX reports two non-entry functions; specialized secondary direct stack is `2,584` bytes, spills are `332` bytes, and registers remain `128`. The call frame raises stack relative to the inline coalescer, but isolating the cold instruction/state body restores curved performance:

| case | prepared path p50 | isolated coalescer p50 | change | p95 | CV |
|---|---:|---:|---:|---:|---:|
| twisted-rock | `744.986 ms` | `125.531 ms` | `-83.15%` | `721.680 ms` | `76.46%` |
| curved-cobble | `122.406 ms` | `123.528 ms` | `+0.92%` | `124.551 ms` | `0.62%` |

Keep the isolated coalescer. This is a major architectural finding: rare exhaustive fallback, not ordinary DDA cost, caused most of the previous twisted median. The high twisted p95/CV still fails a stable-performance claim, so headline speed must not use the median alone.

#### P4.3 residual root-mask tail plan

The residual fallbacks occur after successful coalescing when the thicker final tube's start-level root AABB violates the current `width <= 8`, `height <= 8`, and `area <= 64` ownership-mask contract. First add profiling maxima for rejected root width, height, and area. If every rejected box fits 128 roots with bounded dimensions, extend exact ownership to two 64-bit words; this adds only one word and preserves collision-free root ownership. If boxes are larger, use a cold bounded direct root-box enumerator only for the coalesced final segment. In either case, keep the existing exhaustive fallback beyond the new bound.

The gate is stricter than a median win: zero root-mask fallback in both sampled scenes, bit-identical AOVs, twisted p95/CV collapsing toward the fast cluster, and curved remaining within `2%` of prepared materialization.

The measured rejected rectangles are much larger than 128 roots: up to `143 x 187 = 26,741` on twisted and `1,149 x 1,391 = 1,598,259` on curved. These boxes come from the coalesced tube radius, not represented geometry. Every root square outside the proxy triangle is subsequently rejected by `testTriangleSquareIntersection2D`, so enumerating or masking the expanded box is unnecessary.

Revise P4.3 to first intersect the tube-derived root-index rectangle with the proxy texture AABB's conservative root rectangle. A root can own a represented leaf only if its closed square overlaps that AABB. Use the same one-cell lower-side pad already used for closed ownership, then apply the existing exact 64-bit mask. This removes only roots that the existing proxy-triangle test must reject. If the clamped rectangle still exceeds 64 roots, retain the current fallback and report its dimensions; only then consider cold direct enumeration.

The first clamped implementation eliminates all sampled capacity/root-mask fallbacks and reduces twisted closest leaves to `2.54` per invocation. Twisted path p50 becomes `119.909 ms` and p95 falls to `264.177 ms`. A curved timing run then reveals an empty-loop pathology: `radiusCellsX/Y` still derive from the unclamped thick radius, and the anchor loops iterate the entire `[-radiusCells,+radiusCells]` neighborhood before rejecting offsets outside the clamped root rectangle. One stochastic curved ray kept the GPU busy for more than a minute.

Clamp each anchor's `ox/oy` iteration bounds to the intersection of its radius neighborhood and `[rootMinimum-grid, rootMaximum-grid]`. This enumerates exactly the roots that survive the existing in-loop bounds test and removes only guaranteed rejects. Retain the in-loop test as a defensive check. Re-run both timings; no root-mask fallback and no long-tail launch are required for acceptance.

The neighbor-loop clamp makes curved stable and fast (`119.817 ms` path p50, `121.106 ms` p95, `0.67%` CV) and improves twisted p50 to `115.623 ms`. Twisted p95 remains `261.985 ms`, with the slow frames now caused by expensive valid union tubes rather than fallback or empty neighbor loops.

#### P4.4 relaxed resegmentation retry plan

Use the P2 threshold evidence as a rare-path recovery policy. On the first would-be 33rd segment at the default `eta=0.3`, discard the partial segment list, rebuild the original certified proxy-domain tasks, and regenerate that invocation with `eta=max(eta,1.0)`. Threshold `1.0` previously covered the key scenes within 16 stored segments and produced far tighter traversal than an unsplit/union tube. Ordinary invocations retain `eta=0.3`; only observed capacity-tail rays pay the second segmentation pass.

Keep the isolated union coalescer as a second-overflow safety net, followed by the unchanged exhaustive fallback. Add retry attempt/success/second-overflow counters. Require bit-identical AOVs, no default-scene second overflow, stable twisted p95/CV, and curved within `2%` of the accepted prepared baseline.

The first retry at `eta=1.0` handles all 33 twisted capacity events but 12 still overflow into union coalescing; capacity-tail rays are more pathological than the aggregate threshold corpus. P2 showed `eta=2.0` keeps all sampled policies within eight segments. Raise only the rare retry to `max(eta,2.0)` and repeat the no-second-overflow gate.

The `eta=2.0` retry still leaves seven second overflows in the tail subset. Replace the one-shot policy with at most three cold restarts at `eta=2`, `4`, and `8`. Each restart rebuilds the original certified proxy-domain tasks and overwrites the partial list. Union coalescing remains only after an eta-8 overflow. Count every restart and every overflow after the first; require zero union-coalescer attempts in the sampled key scenes before timing.

#### P4.4 progressive retry result (accepted as work recovery; residual latency under study)

The final policy permits at most six cold restarts at `eta=2,4,8,16,32,64`. Ordinary invocations still use the requested `eta=0.3`; a restart occurs only after a would-be 33rd materialized segment. Every restart discards the partial list and regenerates the original certified proxy-domain intervals, so no approximation is composed with an earlier approximation.

In the profiled key frames, twisted closest rays require 52 restart passes across 33 invocations and curved closest rays require 83 passes across 78 invocations. Twisted has 19 overflows after the first attempt and curved has five; visibility adds six successful curved passes. No sampled primary, closest, or visibility invocation reaches union coalescing or the capacity/root-mask exhaustive fallback. Frozen geometry and normal AOVs remain bit-identical in both scenes.

Counter-free timing is:

| case | prepared path p50 | progressive-retry p50 | change | progressive p95 | CV |
|---|---:|---:|---:|---:|---:|
| twisted-rock | `744.986 ms` | `114.632 ms` | `-84.61%` | `147.335 ms` | `59.89%` |
| curved-cobble | `122.406 ms` | `120.351 ms` | `-1.68%` | `121.630 ms` | `0.54%` |

The twisted CV is dominated by two deterministic sample positions: 28 of 30 samples lie in `111.9–121.8 ms`, while samples 0 and 16 are approximately `542.7` and `168.2 ms`. A repeat run reproduces both positions and the median within `0.2%`. Profiling the exact slow first sample after 16 warm-up frames finds no extra geometric work: closest rays average `4.11` segments, `9.39` DDA steps, `20.87` hierarchy tests, and `2.55` leaves, with 47 retry passes, zero coalescing, and zero fallback. These values match the ordinary profiled frame within sampling noise. Mode 0 on the identical frame is not slow (`7.171 ms` path under the profiling build).

Therefore P4.4 is accepted as a major and correctness-preserving work-recovery mechanism, but it does not yet justify a stable-latency claim. The remaining periodic Mode-1-only latency is not explained by counted segments, nodes, leaves, retries, or fallbacks. Treat large per-ray state, warp-latency amplification, and uncounted traversal control work as the next hypotheses. Do not hide the two samples or report the median as the complete performance story.

#### P4.5 cold recovery-code removal plan

Progressive retries make the isolated union coalescer unreachable in both sampled key scenes, yet its non-inlined call site and call frame remain compiled into every Mode 1 intersection program. Remove the union-coalescer call from the eta-64 overflow branch and go directly to the existing exhaustive conservative fallback. Keep the retry policy and all correctness behavior before that point unchanged. This is safe: an unseen eta-64 overflow becomes slower, never non-conservative.

Measure specialized stack, spills, registers, AOV identity, profiled fallback counts, and counter-free timing. Accept only if the key scenes retain zero capacity fallback and improve one path median by at least `5%` without regressing the other by more than `2%`; otherwise restore the helper as the general safety policy.

#### P4.5 cold recovery-code removal result (rejected)

Removing the call also removes the non-entry compiler boundary. OptiX then inlines/reallocates the surrounding Mode 1 body: specialized secondary direct stack rises from `2,584` to `3,192` bytes and direct spills rise from approximately `332` to `644` bytes at the same `128` registers. A fresh OptiX pipeline build terminates with Windows status `0xC0000409` before rendering. Reject and restore the helper. Its value is not only unseen-case recovery; it is an important compiler resource boundary. Future cold-path changes must preserve an explicit non-inline boundary rather than relying on dead-code elimination.

#### P4.6 proxy-inside propagation plan

The certified compact traversal calls `testTriangleSquareIntersection2D` at every root and admitted descendant. If a node is classified `SquareInsideTriangle`, every descendant square is also inside and repeating the classifier is unnecessary. First add profiling-only counters for classifier calls and outside/overlap/inside outcomes, plus the number of descendant calls whose parent is already inside. This measurement must not change admission.

If the inherited-inside opportunity is material, retain one bit per active compact depth. Pass an `inheritedInside` flag to node admission; skip the classifier only when it is true, and otherwise preserve the existing closed classifier exactly. Store the returned inside state for descendants. This does not skip min/max reads, tube-versus-cell clipping, or leaf tests. Validate identical segment/DDA/hierarchy/leaf/fallback counts and bit-identical AOVs. Accept under the standard `5%` target improvement and `2%` cross-scene regression gate; also reject if the extra compact state materially raises stack or spills.

#### P4.6 proxy-inside propagation result (rejected)

The opportunity is real: inherited inside state can avoid `11.10 / 20.86` twisted closest classifications (`53.2%`), `9.60 / 19.79` twisted visibility classifications (`48.5%`), and `36–39%` of curved secondary classifications. The implementation removes exactly those calls, preserves all segment/DDA/hierarchy/leaf/fallback counts, and produces byte-identical geometry and normal AOVs. Counter-free stack remains `2,584` bytes at 128 registers, while spills rise slightly from `336` to `344` bytes.

Wall time rejects it. Twisted path p50 improves from `114.632` to `111.957 ms` (`-2.33%`), below the `5%` target, while curved regresses from `120.351` to `125.158 ms` (`+3.99%`), beyond the `2%` guard. On the small curved proxies, the additional inherited-state branch and control dependence cost more than the skipped classifier. Revert the state, counters, and skip. This also warns against adding per-depth metadata to the current compact traversal without a larger fused saving.

#### P4.7 cap-16 plus progressive-retry plan

The old cap-16 ablation predates progressive retry and was defeated by sending rare overflow directly to exhaustive leaf enumeration. Re-test `finalSegs[16]` and `tasks[16]` with the accepted eta-2…eta-64 restart policy. Current measured maximum live tasks are 15 on twisted and 11 on curved; primary and visibility segment lists already fit 16, while a rare 17th closest segment will now trigger a conservative relaxed restart rather than exhaustive work.

This removes 16 segment records and 16 task records, approximately `640` bytes of fixed array payload per Mode 1 invocation. Profile retry levels, task/segment/capacity fallbacks, stack, spills, registers, and AOVs. Reject immediately if any key-scene invocation exhausts task capacity or reaches the exhaustive capacity fallback. Otherwise apply the standard `5%` target improvement and `2%` cross-scene regression timing gate.

#### P4.7 cap-16 plus progressive-retry result (rejected)

The combined recovery policy is correct and bounded in the key frames. Twisted closest retry passes rise from 52 to 288 and curved closest/visibility passes rise from 89 to 99, but no invocation reaches coalescing, task-capacity fallback, or segment-capacity fallback. Geometry and normal AOVs remain byte-identical. Counter-free secondary direct stack falls from `2,584` to `1,880` bytes at the same 128 registers and 336 spill bytes.

The resource reduction does not translate to throughput: twisted path p50 changes from `114.632` to `114.609 ms` (`-0.02%`), while curved changes from `120.351` to `121.663 ms` (`+1.09%`). Reject and restore 32/32. Together with P3.2, this is now repeated evidence that fixed array/stack capacity is not the surviving common-path limiter; do not optimize stack size without a simultaneous arithmetic or control-flow reduction.

#### P4.8 start-LOD work-balance plan

The current root scale stops at the first level satisfying `footprint <= 4 * cellScale`. A finer start creates more DDA anchors/roots but shortens each min/max descent; a coarser start does the reverse. Sweep compile-time factors `2`, `4` (frozen), and `8` on the same frames. First test factor 8 because profiled hierarchy time dominates DDA setup; test factor 2 only if factor 8 shifts too much work into anchors.

For each factor report segments, DDA anchors, hierarchy tests, leaves, fallback counts, stack/spills, AOV identity, and counter-free p50/p95. The start level changes scheduling only, not the certified tube or exact leaves, so any AOV difference is a correctness failure. Keep a non-default factor only under the standard `5%`/`2%` timing gate.

Factor 8 is rejected before timing. Its finer roots violate the 8-by-8/64-root exact ownership bound in hundreds of thousands of invocations and trigger exhaustive work. Factor 2 has zero fallback and changes twisted closest DDA/hierarchy from `9.38/20.74` to `5.64/19.02`; curved closest changes from `1.60/3.80` to `0.98/3.90`. Geometry AOVs are bit-identical. Traversal order changes the equal-distance microtriangle selected at only two twisted and four curved pixels; the affected fractions are `0.00034%` and `0.00073%` of valid hits. This is a deterministic ownership issue, not a geometry miss.

Counter-free factor-2 timing clears the gate: twisted path p50 is `101.086 ms` (`-11.82%`) and curved is `111.469 ms` (`-7.38%`) relative to accepted factor 4. Stack/spills/registers remain `2,584/336/128`. Before accepting, make exact equal-`t` leaf ownership independent of traversal order: expose an equal float hit despite the leaf test's open upper bound, then choose a fixed lexicographic `(leaf y, leaf x, microtriangle)` owner. A strictly farther hit must never replace the closest hit. Apply the same rule to factor 2 and factor 4 and require byte-identical geometry and normal AOVs between them; then re-time the final factor-2 build to account for the tie logic.

#### P4.8 start-LOD work-balance result (accepted)

The final implementation uses factor `2` and makes exact equal-distance leaf ownership deterministic. The leaf query widens the current closest upper bound by one float step only to expose an exactly equal hit; a lexicographic `(leaf y, leaf x, microtriangle)` key then selects the winner. Strictly farther hits cannot replace the closest result. With this rule enabled in both builds, factors `2` and `4` produce byte-identical geometry and shading-normal AOVs on twisted-rock and curved-cobble. The earlier handful of normal differences was therefore traversal-order tie breaking, not a represented-surface or coverage difference.

The accepted counter-free `16`-warmup/`30`-sample result relative to factor `4` is:

| case / component | factor-4 p50 | factor-2 p50 | change | factor-2 p95 | factor-2 CV |
|---|---:|---:|---:|---:|---:|
| twisted-rock G-buffer | `15.745 ms` | `13.329 ms` | `-15.34%` | `13.470 ms` | `0.82%` |
| twisted-rock path | `114.632 ms` | `101.241 ms` | `-11.68%` | `133.378 ms` | `65.73%` |
| twisted-rock total | `130.537 ms` | `114.816 ms` | `-12.04%` | `146.948 ms` | `58.91%` |
| curved-cobble G-buffer | `23.766 ms` | `24.681 ms` | `+3.85%` | `25.573 ms` | `2.02%` |
| curved-cobble path | `120.351 ms` | `111.747 ms` | `-7.15%` | `112.771 ms` | `0.72%` |
| curved-cobble total | `144.384 ms` | `136.589 ms` | `-5.40%` | `138.138 ms` | `0.74%` |

Accept factor `2`. It reduces anchors enough to lower total hierarchy-control work even though some roots descend farther. The curved G-buffer component alone regresses, but the full curved frame clears the `5%` gate and the path component improves by more than `7%`. No sampled coalescer, task-capacity, segment-capacity, or root-mask fallback is introduced. The deterministic twisted samples `0` and `16` remain the source of the high path/total CV; this policy improves their p95 but does not explain or eliminate the periodic Mode-1-only latency.

#### P4.9 final Mode-0 gap and P5 attribution gate

Re-time Mode 0 and the accepted Mode 1 with the identical executable, cameras, `eta=0.3`, `16` warm-up frames, and `30` measured frames. Mode 1 remains substantially slower:

| case / component | Mode 0 p50 | Mode 1 p50 | M1 / M0 |
|---|---:|---:|---:|
| twisted-rock G-buffer | `1.435 ms` | `13.329 ms` | `9.29x` |
| twisted-rock path | `5.506 ms` | `101.241 ms` | `18.39x` |
| twisted-rock total | `7.216 ms` | `114.816 ms` | `15.91x` |
| curved-cobble G-buffer | `3.516 ms` | `24.681 ms` | `7.02x` |
| curved-cobble path | `19.395 ms` | `111.747 ms` | `5.76x` |
| curved-cobble total | `23.173 ms` | `136.589 ms` | `5.89x` |

The accepted factor-2 profiles report only `0.50–0.99` primary, `0.78–2.52` closest, and `0.91–2.93` visibility leaf admissions per candidate invocation. Diagnostic stage timers attribute only `0.1–0.4%` of accepted Mode-1 intervals to exact leaf testing, while certificate/segmentation accounts for about `19.5–30.4%` and DDA/hierarchy control for about `68.7–80.1%`. The absolute in-kernel timer magnitudes are not authoritative because of known long-lived-timestamp issues, but the work counts and the extremely small leaf fraction agree.

Close P5 as a measured no-go for the present implementation. Even eliminating exact leaf reconstruction entirely cannot plausibly recover a `5.8–18.4x` path gap, and adding proxy precomputation or caches risks more instance memory and per-thread state. Retain the leaf ideas as later engineering polish only if a future hierarchy changes the admission count or an Nsight profile contradicts this attribution. The next architecture experiment is P6: determine whether a first-order residual hierarchy can remove enough hierarchy work and permit a cheaper admission calculation; do not claim that first-order bounds alone can bridge the current multi-fold gap.

### P4 — traversal deduplication and hybrid root enumeration

1. Measure duplicate roots, nodes, and leaves across adjacent segments.
2. If duplication is material, traverse a small segment packet jointly or give roots invocation-global ownership with an active-segment mask. Descend each root once while preserving each segment's height/`t` interval.
3. Select between DDA and bounded root-box enumeration using a cheap predicted operation count.
4. Propagate `SquareInsideTriangle` from a parent to descendants to remove repeated proxy-domain tests.
5. Tune the start LOD using measured costs instead of the fixed factor `4`.
6. Process certified intervals near-to-far and prune by conservative `t` bounds.
7. Use a visibility-specific path that reports the first certified exact hit immediately.

### P5 — leaf hot-path reduction

1. Precompute effective displacement parameters per geometry instance.
2. Precompute transformed UVs, inverse UV-to-barycentric coefficients, UV edge half-planes, and UV AABB per proxy.
3. Replace four repeated UV inversions per leaf with affine increments from one corner.
4. Fuse the two microtriangle tests to share ray/cell values.
5. Evaluate texture gather or warp-coherent corner reuse; reject any design that raises local memory enough to lose the arithmetic saving.
6. Keep proxy-domain ownership closed and retain the repaired seam behavior.

### P6 — first-order residual hierarchy

Only after the same min/max traversal is lean, integrate the first-order representation:

\[
d(u,v)=p(u,v)+r(u,v),\qquad r\in[r_{\min},r_{\max}],
\]

and perform DDA/hierarchy admission in plane-compensated residual space. Compare zero-order min/max and first-order residual bounds with identical ray intervals, segmentation, traversal scheduling, leaf tests, and GPU layout.

This ordering cleanly separates two claims:

- **execution claim:** certified piecewise shell-ray traversal can be efficient; and
- **representation claim:** first-order bounds reject enough additional work to justify their storage and arithmetic.

If first-order reduces node/leaf tests but not wall time, its implementation cost still fails the practical claim. If it wins only on slope-aligned or grazing cases, those cases become a bounded conditional result rather than a universal speed claim.

#### P6 evidence gate result (closed without GPU port)

The required representation gate already exists and is stronger than a source-level GPU experiment. P-D2 replayed the same certified tube-supercover idea against scalar min/max, componentwise first-order, and hybrid interval clipping over 864 frozen practical rays. On the coherent cohort, the current hybrid uses `0.9906x` scalar nodes and `0.9622x` scalar leaf cubics: only `0.94%` and `3.78%` work reductions, with negative predicted break-even margin before charging the extra first-order payload or arithmetic.

The subsequent representation oracle rules out plane fitting as the missing ingredient. An optimized hybrid single plane uses `0.9841x` nodes and `0.9611x` leaves. More decisively, an unattainable recertified convex support hull, counted as if all of its facets cost one scalar node test, still uses `0.9818x` nodes and `0.9548x` leaves. Thus even the ideal affine-support envelope removes only `1.82%` of coherent nodes and `4.52%` of coherent leaf cubics.

Do not implement the planned `float4 (h0,gu,gv,r)` CUDA hierarchy on the fixed square mip topology. Its extra `2x` payload relative to `float2` min/max, projected tube widening, and residual arithmetic cannot repay an oracle upper bound below `5%`, and it cannot bridge the measured `5.8–18.4x` Mode-0 path gap. Retain the plane-compensated formulation and grazing/boundary regime results as a negative ablation. A future first-order branch requires a genuinely new topology or nonconvex representation and a new gate; it is not an optimization stage of the current method.

## 6. Acceptance gates

### Correctness

- All 13 current CPU Mode-1/A1/A2 tests pass.
- Optimizations that should preserve traversal ownership must reproduce the frozen M1 leaf set exactly in the CPU replay.
- GPU AOVs remain bit-identical where operation order is unchanged; otherwise compare against Mode 0 with the existing mask, relative-position, and normal-error reports.
- Certificate, capacity, step-limit, and stack fallbacks are explicitly counted; no silent partial result is permitted.
- The curved-cobble 64-spp HDR image remains free of the repaired proxy-grid seam.

### Measurement

- At least 16 warm-up and 30 measured frames.
- Report p50/p95 and coefficient of variation; target timing CV below `1%`.
- Use a counter-free build for headline time and a separate sampled profiling build for attribution.
- Report primary, radiance, and visibility ray families separately before aggregating full-frame performance.

### Performance milestones

1. **Measurement milestone:** method-specialized resource reports and complete ray-family costs.
2. **Architecture milestone:** reduce M1 direct stack below `2 KB`, registers below `96` if feasible, and compiler-reported direct spills toward zero.
3. **Traversal milestone:** duplicate roots/leaves below `5%`; total M1 node and leaf work no worse than Mode 0 on both frozen stress scenes.
4. **Intermediate speed milestone:** M1 path time within `2x` Mode 0 on twisted-rock and within `1.25x` on curved-cobble.
5. **Paper gate:** parity or a clear win on a representative subset, with an explained quality/correctness/storage tradeoff and no regression hidden by averaging.

Accept an isolated optimization only if its median counter-free path time improves by at least `5%` on its target case and it causes no unexplained regression above `2%` on another key case. Small arithmetic changes below that threshold should be batched only after profiling shows they hit the same bottleneck.

## 7. Experiment order

The next implementation order is:

1. P0 measurement repair and method-specialized entry points;
2. P1 certified triangular-domain and world-`t` intervals;
3. CPU/sampled replay of P2 segmentation policies;
4. implement the winning P2 policy with a one/two-segment fast path;
5. P4 cross-segment deduplication, hybrid enumeration, and visibility early-out;
6. P5 leaf arithmetic reduction; and
7. P6 first-order residual hierarchy and controlled paper ablation.

Do not begin with isolated leaf algebra or first-order data construction. The present evidence says the largest unknowns are secondary/visibility-ray behavior, eager segmentation, repeated cross-segment traversal, and GPU local state. Those must be measured and reduced first.

## 8. What can be claimed now

The final certified implementation establishes conservative tube construction, closed supercover traversal, deterministic equal-hit ownership, bounded recovery behavior, exact represented-leaf intersection, and repaired proxy ownership. The recovery pass reduces initial Mode-1 path median by `88.0%` on twisted-rock and `43.9%` on curved-cobble, which validates several important implementation diagnoses.

It does **not** establish a competitive performance contribution. Final path time is `18.39x` Mode 0 on twisted-rock and `5.76x` on curved-cobble. The same-surface A2 architecture also fails its held-out ordinary and grazing gates, and the first-order/support-hull representation oracle has negative break-even margin. TFDM and RMIP superiority is untested and must not be claimed. The consolidated claim boundary is [[Note — Final ray application decision after Mode 1 recovery]].

## 9. P7 — zero/one-segment OptiX fast path

P7 is a new architecture hypothesis, not a reinterpretation of the failed first-order or leaf gates. The accepted profiles show that curved-cobble produces zero or one segment in `92.80%` of primary, `96.82%` of closest, and `94.58%` of visibility candidate invocations. The current full intersection still compiles and allocates the general `tasks[32]`, `finalSegs[32]`, progressive retry, and cold recovery control around those common cases. The isolated A2 implementation, whose corpus exercised one segment, is much closer to `NRT-QT` than the full OptiX implementation. This motivates separating common and exceptional control flow at a real device-function boundary.

### P7.0 static and compiler audit

1. Trace the exact call graph from each specialized Mode-1 OptiX intersection entry to certificate generation, materialized segment storage, and traversal.
2. Record which fixed arrays and prepared coefficients are live on the zero/one-segment path.
3. Confirm whether `traverseMode1CertifiedSegment` is used or duplicated inline.
4. Capture the current specialized registers, direct stack, spills, non-entry functions, and instruction count from a fresh build.

No behavior changes in P7.0.

#### P7.0 result

The general arrays are declared directly in `detailedSurface_generic`, before the Mode-1 traversal branch, and the same entry contains certificate generation, retry, materialization, traversal, and reporting. Current fresh-build-equivalent compiler records are `128` registers, `2,520 B` primary / `2,584 B` secondary direct stack, `272 B` / `332 B` direct spills, and approximately `10,882` / `11,054` entry instructions. An inline early branch would skip dynamic stores but would not remove the entry's worst-case stack or instruction footprint.

`traverseMode1CertifiedSegment` exists but has no call site. It is a superseded experimental implementation: factor-4 roots, an explicit 48-node stack, unclamped tube/root neighborhoods, planar leaves, and no deterministic equal-hit owner. It cannot be enabled as the fast path without bringing it to the accepted P4.8 correctness contract.

Extracting the entire accepted multi-segment traversal into an out-of-line helper would move roughly 600 lines plus a large argument/live-state interface and repeats the failed P3.6 producer/consumer pressure risk. Use a narrower architecture first: a new lean certified-one-segment entry with exact nonlinear leaves and an out-of-line `NRT-QT` fallback. This removes general 32/32 state entirely rather than relocating it.

### P7.1 common-path design

For each certified proxy-domain interval:

1. evaluate one prepared whole-interval certificate without allocating a task or final-segment array;
2. if it is accepted and survives the proxy box, immediately consume that one segment through the unchanged certified traversal;
3. if it rejects geometrically, return zero work for that interval;
4. if it requires subdivision, call a `__noinline__` general helper containing the current 32/32 subdivision, retry, coalescing, and fallback policy; and
5. do not report an OptiX intersection until every proxy-domain interval has been processed, preserving closest-hit and recovery semantics.

If more than one proxy-domain interval survives, either process individually through the same one-segment attempt or route the entire invocation to the general helper—choose after the call-graph/resource audit. A slow-path call may recompute the first refused certificate; this deliberately exchanges rare arithmetic for a shorter common live range.

The fast path must not use a weaker certificate, a larger `eta`, an approximate leaf, or a smaller fallback contract. It changes scheduling and storage only.

#### P7.1 revised implementation target after P7.0

Implement `CERT1-HYB`, initially behind a compile-time experiment switch:

1. Use only cheap pre-certificate features—proxy texture footprint and ray incidence against the proxy's average normal—to route clearly adverse large/grazing cases directly to the audited nonlinear quadtree.
2. For the remaining cases, compute the certified proxy-domain intervals and attempt the accepted whole-interval certificate without task or segment arrays. Permit zero rejected intervals or exactly one accepted segment. A second accepted interval, a required split, any refusal, or any resource condition routes to nonlinear traversal.
3. Traverse the accepted tube with factor-2 roots, proxy-clamped root/neighborhood bounds, closed ownership, and exact `testNonlinearRayVsMicroTriangle` leaves. This makes the fast and fallback paths intersect the same nonlinear shell surface; planar-leaf Mode 1 is not used in this comparison.
4. If the certified traversal detects a resource/step condition, discard its partial local result and call nonlinear traversal.
5. Keep the existing full piecewise Mode 1 available as a labeled research path. `CERT1-HYB` is an architectural optimization candidate, not a silent replacement of the piecewise ablation.

The first threshold is deliberately conservative: proxy footprint at most 64 leaf texels and incidence cosine at least `0.8`. Measure routing and sweep only on a development corpus before freezing a paper comparison. A first implementation may compile the threshold as constants; it must report routed-fast versus nonlinear-fallback counts in the profiling build.

#### P7.1a dedicated-entry scaffold result (accepted)

Mode `4` (`CERT1-HYB`) now has six dedicated OptiX intersection programs: primary, closest, and visibility variants for displacement and shell mapping.  In the scaffold each program calls only the nonlinear implementation.  This establishes a real SBT/compiler resource boundary before any certificate or DDA code is added.

The scaffold compiles and is byte-identical to Mode 0 in both geometry and shading-normal AOVs on twisted-rock and curved-cobble.  A fresh OptiX compilation gives the Mode-4 displacement closest/visibility entries exactly the Mode-0 resource report: `128` registers, `376 B` direct stack, `228 B` direct spills, and `11,357` instructions.  Shell entries likewise match Mode 0 at `504 B` direct stack, `248 B` spills, and `9,528` instructions.  The recovered full Mode-1 secondary entries in the same pipeline use about `2,696 B` direct stack and `448 B` spills.

The extra mode/SBT selection has no measurable timing penalty under the 16-warm-up/30-sample protocol:

| case | Mode-0 path p50 | Mode-4 scaffold path p50 | change | Mode-4 p95 | Mode-4 CV |
|---|---:|---:|---:|---:|---:|
| curved-cobble | `19.517 ms` | `19.426 ms` | `-0.47%` | `19.721 ms` | `1.33%` |
| twisted-rock | `5.554 ms` | `5.538 ms` | `-0.28%` | `5.593 ms` | `0.80%` |

Therefore later Mode-4 changes can be attributed to the fast path rather than integration overhead.  Keep the fallback behind an explicit `__noinline__` boundary once conditional certification is introduced; otherwise OptiX may inline both algorithms into one high-resource entry.

### P7.2 staged gates

1. **Compiler gate:** common specialized direct stack below `1.5 KB`, lower spills or instructions, and no fresh-pipeline compiler failure. If the compiler inlines the general helper or charges its arrays to the caller, stop and test a separate OptiX entry/program dispatch rather than adding annotations blindly.
2. **Correctness gate:** byte-identical geometry and shading-normal AOVs against Mode 0 on twisted-rock and curved-cobble, plus all mathematical regression suites.  `CERT1-HYB` uses exact nonlinear leaf tests and therefore represents the Mode-0 nonlinear surface; it is intentionally not compared byte-for-byte with P4.8's planar-leaf full Mode 1.  Profiling must instead show that fast and fallback routing is exhaustive, that every fast tube is certified, and that no resource-refused partial result is reported.
3. **Coverage gate:** report fast accept, fast empty/reject, and slow-helper fractions by primary/closest/visibility family. Curved-cobble must keep at least `90%` of invocations out of the general helper; otherwise the source profile and the implementation classification disagree.
4. **Timing gate:** at least `10%` curved-cobble path improvement and no twisted-rock regression above `2%`, using 16 warm-ups and 30 counter-free samples. Report p50/p95/CV and retain the deterministic twisted tail.

### P7.3 follow-on only after P7.2 passes

- Add a visibility-specific near-to-far early-report path only if profiling shows it can terminate before most admitted nodes.
- Test a two-segment inline capacity only if the one-segment path passes and the extra state stays register-resident.
- Re-run the Mode-0 gap before considering broader baseline work.

If P7.2 fails, the next step is not more subdivision tuning. Use an isolated ray-buffer-to-OptiX integration or a separate custom-intersection program for the fast regime; otherwise accept that per-candidate certification inside the current intersection program is the dominant architectural mismatch.

### P7.4 scalar CERT1-HYB implementation result (rejected)

The scalar implementation removed `tasks[32]` and `finalSegs[32]`, accepted only zero or one certified proxy-domain interval, used the repaired factor-2/proxy-clamped closed-supercover traversal, and replaced planar leaf tests with exact `testNonlinearRayVsMicroTriangle` calls.  Large proxies and rays below incidence cosine `0.8` were intended to call a `__noinline__` nonlinear recovery helper.

OptiX cannot preserve that recovery boundary.  Its compiler explicitly reports that every call to the helper is forcibly inlined because the helper calls `optixGetObjectRayOrigin`.  The resulting displacement entry uses `128` registers, `2,184 B` direct stack, `476 B` spills, and `124,148` entry instructions.  This fails the `<1.5 KB` compiler gate and is much larger than either specialized Mode 0 or full Mode 1 in static code.

Correctness is very close but not accepted.  Twisted-rock is byte-identical to Mode 0 because its large proxy always takes nonlinear recovery.  Curved-cobble differs at seven of `2,073,600` pixels (`0.0003376%` mask mismatch); `99%` of common-hit position errors are zero, but a conservative method cannot dismiss seven boundary misses.

Counter-free 16/30 timing is decisively negative:

| case | Mode-0 path p50 | scalar-hybrid path p50 | change | Mode-0 G-buffer p50 | scalar-hybrid G-buffer p50 | change |
|---|---:|---:|---:|---:|---:|---:|
| curved-cobble | `19.517 ms` | `87.540 ms` | `+348.53%` | `3.545 ms` | `29.297 ms` | `+726.43%` |
| twisted-rock | `5.554 ms` | `5.746 ms` | `+3.47%` | `1.445 ms` | `1.535 ms` | `+6.22%` |

The profiling build explains why threshold tuning is not the next step.  On curved-cobble the one-tube path is used by only `20.54%` of primary, `3.81%` of closest, and `5.93%` of visibility invocations.  Across all routed/fallback work, hierarchy tests change from Mode 0's `9.48 / 10.26 / 8.60` per primary/closest/visibility invocation to `6.39 / 9.76 / 8.02`; the meaningful `32.6%` primary reduction is not repeated on secondary rays.  Admitted leaves rise from `0.29 / 0.35 / 0.26` to `0.36 / 0.37 / 0.34`.

An API-free `__noinline__` boundary around the tube traversal itself is preserved by OptiX and moves about `33k` instructions into non-entry functions.  It reduces entry spills from `476` to `288 B` and entry instructions from `124,148` to `108,910`, but raises direct stack to `2,328 B`.  Timing rejects it: curved G-buffer and path medians regress another `16.44%` and `2.62%` relative to the inline scalar version.

P7 therefore fails all gates: compiler resources, exact correctness, coverage, and timing.  Do not implement the large API-free nonlinear-body refactor; the fast tube has no positive aggregate signal that would justify it.  Restore Mode 4 to the accepted fallback-only scaffold so it remains byte-identical and timing-neutral relative to Mode 0.

### P8 matched-ray lean-regime gate (next and final ray-side architecture test)

The renderer-wide hybrid conflates two questions: whether a certified one-tube traversal is profitable on rays that actually satisfy its regime, and whether OptiX can mix that traversal with recovery in one intersection entry.  P7 proves the latter is currently false but does not isolate the former.  Before any further renderer implementation, capture a deterministic ray/candidate-prism replay corpus and compare the same admitted invocations under:

1. specialized nonlinear quadtree (`NRT-QT`);
2. scalar one-tube traversal with no recovery code in the entry; and
3. certificate-only overhead plus nonlinear traversal, to quantify the dispatch tax.

Bin by ray family, proxy span, incidence cosine, tube ratio, and Mode-0 node/leaf counts.  Report device time per invocation and matched node/leaf work.  The go gate is at least `1.25x` one-tube speedup on a nontrivial bin containing at least `10%` of curved-cobble secondary invocations; otherwise close the DDA ray-performance direction.  This replay is an upper-bound experiment: it does not claim an implementable renderer speedup and must precede any new OptiX/SBT architecture work.

#### P8.0 audit and distinction from A2

The existing A2 packed-surface experiment is the implementation starting point, not the P8 answer.  It already provides same-input CUDA kernels, exact-output checksums, randomized paired timing, and a streaming certified hierarchy.  It found a strong front-ray result (`CERT-DDA/NRT-QT = 0.383x`) but losses for oblique (`1.279x`), near-miss (`1.049x`), and grazing (`1.624x`) decision rays.  Its 864 rays were constructed from ray-family templates, every ray accepted one segment, and none came from the current OptiX path tracer.  P8 asks whether a large enough share of *actual curved-cobble secondary candidate invocations* resembles A2's profitable front regime.

Do not rerun A2 under a favorable cohort and call it P8.  P8 must capture the renderer distribution and reuse the A2 timing machinery only after the records have been frozen.

#### P8.1 frozen renderer capture contract

Build a profiling-only capture configuration; the normal timing build remains unchanged.  At each Mode-0 mapped-surface intersection invocation, record the already folded shell and ray inputs:

- base-shifted object-space proxy positions `p_A,p_B,p_C`;
- scaled displacement directions `n_A,n_B,n_C`;
- transformed texture coordinates `tc_A,tc_B,tc_C`;
- object-space ray origin and direction plus `t_min,t_max`;
- primary/closest/visibility family, launch pixel, frame index, geometry slot, and primitive index; and
- proxy texture span and incidence cosine as audit metadata.

Select launch pixels with a fixed integer hash of `(family, launch_x, launch_y, frame_index)` and retain every candidate invocation generated by a selected ray.  This samples rays independently of GPU scheduling.  Atomic append order is non-deterministic, so canonicalize records on the host by their complete binary32 payload and metadata before hashing.  Capture eight deterministic frames with a target sampling rate of `1/256`, allocate at least 524,288 records, and require zero dropped records.  A repeated capture must produce the same canonical content hash and per-family counts.

The frozen scene is curved-surface plus `disp_cobble.png`, the camera and scale used by the repaired gallery, Mode 0, and the same path-depth/environment settings as the accepted full-renderer timing.  Capture instrumentation is never timed.

#### P8.2 exact displacement-window packing

Convert each captured proxy to the existing 64-cell A2 window representation without changing its represented surface:

1. unwrap repeated texture coordinates to the minimum continuous proxy span;
2. admit only proxies spanning at most 64 source texels in both axes;
3. choose an integer-aligned 64-by-64 source-texel window containing the complete proxy footprint;
4. read its 65-by-65 normalized height samples with the renderer's repeat convention; and
5. affinely remap the proxy texture coordinates into the local unit window.

Because positions already include `baseHeight` and directions already include displacement `scale`, the packed height remains the normalized source value.  Validate the packing on a sample of records by comparing source-texel bits, proxy bounds, and the Mode-0 closest result.  Records crossing a wrap seam are retained only when unwrapping is unique and the repeated 65-by-65 sample audit passes; otherwise classify them as ineligible rather than silently altering coordinates.

#### P8.3 matched CUDA replay and measurements

Extend the A2 timing probe with a new input schema and three kernels over identical record order and buffers:

1. `NRT-QT`: the specialized nonlinear min/max quadtree baseline;
2. `CERT1-DDA`: one whole-interval certificate followed by the streaming closed tube-supercover hierarchy and exact nonlinear leaf tests, with no recovery body compiled into the kernel; and
3. `CERT+NRT`: compute and discard the same certificate, then execute NRT-QT, measuring the unavoidable classification/dispatch tax separately.

First run untimed correctness outputs and require equal status, closest hit, deterministic owner, and checksum for every admitted record.  Then use 20 warm-ups, 50 randomized paired trials, batches calibrated above 10 ms, and report median nanoseconds per invocation, CV, register count, local bytes, active blocks per SM, nodes, leaves, DDA steps, and certificate acceptance.  Timing cohorts are generated only from predeclared feature bins; no per-ray measured runtime may be used to define a bin.

Use these bins, merging adjacent bins when needed to keep at least 4,096 replay records:

- ray family: primary / closest / visibility;
- proxy span: `1–16`, `17–32`, `33–64` texels;
- absolute incidence cosine: `[0,.5)`, `[.5,.8)`, `[.8,.95)`, `[.95,1]`;
- certified tube-to-proxy-width ratio: `[0,.02)`, `[.02,.05)`, `[.05,.1)`, `[.1,+inf)`; and
- Mode-0 work: node and leaf-count quartiles frozen from the correctness replay.

#### P8.4 decision and allowed follow-on

The DDA direction advances only if an a-priori describable bin:

- contains at least `10%` of all captured curved-cobble closest-plus-visibility invocations;
- has exact output equality and no recovery;
- achieves `CERT1-DDA / NRT-QT <= 0.80x` (at least `1.25x` speedup); and
- retains positive margin after charging the measured `CERT+NRT - NRT-QT` certificate tax.

If it passes, implement only that cheap geometric classifier and place the fast cohort in a separate launch/program so recovery cannot be inlined into the lean kernel.  The next renderer gate would be at least `10%` path-time improvement with no key-case regression above `2%`.  If no bin passes, stop optimizing segmentation, DDA scheduling, or OptiX integration for a speed claim: P7 plus P8 will then show that neither the integrated implementation nor its lean renderer-derived upper bound is competitive with NRT-QT.

#### P8.5 measured decision — renderer-derived piecewise replay passes

P8 passes after replacing indiscriminate one-chord dispatch with a bounded family policy and allowing one midpoint split on certificate refusal. The frozen policy is:

- primary: attempt one certified chord only when proxy incidence is below `0.30`;
- closest and visibility: below incidence `0.15`, try one chord, then split the displacement interval once at its midpoint if the parent certificate refuses;
- otherwise use `NRT-QT`; if either child still refuses, use `NRT-QT`;
- the two child intervals meet at the midpoint and exact leaf-root ownership/deduplication handles the shared boundary.

The policy was developed on curved-surface/cobble and then frozen. Three independent 50-trial timing repetitions per ray family produce:

| renderer-derived corpus | records | primary / NRT | closest / NRT | visibility / NRT | isolated-family aggregate / NRT |
|---|---:|---:|---:|---:|---:|
| curved-surface / cobble | 112,191 | `1.0129x` | `0.9213x` | `0.6422x` | `0.8185x` |
| curved-surface / rock | 100,616 | `1.0132x` | `0.7651x` | `0.7753x` | `0.7856x` |
| sphere / terrain | 23,376 | `0.3393x` | `0.4555x` | `0.3867x` | `0.3962x` |

Every timed candidate family has the same canonical hit checksum as NRT-QT. On curved-cobble, closest requests 1,631 splits and reduces nodes from `6.97M` to `5.40M` and leaf tests from `1.60M` to `1.20M`; visibility reduces nodes from `4.58M` to `4.08M` and leaves from `0.892M` to `0.760M`. On curved-rock only 292 closest and 20 visibility rays request a split, showing that the bounded path remains cheap when one chord usually certifies.

The deterministic twisted-quad/rock capture found a packing bug: its proxy triangles span the full 1,024-texel period, while the replay supports exact 64-cell windows. Repeat unwrapping had incorrectly collapsed UV endpoints `0` and `1`. The packer now rejects raw spans above 64 cells before unwrapping; four regression tests pass, curved-cobble retains its exact original input hash, and all 63,279 twisted records are correctly classified as ineligible. Do not time or cite the invalid pre-fix twisted pack.

The aggregate column is an optimistic sum of isolated family replay launches, not renderer wall time. Authoritative machine-readable evidence is in `experiments/ray_architecture_p8/final_report.json`; the compact table is in `experiments/ray_architecture_p8/final_report.md`.

### P9 — renderer integration of the frozen bounded policy

P9 adds a new intersection mode; Mode 0 and restored Mode 4 remain untouched references. Plan before implementation:

1. map the replay certificate, one midpoint split, streaming DDA/min-max descent, nonlinear exact leaf test, and deterministic owner rule to existing production helpers;
2. specialize primary, closest, and visibility entry points at compile time so primary contains only the one-chord branch and secondary entries contain the bounded split branch;
3. keep large legacy segmentation arrays and general recovery out of the new entries; a certificate refusal calls the existing audited nonlinear traversal;
4. compile capture and traversal statistics out of the timing build, and report registers, direct stack, spills, and instruction counts before rendering;
5. require byte-identical position/mask and shading-normal AOVs against Mode 0 on curved-cobble, curved-rock, sphere-terrain, and twisted-rock; run the selected mathematical regression suite;
6. time 16 warm-ups and 30 samples. Advance only with at least `10%` path-time improvement on one qualifying curved case, no key-case path regression above `2%`, and no new latency tail;
7. if integrated OptiX control/resource pressure erases the replay signal, try only one code-layout pass: separate fast and nonlinear program bodies at a real device/program boundary. Do not reintroduce the general 32-segment implementation.

#### P9.0 first integrated result — correct common cases, rejected layout

Mode 5 integrates the frozen P8 policy as compile-time-specialized primary, closest, and visibility intersection entries while leaving Modes 0 and 4 registered separately. Capture and traversal counters are compiled out. The selected certificate, lazy split, closed-supercover, proxy-domain, world-`t`, hierarchy, and capture-packing suite passes `29/29` tests.

The exact primary AOV gate is byte-identical to Mode 0 on curved-cobble, curved-rock, and twisted-rock. Sphere-terrain has zero mask differences but 1,331 bitwise position differences: 1,326 are at floating-point tie/noise scale and four select a materially different root. A fresh independent dense oracle with 1,048,576 triangles favors Mode 5 at all four material pixels. For example, at pixel `(1146,480)`, Mode-0/oracle position error is `0.034031`, versus `0.000166` for Mode 5. Preserve this as an adjudicated Mode-0 nonlinear-hierarchy discrepancy; do not silently relabel the strict byte-equality result as a pass.

The first compiler/timing layout fails decisively. OptiX reports that it forcibly inlines `detailedSurface_cert1_nonlinear_fallback` because the helper calls `optixGetObjectRayOrigin`. Mode-5 displacement entries therefore contain both algorithms:

| entry | registers | direct stack | direct spills | entry instructions |
|---|---:|---:|---:|---:|
| Mode-5 primary | `128` | `1,512 B` | `512 B` | `87,953` |
| Mode-5 closest/visibility | `128` | `2,152 B` | `452 B` | `104,644` |
| Mode-0 displacement | `128` | `376 B` | `228 B` | `11,357` |

Counter-free `16`-warm-up/`30`-sample renderer timing:

| case | Mode-0 path p50 | Mode-5 path p50 | Mode-5 / Mode-0 |
|---|---:|---:|---:|
| curved-cobble | `19.619 ms` | `178.925 ms` | `9.1202x` |
| curved-rock | `17.799 ms` | `96.603 ms` | `5.4275x` |
| sphere-terrain | `3.363 ms` | `22.887 ms` | `6.8060x` |
| twisted-rock | `5.568 ms` | `5.801 ms` | `1.0417x` |

This rejects the first layout, not the isolated bounded traversal: P8 timed the same ray families without an inlined recovery body and was positive. Use the single allowed layout pass now:

1. parameterize the existing nonlinear implementation with a compile-time API-free context path;
2. preserve the present API-facing instantiation for Mode 0;
3. expose a `__noinline__` API-free fallback that takes geometry, primitive, ray, and `t` range and returns a hit record;
4. let only the Mode-5 intersection entry call `optixReportIntersection`;
5. require the compiler log to retain this helper as a non-entry function, then repeat the same correctness and `16/30` timing gates without changing routing thresholds.

If OptiX still inlines the core, or if the second timing remains outside the gate, stop this in-pipeline hybrid architecture. Do not spend another pass on threshold or local arithmetic tuning.

#### P9.1 API-free fallback and register-budget decision — in-pipeline DDA closed

The API-free layout pass succeeds structurally. OptiX preserves the nonlinear recovery core as a non-entry function of approximately `11,359` instructions instead of forcing it into every bounded entry. At the default register budget:

| entry | direct stack before / after | spills before / after | instructions before / after |
|---|---:|---:|---:|
| Mode-5 primary | `1,512 / 744 B` | `512 / 492 B` | `87,953 / 20,153` |
| Mode-5 closest/visibility | `2,152 / 1,192 B` | `452 / 388 B` | `104,644 / 25,546` |

The exact/oracle-adjudicated correctness result is unchanged and the selected regression suite again passes `29/29`. However, the repeated counter-free renderer timing remains negative:

| case | Mode-0 path p50 | Mode-5 path p50 | Mode-5 / Mode-0 | G-buffer ratio |
|---|---:|---:|---:|---:|
| curved-cobble | `19.442 ms` | `171.525 ms` | `8.8222x` | `1.0474x` |
| curved-rock | `17.713 ms` | `90.074 ms` | `5.0852x` | `1.0396x` |
| sphere-terrain | `3.408 ms` | `22.681 ms` | `6.6552x` | `12.8377x` |
| twisted-rock | `5.547 ms` | `4.447 ms` | **`0.8017x`** | `1.0708x` |

This table isolates the remaining cost. Curved primary rays mostly recover through nonlinear traversal, so their G-buffer stays near Mode 0; curved secondary rays execute bounded DDA and make the path pass `5–9x` slower. Sphere primary rays execute DDA and make the G-buffer `12.8x` slower. Twisted proxies are wider than the 64-cell eligibility cap and always use the API-free nonlinear core; their `19.8%` path win therefore belongs to fallback code layout, not to piecewise DDA or the first-order certificate.

One final compiler-only test raised the OptiX module register budget from the default to `255`, matching the standalone P8 kernel's `254` registers. It reduced Mode-5 primary spills to `40 B` and secondary spills to `32 B`, but lower occupancy slowed Mode 0 and did not repair the DDA gap:

| case | Mode-0 path p50 at 255 | Mode-5 path p50 at 255 | Mode-5 / Mode-0 |
|---|---:|---:|---:|
| curved-cobble | `26.089 ms` | `194.352 ms` | `7.4497x` |
| sphere-terrain | `3.695 ms` | `23.127 ms` | `6.2594x` |
| twisted-rock | `8.061 ms` | `6.361 ms` | `0.7890x` |

The build is restored to the default register budget (`NRTDSM_OPTIX_MAX_REGISTER_COUNT=0`). The conclusion is scoped but firm:

- P8 remains valid as an isolated same-ray CUDA upper-bound result.
- P9 shows that the present bounded certificate plus streaming DDA is not competitive inside this OptiX custom-intersection architecture, even after recovery separation and spill removal.
- The fallback-only win must not be attributed to the proposed DDA method.
- Do not claim renderer speedup over Mode 0/NRT-QT, TFDM, RMIP, or Ogaki et al.; TFDM/RMIP have not yet been implemented as publication baselines.
- Keep Mode 5 and its harnesses as reproducible experimental evidence, but stop threshold, segmentation, leaf-algebra, and register tuning on this in-pipeline route.

A future performance attempt would require a genuinely new architecture—most plausibly a separate CUDA ray-buffer/query stage or another scheduling design that gives the DDA kernel CUDA-like register and batching behavior. That is not a small continuation of P9 and must receive its own plan, correctness contract, and time budget before implementation. For the current Eurographics schedule, treat ray intersection as a conservative-query application/correctness demonstration unless another application supplies the main practical performance result.

### P10 — tube-width versus centerline-DDA bottleneck isolation

This is a user-requested diagnostic after P9, not a reopening of the publication-speed claim. Its purpose is to identify whether the integrated Mode-5 regression is dominated by conservative tube width or by the centerline DDA/hierarchy control path itself.

#### Question and controlled variants

Use the same frozen Mode-5 dispatch, certificate construction, one-midpoint split rule, start-LOD rule, hierarchy, exact nonlinear leaf test, OptiX entry layout, scenes, and timing protocol. Change only the radii passed from an accepted certificate into `traverseMode1CertifiedSegment`:

1. `M0 / NRT-QT`: audited nonlinear reference;
2. `M5 / certified tube`: current conservative method; and
3. `M5-R0 / zero-radius closed supercover`: force `radiusU = radiusV = 0` at the traversal call while retaining closed-cell ownership padding; and
4. `M5-C1 / strict centerline`: additionally remove the `+1` root-neighbor scan so each centerline anchor starts from one owner cell. The root-range bound retains its boundary guard, but it does not enumerate extra cells when the neighbor radius is zero.

`M5-R0` and `M5-C1` are intentionally non-conservative with respect to the rational shell ray. `M5-C1` also drops closed-boundary ownership relative to the chord itself. They can miss valid leaf cells and are therefore performance attribution experiments only: do not use their image equality, apparent correctness, or timing as method results.

#### Measurements

First run the existing counter-free `16`-warm-up / `30`-sample renderer timing on curved-cobble, curved-rock, sphere-terrain, and twisted-rock. Twisted-rock remains a fallback-only layout control and should show no material M5/M5-R0/M5-C1 difference. Report path and G-buffer p50/p95 ratios.

Then use a profiling-only build and one deterministic frame to report, by ray family:

- accepted segments and midpoint splits per invocation;
- DDA centerline anchors;
- admitted min/max hierarchy nodes;
- nonlinear leaf cells and the implied two nonlinear micro-triangle tests per leaf; and
- fallback/refusal counts.

Phase timers may be reported only as directional evidence because per-node device timestamps perturb this small irregular kernel. Counter-free wall time is authoritative.

#### Interpretation gates

- If `M5-R0` removes at least half of the excess time `M5 - M0` and materially reduces hierarchy/leaf work, tube expansion is the leading actionable cost. The next separately planned method experiment should compare the current componentwise box tube against a conservative directional/oriented tube and against shorter certified chords.
- If `M5-R0` removes less than one quarter of the excess time, the centerline DDA/control path or its OptiX scheduling is the leading cost. Do not invest in tighter tube mathematics for performance; investigate a leaner traversal or separate CUDA scheduling only under a fresh architecture plan.
- Between those thresholds, add one leaf-disabled or fixed-leaf-list microbenchmark before choosing an optimization. Do not infer the winner from instrumented phase time alone.

The default build must be restored to conservative radii after the experiment. Any code switch must be compile-time, visibly named as unsafe/diagnostic, default `OFF`, and incapable of changing Mode 0.

#### P10 measured result — tube cost is secondary; integrated DDA control dominates

Fresh counter-free builds used the same `16` warm-ups and `30` retained GPU samples for every variant. `M5-R0` retained the closed-supercover `+1` root-neighbor scan even at zero radius. After noticing that distinction, `M5-C1` removed that scan as a deliberately unsafe upper bound.

| case | conservative M5 / M0 | zero-radius supercover / M0 | strict centerline / M0 | M5 excess removed by strict centerline |
|---|---:|---:|---:|---:|
| curved-cobble | `8.8137x` | `6.0485x` | `5.6623x` | `40.45%` |
| curved-rock | `5.1263x` | `4.9005x` | `4.5447x` | `13.66%` |
| sphere-terrain | `6.6102x` | `6.4637x` | `6.1206x` | `10.03%` |
| twisted-rock fallback control | `0.8010x` | `0.8026x` | `0.8014x` | noise/layout only |

The strict-centerline path is not a valid intersection method, yet it remains `4.5–6.1x` slower than Mode 0 on every active case. The tube is therefore a measurable secondary cost—most visibly on cobble—but neither tube width nor closed-supercover bookkeeping explains the integrated regression.

The one-frame work profiles rule out segment proliferation and excessive nonlinear leaf solving:

- cobble closest uses one accepted segment on `14.82%` of candidate invocations and two on only `0.11%`; the other invocations follow the frozen fallback policy;
- sphere primary accepts one segment on `8.70%` of candidate invocations;
- tube to strict-centerline changes cobble-closest total hierarchy nodes/invocation from `10.96` to `9.92` and leaf cells from `0.69` to `0.42`;
- rock-closest changes only `11.75 -> 11.54` nodes and `0.58 -> 0.53` leaves; and
- sphere-primary changes only `5.11 -> 5.08` nodes and `0.30 -> 0.29` leaves.

These totals include NRT fallback work, but that is itself informative: a small DDA-eligible fraction creates a large wall-time regression even though total node and leaf counts remain comparable to Mode 0. Intrusive device-timestamp profiles consistently put about `90–96%` of measured Mode-5 invocation time in hierarchy/control and only about `0.4–4.5%` in nonlinear leaf work. Treat those percentages as directional, not authoritative timing, because the timestamps perturb an irregular OptiX program.

The largest isolated code region is therefore the integrated `traverseMode1CertifiedSegment` control path, not certificate segment count or the two nonlinear leaf solves. At each descended child it combines componentwise UV clipping, proxy triangle/square classification, min/max fetch, outward rounding, height-interval clipping, compact DFS state, and divergent accept/fallback behavior. P8 shows that the same conservative idea can be profitable in a homogeneous standalone CUDA replay; P9/P10 show that this work is not scheduled or laid out competitively inside the present mixed OptiX custom-intersection entry.

Allowed conclusions and follow-on:

- Do not claim that a thinner tube alone will recover renderer performance.
- Do not optimize segmentation or nonlinear leaf algebra next; both have been ruled out as leading costs on this corpus.
- Propagating proxy-cell inside/outside state or replacing four-child clipping with an ordered child DDA could reduce constants, but the strict impossible upper bound leaves too large a gap to justify expecting parity from such local changes.
- The only performance direction with positive evidence is a separately planned homogeneous CUDA ray-query stage or equivalent launch/program separation that preserves the P8 batching regime. That is a new architecture, not a P10 micro-optimization.

Authoritative counter-free reports are under `.tmp/mode1_certified/p10/timing-tube`, `timing-radius-zero`, and `timing-strict-centerline`. Work-count reports are under the matching `profile-*` directories. The diagnostic switches remain default `OFF`; the production build must use conservative radii.

Final restoration check: `NRTDSM_OUTPUT_TRAVERSAL_STATS=OFF`, `NRTDSM_CAPTURE_P8=OFF`, both P10 diagnostic switches `OFF`, and the default OptiX register budget `0`. The Release renderer rebuild succeeds and the expanded certificate/proxy/world-`t`/closed-DDA/P8/A2 regression selection passes `36/36` tests through `unittest`.
