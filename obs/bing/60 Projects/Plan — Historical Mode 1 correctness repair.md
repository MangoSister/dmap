---
title: Plan — Historical Mode 1 correctness repair
status: catastrophic-regression-fixed-residual-certification-open
date: 2026-08-25
---

# Plan — Historical Mode 1 correctness repair

## 1. Goal and scope

Repair the historical NRTDSM `IntersectionMode_LinearRaySegments` path used by the old Mode 1 renderings, and audit its hierarchical DDA independently of Mode 0. The immediate regression is the large horizontal/vertical cross on `twisted_quad`; the broader goal is to ensure that traversal never silently omits a represented leaf.

This work does **not** rename the standalone A2 method as Mode 1. Historical Mode 1 represents fixed-diagonal, piecewise-planar displaced texel microtriangles and uses the shell-space curve/chords only to select candidate cells. That surface contract remains fixed during this repair.

## 2. Frozen reference cases

The first exact reproduction is:

- proxy: procedural `twisted_quad`;
- map: `data/disp_rock.png`, scale `0.2`;
- camera: `(0, 1.2, 1.0)`, pitch `50`, yaw `180`;
- outputs: geometry AOV plus beauty image for dense hardware-triangle oracle, Mode 0, and Mode 1;
- baseline folder: `.tmp/mode1_correctness/before/`.

The present Release executable reproduces the cross in Mode 1. We will retain `quad`, `curved_surface`, and `sphere` with `test_disp.png` as regression cases, then add adversarial synthetic rays for exact grid boundaries, grid corners, horizontal/vertical segments, reverse traversal, near-zero UV motion, long diagonals, texture edges, and segment endpoints.

## 3. Correctness contract

For each ray/proxy invocation:

1. The prism interval must contain every intersection with the represented displaced microtriangles.
2. Event partitioning and segment generation must cover the complete supported ray interval. A denominator event, numerical refusal, or capacity limit must trigger an explicit conservative fallback, never a partial result.
3. Candidate traversal must visit every leaf cell possibly intersected by the true shell-space curve. A centerline-only DDA is insufficient for a bowed curve; either a certified tube supercover or an equivalently conservative fallback is required.
4. Min/max rejection must use a conservative height interval over the same admitted curve interval. The current heuristic `hBow` is not a proof.
5. The DDA must use a closed-cell/supercover convention: exact edge and corner contacts retain every touching owner. Tied X/Y crossings advance both axes without a numerical jump that can skip ownership.
6. Stack, work-queue, segment, and step capacities must be checked. Overflow/refusal must execute a whole-invocation fallback and be counted.
7. Leaf tests must be restricted to the admitted ray interval, test both fixed-diagonal triangles, and retain the closest valid hit globally. Traversal order alone is not a proof that the first visited leaf hit is closest.
8. Reported base coordinates and shading attributes must be reconstructed from the actual winning leaf intersection, not from an approximate chord coordinate.

## 4. Ordered implementation stages

### R1 — Diagnose the catastrophic cross

Add temporary counters for:

- segment-array saturation;
- skipped singular intervals;
- DDA step-limit termination;
- DDA stack overflow;
- exact X/Y boundary ties;
- first-leaf early exits;
- segment and DDA high-water marks.

Run the frozen twisted case and localize the cross pixels by geometry AOV. Test one suspected mechanism at a time. Do not keep a larger fixed array or disabled guard as a final correctness fix unless the measured maximum is also bounded analytically.

### R2 — Independent DDA differential test

Extract the incremental centerline DDA transition logic into a host/device-testable primitive. Compare its closed cell set with an exhaustive segment-versus-closed-cell oracle for randomized and adversarial segments at every hierarchy level. Validate forward/reverse symmetry and non-power-of-two map policy.

Then compare curve candidate coverage against the existing A1/A2 certified closed tube-supercover reference. This separates “the DDA walks its chord correctly” from “the chord safely covers the curve.”

### R3 — Conservative repair

Port the smallest proven pieces required by the failures:

- deterministic closed-supercover tie handling;
- certified componentwise chord tube radii, or explicit fallback when certification refuses;
- hierarchical cell/tube overlap and conservative min/max height clipping;
- checked work capacity with whole-invocation fallback;
- global closest-hit retention and actual leaf-hit attributes.

Reuse the A1/A2 mathematics and differential vectors. Do not preserve the historical midpoint estimate or heuristic `hBow` as a correctness predicate.

### R4 — Renderer integration and evidence

Build Release and render identical camera rays through:

1. dense hardware-triangle oracle;
2. repaired Mode 1;
3. Mode 0 as a diagnostic comparator, not truth.

Save beauty, position, normal, mask-XOR, and depth-error images. Report mask disagreement, common-hit position percentiles, closest-depth failures, fallback rate, segments/ray, hierarchy nodes, DDA cells, exact leaves, and capacity high-water marks.

