---
title: Note — Final ray application decision after Mode 1 recovery
tags: [ray-tracing, displacement-mapping, certified-traversal, performance, decision, Eurographics]
status: final-negative-performance-decision
created: 2026-08-30
updated: 2026-08-30
---

# Final ray application decision after Mode 1 recovery

## Bottom line

The certified piecewise shell-ray construction is a real mathematical and correctness result, but the current ray application does **not** support a competitive-performance or state-of-the-art claim. Extensive implementation recovery reduced major avoidable overhead, yet final Mode 1 remains `5.76x–18.39x` slower than Mode 0 in path tracing on the two matched full-renderer cases. The independent same-surface A2 study is more favorable but still fails its predeclared held-out gate: certified DDA is excellent for front rays and slower for ordinary, oblique, grazing, moderate-shell, and stress-shell cohorts.

Do not claim faster tracing than Ogaki/nonlinear quadtree, TFDM, RMIP, PDM, dense triangles, or hardware displacement micromaps. TFDM and RMIP have not received a normalized paper timing because the internal baseline gate that authorized those ports failed.

## What the recovery work improved

Relative to the first counter-free P0 implementation, accepted optimizations reduce Mode 1 path median from `844.318 ms` to `101.241 ms` on twisted-rock (`-88.0%`) and from `199.082 ms` to `111.747 ms` on curved-cobble (`-43.9%`). G-buffer median falls from `21.673 ms` to `13.329 ms` (`-38.5%`) and from `30.858 ms` to `24.681 ms` (`-20.0%`).

The largest concrete improvements were:

1. certified proxy-triangle and world-ray-parameter clipping, which stops work outside the represented proxy and prunes segments behind an existing hit;
2. prepared certificate algebra, which reuses ray/proxy invariants;
3. a cold, non-inlined union-certificate safety boundary, which also prevents harmful compiler inlining/resource growth;
4. progressive rare-overflow resegmentation at `eta=2,4,8,16,32,64`, which removes catastrophic exhaustive-fallback work from the sampled scenes;
5. clamping coalesced root and neighbor enumeration to the proxy domain; and
6. the accepted start-LOD factor `2`, which reduces anchors/control work and improves final total-frame median by another `12.04%` on twisted-rock and `5.40%` on curved-cobble.

Factor `2` originally exposed equal-distance leaf scheduling differences. A deterministic lexicographic `(leaf y, leaf x, microtriangle)` ownership rule now makes factor `2` and factor `4` byte-identical in geometry and shading-normal AOVs on both scenes. Strictly farther hits cannot replace the closest hit.

Several plausible optimizations were measured and rejected: streamed certificate/traversal, capacity `16`, proxy-inside propagation, removing the cold compiler boundary, and leaf caching. Their resource or work reductions did not translate into sufficient wall-time gains.

## Final full-renderer comparison

RTX 5090, 1920x1080, identical executable/camera/map inputs, `eta=0.3`, 16 discarded warm-up frames, 30 measured frames, traversal counters disabled:

| case / component | Mode 0 p50 | final Mode 1 p50 | M1 / M0 | final M1 p95 | final M1 CV |
|---|---:|---:|---:|---:|---:|
| twisted-rock G-buffer | `1.435 ms` | `13.329 ms` | `9.29x` | `13.470 ms` | `0.82%` |
| twisted-rock path | `5.506 ms` | `101.241 ms` | `18.39x` | `133.378 ms` | `65.73%` |
| twisted-rock total | `7.216 ms` | `114.816 ms` | `15.91x` | `146.948 ms` | `58.91%` |
| curved-cobble G-buffer | `3.516 ms` | `24.681 ms` | `7.02x` | `25.573 ms` | `2.02%` |
| curved-cobble path | `19.395 ms` | `111.747 ms` | `5.76x` | `112.771 ms` | `0.72%` |
| curved-cobble total | `23.173 ms` | `136.589 ms` | `5.89x` | `138.138 ms` | `0.74%` |

The twisted CV is not random measurement noise. Samples `0` and `16` repeatedly enter a Mode-1-only slow state even though their recorded segments, DDA steps, hierarchy nodes, leaves, retries, coalescing, and fallback counts match ordinary samples. Mode 0 is not slow at those positions. This unresolved latency tail must be disclosed.

This full-renderer table is a practical implementation comparison, not the clean same-surface causal ablation. The authoritative same-surface architecture comparison is A2 below.

## Same-surface isolated comparison with NRT-QT

The packed A2 GPU experiment gives both methods identical certified inputs and exact represented leaves. It passes checksums and root/owner correctness over the frozen corpus.

