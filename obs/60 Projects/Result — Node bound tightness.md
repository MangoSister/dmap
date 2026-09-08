---
title: Result — Node bound tightness
tags: [project, result, ray]
created: 2026-09-07
---

# Result — Node bound tightness

T3 of [[Plan — Path tracer with displaced surfaces]]: the per-node tests of the traversal, TFDM's box and the Taylor slab of D3, measured on the scene set of T2 by the task `validate_node_bounds` (`code/cpp/src/validate_node_bounds.cpp`, config `code/data/configs/validate_node_bounds.toml`, log `code/data/results/t3_validate_node_bounds.log`). Reasoning and decisions are in [[Log — Conservative metric queries]] (2026-09-07, T3).

## Setup

For sampled base triangles, every cell of every level from the footprint's roots to the leaves. Per cell, 16 surface points uniform in the cell's part of the triangle, and per point one ray through it from a random direction at a random distance between 0.03 and 3 mean edges. The three tests run on each ray: the box (TFDM's AABB in the uv-aligned tangent space, affine arithmetic over the cell with the min-max channel), the slab (D3's linear form with interval coefficients, anchored at the entry of the cell's column and clipped to that column), and both (the intersection). Interval lengths are in object units. The slab is read at every level here; the traversal reads it only up to `slab_max_level`.

Conservativeness, all three scenes: 0 violations in 91,568 surface points (box, slab, the exact slab identity F = m_z ρ, and the per-triangle object-space AABB) and 0 in 91,568 rays (box, slab, both contain the sample's t).

## Tables

Median interval length per level; "slab<box" is the fraction of rays where the slab interval is the shorter one.

S1, the experiment triangle, rock at 64 texels, whole tile (levels 0 to 5 are 1 to 32 texels):

| level | cells | rays | box | slab | both | slab/box | slab<box |
|---|---|---|---|---|---|---|---|
| 0 | 2080 | 33280 | 3.80e-4 | 9.85e-5 | 9.69e-5 | 0.26 | 0.89 |
| 1 | 528 | 8448 | 7.53e-4 | 5.09e-4 | 4.87e-4 | 0.68 | 0.64 |
| 2 | 136 | 2176 | 1.45e-3 | 1.42e-3 | 1.31e-3 | 0.98 | 0.33 |
| 3 | 36 | 576 | 2.72e-3 | 2.96e-3 | 2.63e-3 | 1.09 | 0.12 |
| 4 | 10 | 160 | 4.70e-3 | 6.27e-3 | 4.70e-3 | 1.34 | 0.01 |
| 5 | 3 | 48 | 6.51e-3 | 1.15e-2 | 6.51e-3 | 1.76 | 0.00 |

S2, cc_torus with its own layout, rock at 256 texels, 32 triangles (a triangle spans about 2 texels; levels above 2 are mostly cells larger than the triangle):

| level | cells | rays | box | slab | both | slab/box | slab<box |
|---|---|---|---|---|---|---|---|
| 0 | 914 | 13635 | 4.62e-4 | 2.02e-4 | 2.00e-4 | 0.44 | 0.87 |
| 1 | 383 | 5424 | 9.52e-4 | 6.81e-4 | 6.60e-4 | 0.72 | 0.70 |
| 2 | 108 | 1322 | 1.74e-3 | 1.48e-3 | 1.45e-3 | 0.85 | 0.51 |
| 3 | 32 | 430 | 2.52e-3 | 2.30e-3 | 2.26e-3 | 0.91 | 0.37 |
| 4 | 16 | 167 | 5.03e-3 | 4.98e-3 | 4.75e-3 | 0.99 | 0.18 |
| 5 | 8 | 39 | 1.03e-2 | 1.17e-2 | 1.03e-2 | 1.13 | 0.00 |
| 6 | 4 | 11 | 1.36e-2 | 1.36e-2 | 1.36e-2 | 1.00 | 0.00 |
| 7 | 2 | 2 | 2.54e-2 | 2.54e-2 | 2.54e-2 | 1.00 | 0.00 |

S3, spot with cobble at 128 texels tiled four times, 32 triangles:

| level | cells | rays | box | slab | both | slab/box | slab<box |
|---|---|---|---|---|---|---|---|
| 0 | 1240 | 17171 | 2.69e-3 | 2.21e-3 | 1.84e-3 | 0.82 | 0.46 |
| 1 | 456 | 5774 | 5.06e-3 | 5.47e-3 | 4.40e-3 | 1.08 | 0.28 |
| 2 | 191 | 2076 | 9.04e-3 | 1.21e-2 | 8.85e-3 | 1.34 | 0.10 |
| 3 | 78 | 710 | 1.47e-2 | 2.39e-2 | 1.47e-2 | 1.62 | 0.00 |
| 4 | 16 | 119 | 1.72e-2 | 2.45e-2 | 1.72e-2 | 1.42 | 0.00 |

## Findings

- **Both tests are conservative**, and so is the per-triangle AABB, on real layouts, a tiled chart, and the experiment triangle: 0 violations in 183k checks.
- **The slab wins at the leaves by a wide margin**: at level 0 the median interval is 3.9× shorter than the box on S1, 2.3× on S2, 1.2× on S3. This is where a traversal spends most of its node tests.
- **The crossover along a random ray is finer than the channel study's.** Slab shorter than box at and below level 2 on S1 (4-texel cells), level 4 on S2, level 0 on S3; the channel study measured the region itself thinner below 8-texel cells. Two things separate the measures: the ray-interval length adds the obliquity of random directions and the widening from taking the normal's interval hull in the coefficients, which grows with the cell, and the tiled S3 chart puts more texture frequency into each cell. Anchoring the slab form at the column's entry instead of the ray origin moved S1's crossover from level 1 to level 2 and cut the leaf interval by a quarter; the remaining looseness is the hull.
- **Both is never longer than the box and strictly shorter below level 4 on S1 and S2**, by 3% at S1's level 3 and 66–74% at level 0. The level policy `slab_max_level` therefore only decides where the slab's cost is worth paying, not whether it can hurt; T5 measured that in node tests and time and settled the default at 1 ([[Result — Traversal cost study]]).
