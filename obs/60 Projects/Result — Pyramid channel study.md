---
title: Result — Pyramid channel study
tags: [project, result, phase2, bounds, sampling]
created: 2026-08-30
---

# Result — Pyramid channel study

Two diagnostics on the Taylor pyramid's node, run to settle where its channels are and are not adequate: the height bound that ray traversal reads, and the area estimate that the descent sampler reads. Both were prompted by the ray-tracing question raised on 2026-08-30 (see [[Log — Conservative metric queries]]). Code: `code/python/poc/experiments/exp08_minmax_recovery.py` and `exp09_weight_estimator.py`. Node semantics and the fold: [[Taylor-model bound pyramid]] §2–§5.

They answer different questions and reach opposite conclusions, which is the point: the bound channels need a new one, the estimate channels need a different one, and neither fix helps the other consumer.

---

## Part 1 — Why min-max h recovered from the node is loose

Phase 1 measured that the recovered height interval is 1.06× the exact channel at texel cells and 5–29× at the root, and concluded that ray traversal should keep a dedicated height channel. This decomposes that number.

### Setup

`disp_rock` and `disp_cobble` box-filtered to a 65 × 65 node grid (64 leaf cells per side), unit scale. Reference: `MinMaxPyramid`, whose leaf is exact and whose min/max fold is exact at every level, so its interval is the true range.

The recovery is `h ∈ h0 ± (|gu|s + |gv|s + r)`. Two terms, split as:

- **ideal/T** — the tightest possible `r` for the slope the fold picked, against the true half-range `T`. Isolates the recovery formula.
- **fold cost** — what the fold's `r` adds on top of that tightest `r`. Isolates fold accumulation.

The tightest `r` is computed exactly, not sampled. `h` is bilinear per texel, and `h` minus a linear function is still bilinear per texel, so the residual's extremes over any union of texel cells sit at texel grid nodes. Enumerating the nodes of the closed cell is exact.

### Measured

Medians over cells. Cell size in texels per side.

| | | disp_rock | | | | disp_cobble | |
|---|---|---|---|---|---|---|---|
| cell | true half `T` | ideal/`T` | folded/`T` | fold cost | ideal/`T` | folded/`T` | fold cost |
| 1 | 0.032 / 0.070 | 1.10 | 1.10 | 1.00 | 1.07 | 1.07 | 1.00 |
| 2 | 0.061 / 0.137 | 1.32 | 1.37 | 1.10 | 1.40 | 1.46 | 1.10 |
| 4 | 0.111 / 0.212 | 1.45 | 1.68 | 1.31 | 1.95 | 2.19 | 1.29 |
| 8 | 0.186 / 0.273 | 1.75 | 2.11 | 1.41 | 2.32 | 3.38 | 1.67 |
| 16 | 0.262 / 0.344 | 2.29 | 2.90 | 1.51 | 2.95 | 4.85 | 2.06 |
| 32 | 0.358 / 0.431 | 3.37 | 4.71 | 1.73 | 4.18 | 6.83 | 2.43 |
| 64 (root) | 0.452 / 0.462 | 8.32 | 10.93 | 1.63 | 6.27 | 10.61 | 2.31 |

Is the stored slope a trend? `disp_rock`, medians:

| cell | stored slope `\|gu\|` | cell's mean gradient | hull half-width `ru` |
|---|---|---|---|
| 1 | 1.808 | 1.808 | 0.569 |
| 8 | 1.049 | 0.710 | 5.871 |
| 32 | 0.720 | 0.099 | 12.055 |
| 64 | 1.289 | 0.008 | 15.041 |

### Findings

1. **The representation is not lossy.** On an exact plane the recovery is 1.000000 at every level. The six numbers can encode a min-max node directly — slope 0, `r` = the true half-range — so all looseness comes from the fold's choices, not from what the node can hold.
2. **The dominant cause is the recovery formula with a nonzero slope**, worth 8.32× of the 10.93× at rock's root. The formula adds the plane's peak excursion and the residual's peak as if they coincided. When the plane is a real trend they effectively do; when it is not, the plane sweeps outside the data and `r` must be large enough to pull the surface back. At rock's root the tightest residual for that slope is already 4× the entire true range.
3. **The slope at coarse cells is not a trend.** It is the midpoint of the gradient interval hull, set by the two most extreme texels anywhere in the cell: 1.289 at the root against a true mean gradient of 0.008. The fold is right to store it that way — `ru`, `rv` must be the exact gradient range because the gradient enters `G` through `∇h∇hᵀ`, so the midpoint is the natural centre for that channel. Reading it as a height trend is what costs.
4. **Fold accumulation is the smaller half**, 1.00 at leaves rising to 1.73 (rock) and 2.43 (cobble). The fold builds each parent from the child *models*, so slack already in a child becomes part of the parent's `r` and can never be recovered. The min-max fold structurally cannot do this: the range of a union is the union of ranges, so it is exact at every level with no model to mis-fit.
5. **The gradient channels are already exact.** Direct enumeration reproduces the folded `gu, gv, ru, rv` to 3.6e-15 at every level. All of the measured loss is in the height channel. Equivalently, `(gu, ru)` is a lossless re-parameterization of `[hu_lo, hu_hi]`, and midpoint-radius is exactly the form centred affine propagation wants.

