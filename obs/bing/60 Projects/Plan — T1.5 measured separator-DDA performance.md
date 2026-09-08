---
title: Plan — T1.5 measured separator-DDA performance
tags: [plan, ray-tracing, shell-space, DDA, CUDA, performance]
status: superseded-for-closest-hit
created: 2026-08-30
updated: 2026-08-30
parent: "[[Plan — T1 separator-sign shell-ray certificate]]"
---

# Plan — T1.5 measured separator-DDA performance

> [!abstract] Purpose
> T1 failed its conservative segment-count and predicate-count continuation gates, but it also reduced direct structural min/max fetches to `0.177×` NRT-QT. Raw counts do not determine GPU time. T1.5 therefore tests a new, explicitly measured hypothesis: **does the current zero-radius, topology-certified leaf DDA have enough device-time advantage to pay for separator-driven segmentation?** This does not reinterpret T1 as a pass.

## 1. Frozen comparator and corpus

- Baseline: the existing same-surface `NRT-QT` CUDA kernel, which performs outward rational Bernstein range evaluation at min/max-quadtree nodes and exact cubic testing at admitted leaves.
- Candidate: the T1 separator-sign segmentation, direct 64×64 leaf DDA, conservative event-height/min/max rejection, and the same exact cubic leaf test.
- Data: the frozen 864 practical A2 rays, 48 displacement windows, three shell families, and the existing development/decision split grouped by `(window_id, shell_id)`.
- Hardware protocol: same packed windows/replays, 32,768 repeated records per cohort, 20 warm-ups, 50 randomized paired trials, and at least 10 ms calibrated batches.
- The analytic T0/T1 cases remain correctness tests; performance claims use practical rays only.

All outputs, configuration hashes, trial samples, device metadata, checksums, registers, local memory, and active blocks per SM are persisted.

## 2. Stage T1.5-A — optimistic traversal envelope

The CPU T1 oracle emits, for every ray, its merged certified leaf-cell/height schedule. A device kernel consumes that schedule, performs leaf min/max rejection, and runs the same exact cubic leaf tests as NRT-QT.

This stage intentionally does **not** charge construction of the schedule. It is an optimistic lower bound on candidate time and answers one useful stop question:

> If segmentation and certification were free, would direct leaf traversal be fast enough to matter?

The result must be described as a performance envelope, never as current end-to-end method performance.

Correctness gates:

- every schedule is regenerated from the frozen T1 implementation;
- zero T1 required-survivor omission before packing;
- candidate and NRT-QT status/hit checksums match in every timed cohort;
- zero schedule-capacity overflow or silent truncation; and
- reverse/analytic correctness remains covered by the existing T0/T1 tests.

Continuation gates for implementing the full device segmenter:

- timing coefficient of variation below `5%` for both methods in every cohort;
- median candidate/NRT time ratio at most `0.80` on held-out `decision-ordinary`;
- median ratio at most `0.90` for held-out `S1-moderate` and `S2-stress`; and
- no held-out ordinary ray family above `1.00`.

Failure stops T1.5: adding segmentation cannot make an already slower optimistic kernel faster. Passing only authorizes T1.5-B.

## 3. Stage T1.5-B — full current method

Implement the current T1 construction in the measured CUDA kernel:

1. derive the rational shell coefficients already shared with NRT-QT;
2. conservatively certify fixed denominator and derivative signs, otherwise use explicit NRT-QT fallback;
3. maintain a bounded interval stack;
4. form chord grid events lazily and evaluate filtered separator signs;
5. split at a failed separator or unproved chord corner;
6. traverse accepted segments in increasing shell-height order with zero-radius DDA;
7. conservatively bracket event heights, perform leaf min/max rejection, and execute the common exact leaf test; and
8. preserve closed edge/corner ownership and deduplicate leaf/hit work across segment boundaries.

The binary32/64 device filter must be validated against the CPU Decimal-audited T1 decisions. An uncertain device sign is never guessed; it splits or falls back. The current practical corpus has no interior turn, but the device derivative-sign certificate and explicit fallback are still mandatory.

Final performance gates:

- all correctness and checksum gates pass;
- practical fallback fraction at most `5%`;
- timing coefficient of variation below `5%`;
- median full-candidate/NRT ratio at most `0.80` on `decision-ordinary`;
- median ratio at most `0.90` on both held-out `S1-moderate` and `S2-stress`;
- no held-out ordinary family above `1.10`; and
- no correctness dependence on CPU-precomputed segment or cell schedules.

