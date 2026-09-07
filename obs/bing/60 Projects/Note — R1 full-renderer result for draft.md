---
title: Note — R1 full-renderer result for draft
tags: [draft, ray-tracing, R1, negative-result]
status: evidence-ready
updated: 2026-08-30
---

# R1 full-renderer result for draft

## Safe statements now

- We implemented O2 as a separate tessellation-free renderer path (Mode 6) without replacing the historical certified tube-supercover path (Mode 1) or the nonlinear min/max reference (Mode 0).
- Mode 6 constructs topology-preserving piecewise shell-ray segments, emits a closed ordered leaf-cell stream, applies padded level-zero min/max rejection, and performs the same exact nonlinear microtriangle tests as Mode 0.
- The compact renderer schedule is byte-identical to the earlier materialized diagnostic schedule on the four frozen geometry and shading-normal AOV cases and reduces Mode-6 closest/visibility direct stack from 7,904 B to 5,184 B.
- Two conservative closest-hit termination designs were tested: a robust per-cell suffix rescan and a monotonic-frontier bound. Both preserved output but increased GPU state/cost and were rejected. The timed production Mode 6 is ordered but no-stop.
- In the matched 1920x1080, 20-warmup/50-sample full renderer experiment, active Mode 6 is 25.55x Mode 0 total on curved/cobble and 31.57x on sphere/terrain. It is also 4.46x and 2.80x Mode 1, respectively.
- Flat/brick and twisted/rock are whole-candidate Mode-0 fallback controls under the current 64-cell canonical-window restriction; their near-Mode-0 timings are not O2 results.
- The negative performance result is stable on active cases: total-frame CV is 0.66% for curved/cobble and 2.46% for sphere/terrain.
- The final Mode-1 one-spp curved/cobble geometry and shading-normal hashes exactly match the pre-R1 artifacts, demonstrating that Mode 1 was preserved.

## Do not write

- Do not claim that the current ordered shell DDA is faster than Mode 0, nonlinear ray tracing, TFDM, or RMIP.
- Do not call the isolated O2.4d candidate-query timings end-to-end rendering results.
- Do not call the fallback-only flat/twisted ratios speedups.
- Do not claim that production Mode 6 has early termination.
- Do not imply that TFDM or RMIP has been reproduced in this renderer.

## Interpretation

The ray application currently contributes a validated formulation and a useful architecture diagnosis, not a competitive performance result. The dominant problem is no longer tube width: it is topology-event/schedule construction plus large GPU local state. A credible renewed performance attempt requires a streaming generator that interleaves event generation, DDA ownership, and leaf tests without materializing the general work/group/cell arrays. That is a new hypothesis and should be planned separately rather than described as a small optimization.

Primary evidence: [[Plan — R1 full-renderer validation of ordered shell DDA]], Section 13. Machine-readable results: `.tmp/r1_full_benchmark/summary.json`.