### Direct construction, priced

Building each level by enumeration instead of folding is exact, conservative by construction, and passes the checks: it reproduces the closed-form leaf to 1.8e-15, never exceeds the folded `r`, and shows 0 violations in ~90k dense samples. It removes the fold accumulation only, so the recovery improves from 10.93× to 8.32× (rock root) and 10.61× to 6.27× (cobble root). Build cost goes from `(4/3)N²` to about `N² log N`, with all levels independent and therefore parallel where the fold is sequential. An operation count put that at ~4.5× at `N = 64`; measured in C++ it is 1.6× (Part 3), because the count ignored the fold's twenty corner evaluations per parent.

Random sampling of `h` is not an alternative: a finite sample can miss the extremum, so min/max over samples is an estimate, not an enclosure. Padding with a Lipschitz term from the node's own gradient bound restores validity, but at texel spacing the padding is zero and the method degenerates to the exact enumeration above.

---

## Part 2 — The descent weight's midpoint model

S7 measured product descent conceding 1.3–3.1× to the product table at amplitude 0.2, and attributed it to the coarse-level midpoint `√det G` composition without isolating a mechanism. This does.

### Setup

One `cc_torus` base triangle, 64 leaf cells per side, three configurations. Truth per cell: the mean of `√det G` over its texels, the same per-texel quadrature the S7 references use. Three estimators, all taking base forms at the cell centre as `composedWeight` does:

1. **midpoint** — what the code does today: `√det G` from the node's `(h0, gu, gv)` at the cell centre.
2. **mean gradient** — the same form with `h` and `∇h` replaced by their cell means.
3. **moments** — `√det E[G]`. Every term of the metric formula is at most quadratic in `(h, ∇h)`, so `E[G]` is exact from `E[h]`, `E[h²]`, `E[∇h]`, `E[∇h∇hᵀ]`: seven base-independent numbers that fold by averaging.

### Measured

Median estimator / truth. "low" is the fraction of cells the midpoint model underestimates.

| cell | rock 0.05 | | rock 0.2 | | | cobble 0.2 | | |
|---|---|---|---|---|---|---|---|---|
| | midpoint | moments | midpoint | mean grad | moments | midpoint | mean grad | moments |
| 1 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| 4 | 0.984 | 1.000 | 0.863 | 0.871 | 1.039 | 0.719 | 0.591 | 1.315 |
| 8 | 0.977 | 1.001 | 0.803 | 0.772 | 1.063 | 0.500 | 0.429 | 1.416 |
| 16 | 0.975 | 1.001 | 0.782 | 0.730 | 1.079 | 0.452 | 0.408 | 1.427 |
| 32 | 0.968 (100% low) | 1.001 | 0.719 (100% low) | 0.721 | 1.073 | 0.526 (100% low) | 0.406 | 1.430 |

### Findings

1. **The midpoint model is systematically low, not noisy.** From 16-texel cells up, every cell underestimates, in all three configurations. The error reaches 2× on cobble at amplitude 0.2.
2. **The mechanism is the discarded gradient covariance.** The weight builds `∇h∇hᵀ` from one gradient; the cell's true mean metric uses `E[∇h∇hᵀ]`. The difference is the gradient covariance over the cell, which is positive semidefinite and is exactly the cell's roughness. Dropping it lowers `G` in the Loewner order, and `det` is monotone there. Rough subtrees are therefore under-weighted at every coarse level, and the descent compounds it.
3. **A better slope does not help.** Replacing the gradient-hull midpoint with the cell's true mean gradient changes nothing on rock and makes cobble worse. The problem is the nonlinearity, not the choice of slope. This is a negative result worth keeping: it rules out the cheapest candidate fix.
4. **The bias tracks the S7 variance gap** across all three configurations:

   | config | midpoint / truth at 32-texel cells | S7 product descent vs product table, far field |
   |---|---|---|
   | rock 0.05 | 0.968 | 1.04× (tie) |
   | rock 0.2 | 0.719 | 1.39× |
   | cobble 0.2 | 0.526 | 3.05× |

   Monotone across three points. Suggestive, not conclusive.