## 5. Gates

The repair is accepted only if:

- the twisted cross is absent in beauty, hit mask, and depth;
- the extracted DDA has exact closed-cell-set equality on all deterministic differential tests;
- no capacity or numerical path returns a partial traversal;
- no catastrophic wrong-depth hit remains in the frozen regression set;
- disagreements with the dense oracle are classified as oracle discretization, represented-surface difference, or a bounded chord approximation—never unexplained traversal omission;
- all changes build in Release and leave the standalone certified A2 tests passing.

Performance is measured only after these gates. A correctness fallback may be slow initially.

## 6. Initial audit findings

The current kernel is not yet conservative:

- segment construction stops silently at 32 entries;
- denominator-endpoint evaluation can skip an interval;
- a zero-width chord is traversed although the true UV ray can bow into adjacent cells;
- `hBow` is a heuristic derived from a midpoint UV threshold;
- exact grid-corner ties advance only one axis and force a later `1e-5` jump;
- the 5,000-step guard and 16-entry stack can terminate without fallback;
- tiled UVs are clamped to one image domain even though texture sampling repeats;
- the leaf test uses the full prism/known-hit interval rather than the current segment interval;
- the first visited leaf hit terminates a segment even though candidate ordering is approximate;
- reported barycentrics are derived from the chord rather than the winning microtriangle.

These are hypotheses to isolate and repair in order, not claims that every item causes the visible cross.

## 7. 2026-08-25 repair result

### 7.1 Root cause of the cross

The frozen Release reproduction showed that no cross pixel used the 32-segment capacity, skipped a singular interval, reached the 5,000-step limit, or overflowed the 16-entry stack. Advancing both axes at an X/Y tie and removing the `1e-5` parameter jump also left the cross unchanged.

Replacing the per-cell linear-height window plus heuristic `hBow` with the complete endpoint height range of the current h-monotone segment removed the cross. This isolates the catastrophic failure to non-conservative height rejection. On the twisted-rock frame:

- before: `14.13` hierarchy cells and `1.77` exact leaves per active pixel;
- conservative h range: `34.88` cells and `14.01` leaves before removing unsafe leaf early-exit;
- final correctness-first path: `60.40` cells and `27.18` leaves after global closest-hit retention;
- Mode-1 versus Mode-0 mask disagreement fell from `2.3155%` to `1.2694%`;
- the large horizontal/vertical cross disappeared.

The cost increase is expected. No historical performance number remains valid after this repair.

### 7.2 Additional verified repairs

- Exact grid ties advance both axes; no DDA transition advances the segment parameter by a fixed epsilon.
- Positive hierarchy intervals shorter than `1e-6` are no longer discarded.
- A first visited leaf hit no longer ends the segment; all admitted leaves can update the global closest hit.
- Base barycentrics are reconstructed from the actual winning world microtriangle barycentrics and its UV corners, not by projecting hit t onto the approximate chord.
- Per-pixel diagnostic flags expose segment capacity, singular skips, step-limit termination, and stack overflow while preserving the 16-byte statistics ABI.

### 7.3 DDA audit conclusion

`scripts/mode1_dda_audit.py` compares both a flat centerline walk and a host emulation of the actual multilevel push/descend/resume machine against exhaustive closed-cell clipping.

- 2,000 generic straight segments: zero flat-DDA mismatches and zero hierarchy-versus-flat mismatches.
- Exact grid-edge/corner cases: the half-open walk intentionally omits point-only/opposite boundary owners, so it is not a closed supercover.
- Bowed-curve construction: 10 cells reached by the curve are absent from its zero-width chord candidate set.

Therefore the repaired hierarchy correctly walks generic straight chords under its half-open convention, but historical Mode 1 is **not yet a conservative curved-ray traversal**. The remaining ray-side repair is to port the certified componentwise tube plus closed supercover already validated in A1/A2, or to use an explicit whole-invocation conservative fallback. Tightening the midpoint threshold is not a proof.

### 7.4 Renderer regression

The current Release build passes the existing dense-oracle suite at one primary sample:

| case | method | mask disagreement | relative mean position | median normal error |
|---|---:|---:|---:|---:|
| quad | Mode 1 | `0.00000` | `0.00000` | `0.000 deg` |
| curved surface | Mode 1 | `0.00180` | `0.00007` | `0.827 deg` |
| sphere | Mode 1 | `0.00003` | `0.00028` | `0.502 deg` |

Artifacts are under `.tmp/mode1_correctness/`; the standard regression output is under `.tmp/mode1_correctness/regression/`. Nine targeted DDA/A1/A2 tests pass.

This closes the catastrophic-rendering repair, not the certified-tube integration stage.
