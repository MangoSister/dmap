---
title: Result — Phase 1 Taylor pyramid
tags: [result, phase1, pyramid, bounds, tightness]
created: 2026-08-26
---

# Result — Phase 1 Taylor pyramid

Phase 1 of [[Project — Conservative metric queries without tessellation]]: the Taylor-model bound pyramid ([[Taylor-model bound pyramid]]), its propagation to certified metric bounds, and the tightness kill-test with ablations. Code in `C:\dmap\code` (`dmapref/affine.py`, `pyramid.py`, `node_bounds.py`, `dense_reference.py`; experiments 5–7; 24 unit tests pass). Full experiment logs and figures in `code/experiments/out/`.

## Verdict up front

**The kill criterion is met**, after being revised the same day (log, 2026-08-26) from "within 3× at the levels applications traverse" to "within 3× at cells up to 8 texels per side" — the cells whose bounds the applications actually read for answers, per the application-by-application review in the log. Every bound is conservative (0 violations in ~4 million cell checks). On 0.05×edge amplitude, the worst median certified-to-true width ratio over √det G, λmax, and the cone at criterion cells is 2.77; at coarser cells the ratios grow to 2.7–4.1 (√det G, 32–64-texel cells), which only affects efficiency (descent allocation, refinement depth, walk length, deep LoD drops). The node beats every ablation at every level, so none of the tested alternatives is tighter. The looseness at coarse cells is mostly inherent to the node itself: its six numbers bound h and the gradient by independent ranges, so they also cover value combinations that never occur together on the surface patch, and that gap grows with cell size.

## What was built

- `affine.py`: batched affine arithmetic over the five shared symbols (εu, εv, εh, εgu, εgv), with squares anchored at the midpoint of the quadratic term's range and a secant-linearized affine sqrt.
- `pyramid.py`: `TaylorPyramid` (exact bilinear leaf; conservative fold) and `MinMaxPyramid` (exact independent min-max channels for h, h_u, h_v — ablation storage and exact reference).
- `node_bounds.py`: conservative cell enclosures of the base forms (N, Nu, Nv from the affine interpolated normal; G0 exact per flat triangle), and propagation of a node to certified intervals on the entries of G, det G, √det G, the eigenvalues of G0⁻¹G, and a normal cone. Ablation variants share the propagation code.
- `dense_reference.py`: vectorized pointwise truth (pinned against `metric.py` by a unit test) and per-cell range reductions at every level.

## Implementation refinements found during the phase (now in the concept note)

1. **Fold through interval hulls.** The note's child-mean parent plane is valid but loose. The parent gradient interval is now the interval hull of the four child intervals — the smallest interval containing them, which is exactly the min-max range of the gradient — with the parent slope at its midpoint. The parent offset and remainder are the midpoint and half-width of the overall maximum and minimum of the child plane bounds, evaluated at the quadrant corners; this choice of offset minimizes the remainder for the chosen slope. This alone cut √det G widths by ~25–40% at mid levels.
2. **Intersect affine with interval propagation.** Both routes are valid enclosures of the same node; their strengths are complementary. The affine route keeps correlation across products (det G); exact interval squares are tighter once a deviation straddles zero (x² ≥ 0 is invisible to an affine form, and on rough content at coarse cells the gradient deviation always straddles zero). The intersection dominates both.
3. **Eigenvalue enclosures.** The naive interval discriminant is orders of magnitude too wide. Implemented: Weyl perturbation around the interval-centre matrix ∩ the all-affine trace/discriminant route with affine sqrt ∩ a refinement through λmin·λmax = det G/det G0.
4. **Normal cone.** Per-symbol vector norms for the deviation radius (the componentwise box forgets that one symbol moves all three components together), and the perpendicular-component sine bound intersected with the cosine bound. A guard matters for validity: once the deviation radius reaches the centre length the cone must open to π.

## Experiment 5 — tightness (the kill-test)

Setup: cc_torus and spot × disp_rock and disp_cobble × amplitudes {0.05, 0.2}×mean edge, 10 seeded triangles each; 256×256-cell tile (3×3 box downsample of the 1k textures); truth from 5×5 samples per leaf cell, reduced per cell per level. Ratio = certified width / true range; the kill criterion is evaluated on √det G, λmax, and the cone at cells up to 8 texels per side (revised criterion, log 2026-08-26). Full tables: `exp05_log.txt`; figures `exp05_tightness_amp0.05.png`, `amp0.2.png`.

Median ratios, 0.05×edge (typical), by cell side in texels:

| Quantity | 1 | 4 | 16 | 64 |
|---|---|---|---|---|
| √det G | 1.1–1.2 | 1.6–1.9 | 2.1–2.6 | 3.0–4.1 |
| λmax | 1.3–1.4 | 1.5–2.0 | 1.8–2.4 | 2.4–2.9 |
| cone | 1.2–1.4 | 2.2–2.5 | 2.3–3.5 | 2.2–3.6 |