| held-out cohort | CERT-DDA-MM / NRT-QT | interpretation |
|---|---:|---|
| front-hit | `0.383x` | certified DDA is `2.61x` faster |
| oblique-hit | `1.279x` | certified DDA is `27.9%` slower |
| grazing-hit | `1.624x` | certified DDA is `62.4%` slower |
| near-miss | `1.049x` | certified DDA is `4.9%` slower |
| held-out ordinary | `1.261x` | fails the ordinary-ray gate |
| moderate shell | `1.457x` | fails the target-shell gate |
| stress shell | `1.272x` | slower |
| all recorded samples, non-decision-balanced | `0.798x` | favorable pooling is dominated by cohort balance and is not the decision result |

An ideal, zero-cost family dispatcher selects certified DDA only for front rays and reaches bootstrap median `0.9182x` NRT-QT, saving only `4.738 ns/ray`. Because this excludes feature evaluation, branching, queueing, divergence, and code-footprint cost, it fails the hybrid-rescue gate.

## Why leaf optimization and first-order clipping were stopped

After fallback recovery and factor-2 scheduling, the profiler records only `0.50–0.99` primary, `0.78–2.52` closest, and `0.91–2.93` visibility leaf admissions per candidate invocation. Diagnostic accepted intervals assign only `0.1–0.4%` to exact leaf work, versus roughly `19.5–30.4%` certificate/segmentation and `68.7–80.1%` DDA/hierarchy control. Optimizing barycentric reconstruction or fusing the two leaf triangles cannot close the measured multi-fold gap.

First-order clipping has an even stronger no-go result. On the coherent practical corpus:

| representation | node ratio to scalar min/max | leaf ratio | decision |
|---|---:|---:|---|
| current hybrid first order | `0.9906x` | `0.9622x` | negative break-even |
| optimized single-plane hybrid | `0.9841x` | `0.9611x` | negative break-even |
| idealized zero-cost support hull | `0.9818x` | `0.9548x` | negative break-even |

Even the unattainable support hull removes only `1.82%` of coherent nodes and `4.52%` of leaves. A real `float4 (h0,gu,gv,r)` mip would double the scalar-node payload and add tube-projection arithmetic, so a GPU port is not justified on this topology. The first-order formulation remains a valid conservative derivation and negative ablation, not a speed contribution.

## Correctness status

- The selected certificate, proxy-domain, world-`t`, segmentation, DDA, and first-order-oracle regression set passes `44/44` tests.
- The final timing build has `NRTDSM_OUTPUT_TRAVERSAL_STATS=OFF`.
- The accepted start-LOD policy preserves byte-identical geometry and normal AOVs against the factor-4 scheduling reference on twisted-rock and curved-cobble.
- No sampled task-capacity, segment-capacity, coalescer, root-mask, or exhaustive fallback occurs in the final key-scene profiles.
- The repaired curved-cobble gallery remains the visual seam check; performance conclusions do not rely on the beauty images.

## Baseline status and claim boundary

| method | present evidence | allowed conclusion |
|---|---|---|
| `NRT-QT` / Mode 0 | audited same-surface A2 timing plus matched full-renderer timing | current candidate does not beat it broadly |
| TFDM `TwoTriangle` / `Bilinear` | code builds and GPU initialization probes pass; no normalized timing/quality sweep | no speed comparison claim |
| RMIP / PDM / dense triangles / DMM | not integrated under a common quality and timing harness | no comparison claim |

The predeclared architecture plan intentionally blocked TFDM and second-tier baseline ports after A2 failed. Implementing them now would quantify how much the candidate loses by, but cannot rescue the paper's primary performance hypothesis.

## Eurographics contribution decision

As currently framed, the ray project is **not enough for an EG full paper whose central claim is faster displacement ray tracing**. What survives is:

1. a certified rational shell-ray chord/tube construction;
2. closed tube-supercover traversal with explicit boundary ownership and conservative fallback;
3. a rigorous exact-root/owner validation chain; and
4. an unusually complete characterization of where piecewise DDA and first-order slabs help or fail.

Those are technically meaningful, but the positive runtime advantage is limited to front-facing isolated rays, while ordinary and full-renderer results lose. A paper would need either a new architectural idea that changes this cost structure—not another local optimization—or a different central contribution where certification/correctness itself is sufficient and clearly novel relative to prior work. The current evidence should be used to make that scope decision, not softened into a universal-speed narrative.

## Authoritative artifacts

- [[Plan — Mode 1 performance recovery]]
- [[Report — A2 same-surface GPU comparison]]
- [[Plan — P-D2 residual clipping ablation]]
- [[Plan — First-order ray representation oracle]]
- `experiments/ray_architecture_a2/ray-a2-4-timing-d9125676ceb0`
- `experiments/ray_architecture_r0/ray-r0-hybrid-oracle-3dd5b9a1a015`
- `.tmp/mode1_certified/p2_segmentation/*_mode0_final_protocol.json`
- `.tmp/mode1_certified/p2_segmentation/*_e03_lod2_tie_timing.json`