5. **Moments fix most of it, above 8-texel cells.** Error drops from 2× low to at most 1.43× high, and the residual overshoot is guaranteed in sign: `√det` is concave on the positive definite cone (Minkowski), so `√det E[G] ≥ E[√det G]`. Below 8-texel cells on cobble the moment estimator's overshoot exceeds the midpoint's undershoot, so the better estimator is level-dependent. On rock, moments win at every level.

### What this does and does not mean

Estimator error is not variance. This measures the weight, not the sampler. The chain is weight estimate → mass allocation → variance, and only the first link is measured here; whether moment weights close the S7 gap needs the S7 harness. Sampling correctness is unaffected either way: the sampler reads `h0, gu, gv` as an estimate, never reads `r`, and divides by `√det G` evaluated pointwise at the drawn sample, so weight quality steers variance only.

Moments are estimates, not bounds. They would sit alongside the bound channels, never replace them: certified-zero pruning and every B/C consumer need real enclosures.

---

## Part 3 — The same two questions in C++, at renderer resolution

Parts 1 and 2 ran on a 64-cell tile in the numpy reference. Both changes are now
implemented in the C++ core (`code/cpp/src/taylor_pyramid.{h,cpp}`, `PyramidBuild::Fold`
and `PyramidBuild::Direct`, eight channels), and this part re-measures the height bounds
at the resolution the renderer uses. Task: `test_pyramid_channels`, config
`code/data/configs/test_pyramid_channels.toml`. Correctness is a separate task,
`validate_pyramid`.

### Setup

One `cc_torus` base triangle, both textures box-filtered to a 257 × 257 node grid
(256 leaf cells per side), amplitudes 0.05 and 0.2 × mean edge. Lengths in mean-edge
units; every ratio below is amplitude-independent, since all eight channels are linear
in the scale.

`box` is the stored range `h_max − h_min`; `slab` is the Taylor thickness `2r`;
`rec/stored` is the recovered half-width `|gu|s + |gv|s + r` over the stored half-range,
for the folded node and for the directly built one.

### A. The two height bounds

| | | disp_rock | | | | disp_cobble | | |
|---|---|---|---|---|---|---|---|---|
| cell | box | slab | rec/stored | rec direct/stored | box | slab | rec/stored | rec direct/stored |
| 1 | 0.0037 | 0.0004 | 1.09 | 1.09 | 0.0054 | 0.0003 | 1.06 | 1.06 |
| 2 | 0.0070 | 0.0027 | 1.32 | 1.27 | 0.0113 | 0.0032 | 1.27 | 1.23 |
| 4 | 0.0132 | 0.0089 | 1.57 | 1.38 | 0.0250 | 0.0171 | 1.68 | 1.51 |
| 8 | 0.0238 | 0.0238 | 1.88 | 1.57 | 0.0544 | 0.0763 | 2.68 | 2.25 |
| 16 | 0.0413 | 0.0583 | 2.38 | 1.84 | 0.0865 | 0.2571 | 5.15 | 4.05 |
| 32 | 0.0671 | 0.1392 | 3.23 | 2.54 | 0.1110 | 0.7229 | 10.50 | 7.43 |
| 64 | 0.1042 | 0.3006 | 5.39 | 4.61 | 0.1457 | 1.5697 | 14.32 | 7.08 |
| 128 | 0.1774 | 0.8085 | 13.12 | 10.94 | 0.1796 | 2.8056 | 22.02 | 10.63 |
| 256 (root) | 0.1899 | 2.9879 | 28.83 | 26.20 | 0.1867 | 4.5370 | 29.50 | 10.91 |

### B. The two constructions

Median `r` in mean-edge units at amplitude 0.2, and the median absolute shift in `h0`
between the two constructions.

| | disp_rock | | | | disp_cobble | | |
|---|---|---|---|---|---|---|---|
| cell | `r` fold | `r` direct | fold/direct | \|h0 shift\| | `r` fold | `r` direct | fold/direct |
| 1 | 0.00020 | 0.00020 | 1.00 | 0.00000 | 0.00015 | 0.00015 | 1.00 |
| 8 | 0.01190 | 0.00819 | 1.42 | 0.00078 | 0.03817 | 0.02728 | 1.32 |
| 32 | 0.06961 | 0.04427 | 1.51 | 0.00364 | 0.36146 | 0.21316 | 1.69 |
| 128 | 0.40423 | 0.28645 | 1.87 | 0.02187 | 1.40279 | 0.43926 | 4.15 |
| 256 | 1.49397 | 1.24407 | 1.20 | 0.08575 | 2.26847 | 0.53259 | 4.26 |