These are predictive same-surface kernel gates, not a renderer or external TFDM/RMIP comparison. A pass authorizes integration into a renderer mode; a failure records the bottleneck by segment setup, sign predicates, DDA steps, min/max survivors, exact leaves, and device resource use.

## 4. Interpretation boundary

- T1.5-A pass: direct traversal has enough optimistic headroom to measure the full segmenter.
- T1.5-A failure: the current direct-leaf formulation cannot beat NRT-QT even with free segmentation.
- T1.5-B pass: the current method has a measured same-surface GPU traversal advantage and merits renderer integration.
- T1.5-B failure: the topology result remains valid, but the current realization has no supported performance contribution.

No stage supports a claim of superiority to TFDM, RMIP, or published nonlinear-ray-tracing implementations until equivalent external baselines and full-renderer measurements exist.

## 5. Decision log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-30 | Reopen device measurement as a new T1.5 hypothesis. | T1's `0.177×` leaf-fetch ratio may outweigh its p95 seven-segment tail; only measured device time can decide. |
| 2026-08-30 | Measure an optimistic traversal envelope before porting the full segmenter. | It is a cheap necessary-condition test and prevents spending time on a full CUDA port when free segmentation would still lose. |
| 2026-08-30 | Keep schedule construction outside T1.5-A claims. | Precomputed schedules hide the main proposed runtime work and therefore cannot establish end-to-end performance. |
| 2026-08-30 | Stop before T1.5-B. | The correctness-passing optimistic envelope misses the held-out ordinary, S1, and per-family continuation gates, so the current full separator port lacks the required headroom. |

## 6. T1.5-A result

Authoritative run: `ray-t15-envelope-f619b4ad5265` on an NVIDIA GeForce RTX 5090 (compute capability 12.0). All hit/status checksums match NRT-QT, all outputs are regular, the regenerated schedules have zero required-survivor omission and zero fallback, and every timing CV is below `0.66%`.

The packed schedules reproduce T1's practical totals: `6025` merged leaf cells, maximum `39` cells per ray, `1632` accepted segments, `768` splits, and `11561` sign predicates. Schedule construction and those predicates/splits are excluded from the measured candidate kernel.

| held-out cohort | candidate ns/ray | NRT-QT ns/ray | candidate / NRT |
|---|---:|---:|---:|
| decision ordinary | `74.75` | `78.22` | `0.9557×` |
| S0 affine | `62.55` | `46.29` | `1.3512×` |
| S1 moderate | `64.31` | `59.75` | `1.0762×` |
| S2 stress | `81.07` | `103.34` | `0.7845×` |
| front hit | `9.38` | `30.07` | `0.3118×` |
| grazing hit | `104.37` | `109.01` | `0.9575×` |
| near miss | `52.51` | `58.17` | `0.9027×` |
| oblique hit | `48.39` | `27.20` | `1.7789×` |

The candidate and NRT-QT kernels both use 254 registers and four active 64-thread blocks per SM; the envelope candidate reports 208 local bytes versus NRT-QT's 848. Therefore occupancy does not explain the held-out miss. The direct schedule is attractive in stress and front-facing regimes, but exact-leaf/survivor work and fixed per-ray setup erase the structural-fetch advantage in moderate and oblique regimes.

Three frozen continuation checks fail:

- held-out ordinary is `0.9557×`, not `≤0.80×`;
- S1 is `1.0762×`, not `≤0.90×`; and
- oblique is `1.7789×`, violating the `≤1.00×` per-family ceiling.

T1.5-B is therefore not implemented. This envelope is intentionally favorable because it omits runtime segmentation and certification. It does load a packed schedule from device memory, so it is an engineering upper-bound experiment rather than a formal mathematical lower bound on every possible fused implementation. The missing `0.1557×` ordinary headroom and the large oblique loss are nevertheless too large to justify the full current port under the predeclared protocol.

> [!warning] Closest-hit correction
> The envelope also merged cells into a global map, sorted them lexicographically, enumerated all intersections, and never stopped at a certified closest hit. It therefore measures unordered candidate-set throughput, not the intended front-to-back DDA renderer query. The general closest-hit conclusion is reopened under [[Plan — T1.6 ordered closest-hit shell DDA]]. The numerical T1.5 result remains valid for its narrower all-candidate contract.

Safe conclusion:

> Unordered replay of the topology-certified leaf set can outperform nonlinear quadtree traversal for high-curvature/stress shells and front-facing rays, but it does not provide a broad same-surface GPU advantage. This result does not measure front-to-back closest-hit termination; ordered DDA remains a separate, gated hypothesis.
