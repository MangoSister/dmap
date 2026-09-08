---
title: Report — A2 same-surface GPU comparison
tags: [results, ray-tracing, CUDA, DDA, nonlinear-ray, Eurographics]
status: frozen-negative-mixed-result
created: 2026-08-25
updated: 2026-08-25
---

# Report — A2 same-surface GPU comparison

## Bottom line

The corrected triangle-proxy method is mathematically complete on the current 864-ray packed corpus, but it is not uniformly faster than the same-surface nonlinear quadtree. The honest contribution boundary is regime-dependent acceleration, not “faster than Ogaki everywhere.”

The streaming certified hierarchy is `0.798x` `NRT-QT` on the pooled corpus and `0.383x` on held-out front-facing rays. It is `1.261x` on held-out ordinary rays and `1.624x` on grazing rays. The predeclared A2 performance gate therefore fails.

## What is now established

1. The dominant-axis ray-annihilator coefficients, certified rational chord tube, closed tube supercover, scalar min/max descent, exact cubic leaf roots, packed world refinement, ownership grouping, and closest-hit selection agree with an independent packed-input oracle.
2. Incremental DDA emits exactly the same certified tube cells as exhaustive scan on the frozen corpus.
3. The hierarchy reduces the 8,192 possible microtriangles per ray to p99 26 exact tests, with p99 40 min/max nodes and no fallback.
4. A streaming implementation removes the correctness scaffold's large arrays, reducing reported local memory from 10,144 to 880 bytes per thread.
5. The remaining performance boundary is geometric: front rays benefit strongly, while grazing and several low-work rays favor repeated nonlinear bounds.

## What is not established

- superiority over upstream TFDM, RMIP, PDM, explicit triangles, or hardware DMM;
- a full-renderer or OptiX custom-primitive speedup;
- a multi-segment performance benefit, because every current A2 ray accepts one segment; or
- a confirmatory hybrid-dispatch result, because the current decision split has already been observed.

## Draft-safe wording

> We introduce a certified shell-ray linearization and closed tube-supercover hierarchy for triangle-proxy displacement. On a same-surface packed-input GPU study, the method exactly reproduces a dense represented-surface oracle and substantially reduces nonlinear hierarchy work. Its runtime benefit is regime dependent: it accelerates front-facing rays but does not outperform nonlinear quadtree traversal for grazing and several low-work cohorts. These results motivate a conservative hybrid dispatcher evaluated on a separately frozen confirmation corpus.

Do not yet write “faster than TFDM,” “faster than nonlinear ray tracing,” “state of the art,” or a universal performance claim.

## Authoritative artifacts

- correctness candidate: `experiments/ray_architecture_a2/ray-a2-5-hyb-stream-e8b2283abbb2`;
- correctness baseline: `experiments/ray_architecture_a2/ray-a2-3-nrt-qt-ef2448723ee6`;
- frozen timing: `experiments/ray_architecture_a2/ray-a2-4-timing-d9125676ceb0`;
- raw timing samples: one `*.raw.json` per cohort inside the timing artifact; and
- complete numbers: `summary.json` and `result.json` in the timing artifact.

Related: [[Plan — A2 isolated CERT-DDA-MM implementation]] · [[Plan — Certified ray architecture comparison]] · [[Guide — Eurographics paper draft]]

## Post-A2 hybrid upper bound — 2026-08-25

The planned R0 upper-bound audit also fails. Even a zero-cost perfect family dispatcher reaches only bootstrap median `0.9182x` `NRT-QT`, saves `4.738 ns/ray`, and selects certified DDA only for front rays. Because this excludes every real dispatch and scheduling cost, no hybrid kernel or new confirmation corpus is justified. See [[Plan — R0 hybrid ray rescue oracle]].