- At the leaf the bound is nearly exact (√det G ratio 1.17, width 2.8% of the value on cc_torus + rock).
- Relative widths grow with cell size: at typical amplitude the certified √det G interval is 17–100% of the value at 4-texel cells and 1–12× the value at 32-texel cells, with the rock texture at the low end and cobble at the high end. At 0.2×edge on the oblique coarse mesh (spot + cobble) ratios reach 12 at 64-texel cells: obliquity plus large amplitude is the genuinely hard regime, consistent with the master plan's limitation on obliquity.
- **λmin is excluded from the ratio criterion, by structure, not by choice:** a slope-dominated metric is a rank-one-like update of G0, which leaves the small eigenvalue of G0⁻¹G pinned near 1; its true per-cell range is nearly zero, so any width divided by it is enormous (medians from tens to thousands). The meaningful number: the certified λmin width is 0.4–2.7× its value at 4-texel cells. This limits per-node anisotropy certificates (application C) and conditioning certificates (B1); to state in the paper.
- **The certified cone saturates at π from ~8–16-texel cells up** on textured content: the normal box's deviation reaches its centre length. True cones there are wide too (ratios stay 2–4), but coarse-level cone consumers (light-tree traversal) will see full cones on rough tiles.

## Experiment 6 — ablations

A0 = joint node, affine ∩ interval propagation. A1 = independent exact min-max channels for (h, h_u, h_v), interval propagation. A2 = the Taylor node collapsed to intervals, interval propagation. A3 = the min-max channels as centred affine forms with private symbols. Three combos (typical, smooth, oblique-stress). Median √det G width vs A0, and false-degeneracy alarms (certified det G lower bound ≤ 0 while the true det stays positive):

- **A1/A2 are 1.6–1.9× wider at 1-texel cells, 1.2–1.3× at 4-texel, converging to parity by ~32-texel.** The correlation advantage is real and concentrated at fine levels — where applications actually consume bounds — and its size is content-dependent, not the order of magnitude the concept note's §1 argument might suggest.
- **A3 ≡ A0 (within 1%) at every level.** Giving the same gradient two inconsistent worst-case values is a failure of *interval propagation*, not of channel storage: any centred form keeps one consistent value per channel, and sharing the plane between the h and ∇h models adds nothing measurable because the gradient remainder symbols carry no position dependence anyway. What the joint node still provides over 6 independent channels: the O(s²) slab (§5 of the note, the ray and proximity consumer), the exact leaf, and one storage layout for both; but for pure metric-bound tightness, exact channels plus centred propagation match it.
- **False alarms:** at 4-texel cells on typical content A0 alarms on 1.8% of cells vs 3.8% for A1; by 32-texel cells everyone alarms on ~75–100% of textured cells. Certifying det G > 0 over large rough cells is beyond any of these bounds; positivity certificates are a fine-level tool.
- **Min-max recovery cost:** the h interval recovered from the Taylor node is 1.06× the exact channel at 1-texel, 1.6–1.8× at 4-texel, 5–29× at the root. TFDM-style traversal on recovered min-max is priced accordingly; near the root the dedicated channel is much tighter.

Figure: `exp06_node_ablation.png`.

## Experiment 7 — cost and memory

| Tile | Taylor build | min-max build | Taylor MB | 2-channel MB |
|---|---|---|---|---|
| 128² | 3.1 ms | 1.6 ms | 1.00 | 0.33 |
| 256² | 11.3 ms | 5.4 ms | 4.00 | 1.33 |
| 512² | 49.5 ms | 23.7 ms | 16.00 | 5.33 |

Memory is exactly 3.0× the 2-channel min-max pyramid (six float64 channels, same topology, as designed). Build is one vectorized mipmap-style pass. Bound queries, batched per level: ~0.4 μs/node for the cell enclosure plus ~1.1 μs/node for propagation, in numpy; the C++ core (Phase 2) is the production answer.

## Caveats

- Identity chart per face, whole-tile-per-triangle; the true-range reference samples the full unit square, including the corner outside the triangle domain.
- Truth is 5×5 samples per leaf cell; true ranges are slightly underestimated (ratios slightly overestimated).
- The base-form enclosure contributes almost nothing to the widths (zeroing its radii changes √det G widths by <5%); the node remainders carry nearly all of it. Bounding the base forms more tightly is not where improvement lies. The residual gap to truth comes from the node itself: six independent ranges cover value combinations of h and the gradient that the two-parameter surface never produces, and the gradient remainder symbols carry no position dependence.

Related: [[Project — Conservative metric queries without tessellation]] · [[Log — Conservative metric queries]] · [[Taylor-model bound pyramid]] · [[Result — Phase 0 numpy reference]] · [[The induced metric of a displaced surface]]