### Build cost and memory

Best of ten builds; memory is the whole pyramid, all levels.

| tile | fold | direct | direct/fold | 8-channel | 6-channel | 2-channel |
|---|---|---|---|---|---|---|
| 64² | 0.09 ms | 0.15 ms | 1.6 | 341 KB | 255 KB | 85 KB |
| 128² | 0.40 ms | 0.75 ms | 1.9 | 1365 KB | 1023 KB | 341 KB |
| 256² | 1.37 ms | 2.81 ms | 2.1 | 5461 KB | 4095 KB | 1365 KB |

### Findings

1. **The crossover is at 8-texel cells, on both textures.** Below it the slab is the
   thinner bound, above it the box, and on rock the two are equal at 8 exactly. This is
   the sharpest form of the "different query shapes" claim: the level at which a
   traversal should switch which bound it reads is measurable and, on this content, the
   same for smooth and rough input.
2. **The recovery gap grows with tile size**, from 10.9× at the root of a 64-cell tile
   (Part 1) to 28.8× at the root of a 256-cell tile. It has to: the recovery adds the
   plane's excursion across the cell, which is linear in cell width, to a range that
   saturates. Larger tiles make the dedicated channel matter more, not less.
3. **Which of the two causes dominates is content-dependent.** On rock, direct
   construction barely moves the root (28.83 → 26.20): the fold is almost blameless and
   nearly all the gap is the slope. On cobble it removes most of it (29.50 → 10.91).
   Part 1's finding that fold accumulation is the smaller half survives as a
   multiplicative statement, but its share grows with both tile size and roughness — the
   ratio `r` fold/direct at the root goes from 2.31 at 64 cells to 4.26 at 256 cells on
   cobble. Neither cause can be ignored, and neither alone reaches parity: even at
   10.91× the recovered interval is not a competitive traversal bound.
4. **The `h0` shift between constructions is far below its bound.** Nesting forces
   `|Δh0| ≤ r_fold − r_direct`; at cobble's root that bound is 1.735 and the measured
   median shift is 0.171, a tenth of it. The fold's inflation is close to symmetric, so
   the plane it picks is nearly the plane direct enumeration picks. This is why the
   descent sampler, which reads `h0` and never reads `r`, is almost indifferent to the
   construction.
5. **Direct construction is cheaper than the operation count suggested**, 1.6–2.1× the
   fold rather than ~4.5×, because the fold's twenty corner evaluations per parent are a
   large constant the count ignored. At 2.8 ms for a 256² tile it is not a build-time
   concern.
6. **Memory is exactly 4× the two-channel baseline**, a third above the six-channel
   node, as designed. Re-measured through the S7 harness: 383 KB per footprint against
   the product table's 16 KB, so the reuse factor at which the hierarchy's memory case
   opens moves from ~19× to ~24× (~12× under float32).


---

## Conclusions

- **Ray traversal needs a dedicated min-max h channel, and nothing else gets it there.** Direct construction removes fold accumulation but leaves 6–11× at the root, because the slope is fixed by the gradient channel, and the gap it leaves grows with tile size. Two extra numbers, folded by min/max, are exact at every level and give TFDM/RMIP's bound identically rather than approximately. Decided; see [[Project — Conservative metric queries without tessellation]] §2.2.
- **The slab is still the fine-level asset.** As a scalar interval the node's recovery is always worse than min-max, because collapsing a tilted region to an axis-aligned range costs the plane excursion. As a region in space the slab is thinner — 0.0044 against a min-max half-width of 0.0324 at rock's texel cells, 7.3× — which is what a ray test or a distance lower bound can use. The two bounds are for different query shapes and neither subsumes the other, and Part 3 locates the crossover at 8-texel cells on both textures.
- **The sampling gap is an estimate problem, not a tightness problem.** Tightening bounds does nothing for the descent weights. Moment channels are the measured candidate, recorded and not implemented ([[Plan — A-MVP sampling implementation]], open decisions).
- **Channels earn their place per level, and it is measurable which.** Moments buy nothing below 4-texel cells; the slab is useless at the root; min-max h is weakest at texel cells. Level 0 alone holds three quarters of all nodes, so storing a channel only where it pays is most of its cost back.

Related: [[Taylor-model bound pyramid]] · [[Result — Phase 1 Taylor pyramid]] · [[Result — A-MVP receiver irradiance study]] · [[Log — Conservative metric queries]] · [[Project — Conservative metric queries without tessellation]] · [[Plan — A-MVP sampling implementation]]
