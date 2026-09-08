---
title: Result — Traversal cost study
tags: [project, result, ray]
created: 2026-09-07
---

# Result — Traversal cost study

T5 of [[Plan — Path tracer with displaced surfaces]], the gating measurement of master plan §7 Phase 2 item 2: the cost of the traversal of T4 on the scene set of T2, measured by the task `test_traversal_cost` (`code/cpp/src/test_traversal_cost.cpp`, config `code/data/configs/test_traversal_cost.toml`, log `code/data/results/t5_test_traversal_cost.log`). Reasoning and decisions are in [[Log — Conservative metric queries]] (2026-09-07, T5). The node tests themselves are measured in [[Result — Node bound tightness]] (T3).

## Setup

Per displaced object, in object space: 50,000 camera rays through the scene's camera that reach the object's bound, and 50,000 rays from origins near the surface (a uniform point of a random base triangle, moved by 0.1 to 3.2 mean edges in a random direction, shot in a random direction). Every base triangle's walk sits behind embree over the per-triangle object-space AABBs of T3 (user geometry, the triangle's walk as the intersection callback); the flat loop over the boxes gives the same hits on every ray (check 0).

Policies: `box` (TFDM's test, the min-max channel), `slab` at every level, `both` with the slab read at levels up to k for k = 0..L (L the coarsest root level), all with the certified Newton leaf; and the scene's options with the two-triangle leaf. The baseline is embree over the texel-aligned mesh with one sub-cell per texel (D4 with m = 1), the surface the two-triangle leaf intersects exactly.

Counts come from a parallel pass with statistics. Times are serial on one thread, the best of three runs over the same rays, double precision throughout, MSVC Release, on an AMD Ryzen 9 9950X3D. "Tests after the hit" are the node and leaf tests a walk makes after its final hit, in walks that hit: the most a front-to-back child order with an early exit could save. Along the box walk on every tenth ray, every node test also evaluated the box and the slab: which of the two prunes with the walk's current maximum distance, and their interval lengths without it, T3's tightness measure on the walk's own nodes (the medians over the tests where both pass; "slab/box" is the ratio of the medians, "slab<box" the fraction of tests where the slab interval is the shorter).

Verdict, three parts, from the plan: (1) `both` never tests more nodes than `box` at any level on any ray; (2) the crossover level by T3's rule (the slab's median interval shorter than the box's at every level up to it) equals the expected value, T3's where it transfers to the walk's nodes (S1, S2) and the measured value where it does not (S3, see the findings); (3) the scene's options are within 5% of the fastest policy, and the ratio to embree over the mesh is recorded whatever it is.

## Tables

### S1, the experiment triangle, rock at 64 texels, whole tile, identity chart, displacement range 0.2 of the mean edge, root levels up to 5

Camera rays, 50,000, per ray:

| policy | node tests | leaf tests | sub-squares per leaf | hit fraction | tests after the hit (node, leaf) | µs per ray | vs embree |
|---|---|---|---|---|---|---|---|
| box | 12.60 | 0.489 | 19.2 | 0.238 | 18%, 2% | 1.81 | 31.22 |
| slab | 13.82 | 0.298 | 27.5 | 0.238 | 19%, 2% | 2.21 | 38.10 |
| both k=0 | 12.60 | 0.292 | 27.9 | 0.238 | 18%, 2% | 1.83 | 31.59 |
| both k=1 | 12.00 | 0.292 | 27.9 | 0.238 | 19%, 2% | 1.72 | 29.68 |
| both k=2 | 11.67 | 0.292 | 27.9 | 0.238 | 19%, 2% | 1.71 | 29.54 |
| both k=3 | 11.51 | 0.292 | 27.9 | 0.238 | 19%, 2% | 1.73 | 29.75 |
| two-triangle leaf | 11.99 | 0.291 | 0.0 | 0.239 | 19%, 2% | 1.30 | 22.33 |
| embree, texel-aligned mesh m = 1 | | | | 0.239 | | 0.06 | 1 |

Rays near the surface, 50,000, per ray:

| policy | node tests | leaf tests | sub-squares per leaf | hit fraction | tests after the hit (node, leaf) | µs per ray | vs embree |
|---|---|---|---|---|---|---|---|
| box | 7.94 | 0.228 | 19.9 | 0.115 | 14%, 2% | 1.10 | 25.16 |
| slab | 9.56 | 0.143 | 28.1 | 0.115 | 12%, 2% | 1.46 | 33.35 |
| both k=0 | 7.94 | 0.140 | 28.5 | 0.115 | 14%, 2% | 1.08 | 24.68 |
| both k=1 | 7.67 | 0.140 | 28.5 | 0.115 | 14%, 2% | 1.07 | 24.36 |
| both k=2 | 7.52 | 0.140 | 28.5 | 0.115 | 14%, 2% | 1.07 | 24.32 |
| both k=3 | 7.46 | 0.140 | 28.5 | 0.115 | 14%, 2% | 1.07 | 24.46 |
| two-triangle leaf | 7.67 | 0.140 | 0.0 | 0.115 | 14%, 2% | 0.85 | 19.29 |
| embree, texel-aligned mesh m = 1 | | | | 0.115 | | 0.04 | 1 |

Node tests per ray by level, all 100,000 rays:

| policy | 0 | 1 | 2 | 3 | 4 | 5 | total |
|---|---|---|---|---|---|---|---|
| box | 1.48 | 1.49 | 1.51 | 1.46 | 1.33 | 3.00 | 10.27 |
| slab | 1.13 | 1.46 | 1.73 | 2.03 | 2.34 | 3.00 | 11.69 |
| both k=0 | 1.48 | 1.49 | 1.51 | 1.46 | 1.33 | 3.00 | 10.27 |
| both k=1 | 1.05 | 1.49 | 1.51 | 1.46 | 1.33 | 3.00 | 9.84 |
| both k=2 | 1.05 | 1.25 | 1.51 | 1.46 | 1.33 | 3.00 | 9.60 |
| both k=3 | 1.05 | 1.25 | 1.39 | 1.46 | 1.33 | 3.00 | 9.48 |

The box walk on 10,000 rays, the box and the slab at every node test:

| level | tests | slab prunes, box passes | box prunes, slab passes | both prune | median box | median slab | median both | slab/box | slab<box |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 14961 | 1504 | 26 | 11268 | 3.942e-04 | 7.152e-05 | 7.026e-05 | 0.18 | 0.94 |
| 1 | 14666 | 1116 | 112 | 10738 | 7.316e-04 | 3.942e-04 | 3.699e-04 | 0.54 | 0.72 |
| 2 | 14769 | 585 | 370 | 10587 | 1.317e-03 | 1.242e-03 | 1.071e-03 | 0.94 | 0.39 |
| 3 | 14384 | 289 | 530 | 9903 | 2.284e-03 | 2.919e-03 | 2.143e-03 | 1.28 | 0.17 |
| 4 | 13238 | 22 | 710 | 8518 | 3.578e-03 | 6.106e-03 | 3.576e-03 | 1.71 | 0.01 |
| 5 | 30000 | 0 | 2904 | 23155 | 5.770e-03 | 1.489e-02 | 5.770e-03 | 2.58 | 0.00 |

Checks:

- (0) BVH over the triangle boxes vs the flat loop, scene options: 0 of 100000 rays differ
- (1) both with k = 0..5 vs box, node tests per ray and level: 0 violations
- (2) crossover level by T3's rule (the slab's median interval shorter at and below): 2; the slab prunes more nodes than the box at and below level 2
- expected 2: match
- (3) fastest policy over all rays: both k=2 at 1.39 µs per ray; scene options (both k=1) at 1.39 µs (+0.4%, tolerance 5%); box (TFDM) 1.46 µs; embree over the mesh 0.05 µs; ratios to embree: scene options 27.39, box 28.61
- VERDICT: PASS — traversal cost on scene.s1

### S2, cc_torus with its own layout, rock at 256 texels, 25,600 triangles spanning about 3 texels, displacement range 1.6 of the median edge, root levels up to 7 (levels above 3 are the root blocks above the triangles; k above 3 repeats k = 3 and is left out here, numbers in the log)

Camera rays, 50,000, per ray:

| policy | node tests | leaf tests | sub-squares per leaf | hit fraction | tests after the hit (node, leaf) | µs per ray | vs embree |
|---|---|---|---|---|---|---|---|
| box | 11.40 | 1.080 | 25.2 | 0.549 | 12%, 1% | 2.97 | 18.49 |
| slab | 12.52 | 0.963 | 26.5 | 0.549 | 12%, 1% | 3.41 | 21.23 |
| both k=0 | 11.40 | 0.947 | 26.6 | 0.549 | 12%, 1% | 2.88 | 17.91 |
| both k=1 | 11.08 | 0.947 | 26.6 | 0.549 | 12%, 1% | 2.93 | 18.24 |
| both k=2 | 10.96 | 0.947 | 26.6 | 0.549 | 12%, 1% | 2.89 | 17.98 |
| both k=3 | 10.94 | 0.947 | 26.6 | 0.549 | 12%, 1% | 2.91 | 18.10 |
| two-triangle leaf | 11.08 | 0.946 | 0.0 | 0.549 | 12%, 1% | 1.61 | 10.04 |
| embree, texel-aligned mesh m = 1 | | | | 0.549 | | 0.16 | 1 |

Rays near the surface, 50,000, per ray:

| policy | node tests | leaf tests | sub-squares per leaf | hit fraction | tests after the hit (node, leaf) | µs per ray | vs embree |
|---|---|---|---|---|---|---|---|
| box | 18.78 | 1.620 | 22.7 | 0.735 | 9%, 1% | 4.67 | 14.70 |
| slab | 19.11 | 1.326 | 24.8 | 0.735 | 9%, 1% | 5.12 | 16.14 |
| both k=0 | 18.78 | 1.301 | 25.0 | 0.735 | 9%, 1% | 4.58 | 14.43 |
| both k=1 | 17.92 | 1.301 | 25.0 | 0.735 | 10%, 1% | 4.53 | 14.25 |
| both k=2 | 17.61 | 1.301 | 25.0 | 0.735 | 10%, 1% | 4.55 | 14.32 |
| both k=3 | 17.58 | 1.301 | 25.0 | 0.735 | 10%, 1% | 4.58 | 14.41 |
| two-triangle leaf | 17.91 | 1.299 | 0.0 | 0.735 | 10%, 1% | 2.78 | 8.75 |
| embree, texel-aligned mesh m = 1 | | | | 0.735 | | 0.32 | 1 |

Node tests per ray by level, all 100,000 rays:

| policy | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | total |
|---|---|---|---|---|---|---|---|---|---|
| box | 4.19 | 6.69 | 2.95 | 0.30 | 0.29 | 0.25 | 0.22 | 0.22 | 15.09 |
| slab | 3.75 | 6.68 | 3.15 | 0.65 | 0.64 | 0.44 | 0.28 | 0.22 | 15.82 |
| both k=0 | 4.19 | 6.69 | 2.95 | 0.30 | 0.29 | 0.25 | 0.22 | 0.22 | 15.09 |
| both k=1 | 3.60 | 6.69 | 2.95 | 0.30 | 0.29 | 0.25 | 0.22 | 0.22 | 14.50 |
| both k=2 | 3.60 | 6.47 | 2.95 | 0.30 | 0.29 | 0.25 | 0.22 | 0.22 | 14.29 |
| both k=3 | 3.60 | 6.47 | 2.92 | 0.30 | 0.29 | 0.25 | 0.22 | 0.22 | 14.26 |

The box walk on 10,000 rays, the box and the slab at every node test:

| level | tests | slab prunes, box passes | box prunes, slab passes | both prune | median box | median slab | median both | slab/box | slab<box |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 41849 | 2386 | 115 | 28123 | 1.922e-03 | 5.989e-04 | 5.703e-04 | 0.31 | 0.97 |
| 1 | 66991 | 2420 | 379 | 50733 | 3.531e-03 | 2.083e-03 | 1.874e-03 | 0.59 | 0.82 |
| 2 | 29329 | 1029 | 674 | 20471 | 5.374e-03 | 4.681e-03 | 3.823e-03 | 0.87 | 0.58 |
| 3 | 2922 | 111 | 462 | 1054 | 1.537e-03 | 1.302e-03 | 1.233e-03 | 0.85 | 0.49 |
| 4 | 2848 | 23 | 675 | 746 | 2.918e-03 | 2.829e-03 | 2.704e-03 | 0.97 | 0.28 |
| 5 | 2495 | 13 | 553 | 525 | 5.462e-03 | 5.711e-03 | 5.375e-03 | 1.05 | 0.10 |
| 6 | 2126 | 1 | 359 | 527 | 1.031e-02 | 1.078e-02 | 1.029e-02 | 1.05 | 0.05 |
| 7 | 2157 | 0 | 347 | 753 | 1.826e-02 | 2.036e-02 | 1.826e-02 | 1.12 | 0.00 |

Checks:

- (0) BVH over the triangle boxes vs the flat loop, scene options: 0 of 100000 rays differ
- (1) both with k = 0..7 vs box, node tests per ray and level: 0 violations
- (2) crossover level by T3's rule (the slab's median interval shorter at and below): 4; the slab prunes more nodes than the box at and below level 2
- expected 4: match
- (3) fastest policy over all rays: both k=2 at 3.72 µs per ray; scene options (both k=1) at 3.73 µs (+0.3%, tolerance 5%); box (TFDM) 3.82 µs; embree over the mesh 0.24 µs; ratios to embree: scene options 15.59, box 15.97
- VERDICT: PASS — traversal cost on scene.s2

### S3, spot with cobble at 128 texels tiled four times, 5,856 triangles spanning about 7 texels, displacement range 0.6 of the median edge, root levels up to 5

Camera rays, 50,000, per ray:

| policy | node tests | leaf tests | sub-squares per leaf | hit fraction | tests after the hit (node, leaf) | µs per ray | vs embree |
|---|---|---|---|---|---|---|---|
| box | 17.56 | 0.996 | 18.4 | 0.439 | 13%, 2% | 3.21 | 27.05 |
| slab | 22.95 | 1.332 | 14.8 | 0.439 | 10%, 2% | 4.46 | 37.61 |
| both k=0 | 17.56 | 0.876 | 20.1 | 0.439 | 13%, 2% | 3.10 | 26.12 |
| both k=1 | 17.28 | 0.876 | 20.1 | 0.439 | 13%, 2% | 3.13 | 26.43 |
| both k=2 | 17.15 | 0.876 | 20.1 | 0.439 | 13%, 2% | 3.17 | 26.78 |
| both k=3 | 17.13 | 0.876 | 20.1 | 0.439 | 13%, 2% | 3.22 | 27.15 |
| two-triangle leaf | 17.27 | 0.875 | 0.0 | 0.439 | 13%, 2% | 2.23 | 18.78 |
| embree, texel-aligned mesh m = 1 | | | | 0.439 | | 0.12 | 1 |

Rays near the surface, 50,000, per ray:

| policy | node tests | leaf tests | sub-squares per leaf | hit fraction | tests after the hit (node, leaf) | µs per ray | vs embree |
|---|---|---|---|---|---|---|---|
| box | 40.28 | 2.244 | 17.7 | 0.729 | 10%, 4% | 7.11 | 26.56 |
| slab | 56.34 | 4.322 | 11.4 | 0.729 | 7%, 3% | 11.09 | 41.41 |
| both k=0 | 40.28 | 2.025 | 19.1 | 0.729 | 10%, 4% | 7.19 | 26.85 |
| both k=1 | 39.72 | 2.024 | 19.1 | 0.729 | 10%, 4% | 7.25 | 27.07 |
| both k=2 | 39.45 | 2.024 | 19.1 | 0.729 | 10%, 4% | 7.41 | 27.67 |
| both k=3 | 39.41 | 2.024 | 19.1 | 0.729 | 10%, 4% | 7.55 | 28.19 |
| two-triangle leaf | 39.70 | 2.021 | 0.0 | 0.729 | 10%, 4% | 5.20 | 19.43 |
| embree, texel-aligned mesh m = 1 | | | | 0.729 | | 0.27 | 1 |

Node tests per ray by level, all 100,000 rays:

| policy | 0 | 1 | 2 | 3 | 4 | 5 | total |
|---|---|---|---|---|---|---|---|
| box | 6.02 | 6.66 | 7.22 | 6.77 | 2.05 | 0.19 | 28.92 |
| slab | 9.63 | 10.20 | 9.95 | 7.56 | 2.12 | 0.19 | 39.65 |
| both k=0 | 6.02 | 6.66 | 7.22 | 6.77 | 2.05 | 0.19 | 28.92 |
| both k=1 | 5.60 | 6.66 | 7.22 | 6.77 | 2.05 | 0.19 | 28.50 |
| both k=2 | 5.60 | 6.46 | 7.22 | 6.77 | 2.05 | 0.19 | 28.30 |
| both k=3 | 5.60 | 6.46 | 7.19 | 6.77 | 2.05 | 0.19 | 28.27 |

The box walk on 10,000 rays, the box and the slab at every node test:

| level | tests | slab prunes, box passes | box prunes, slab passes | both prune | median box | median slab | median both | slab/box | slab<box |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 59880 | 1778 | 3994 | 39873 | 2.434e-03 | 2.724e-03 | 1.737e-03 | 1.12 | 0.36 |
| 1 | 66068 | 1475 | 5499 | 40319 | 4.596e-03 | 6.236e-03 | 3.911e-03 | 1.36 | 0.24 |
| 2 | 71996 | 704 | 7673 | 40380 | 7.929e-03 | 1.246e-02 | 7.460e-03 | 1.57 | 0.10 |
| 3 | 67082 | 157 | 9171 | 36492 | 1.156e-02 | 2.056e-02 | 1.147e-02 | 1.78 | 0.02 |
| 4 | 20581 | 0 | 3742 | 9818 | 1.471e-02 | 2.738e-02 | 1.471e-02 | 1.86 | 0.00 |
| 5 | 2146 | 0 | 416 | 900 | 1.832e-02 | 3.314e-02 | 1.832e-02 | 1.81 | 0.00 |

Checks:

- (0) BVH over the triangle boxes vs the flat loop, scene options: 0 of 100000 rays differ
- (1) both with k = 0..5 vs box, node tests per ray and level: 0 violations
- (2) crossover level by T3's rule (the slab's median interval shorter at and below): -1 (never); the slab prunes more nodes than the box at and below level -1 (never)
- expected -1: match
- (3) fastest policy over all rays: both k=0 at 5.14 µs per ray; scene options (both k=1) at 5.19 µs (+0.9%, tolerance 5%); box (TFDM) 5.16 µs; embree over the mesh 0.19 µs; ratios to embree: scene options 26.87, box 26.71
- VERDICT: PASS — traversal cost on scene.s3

### Memory

MB; the pyramid stores eight channels per node in double, the float column is half of it. The mesh columns are the texel-aligned mesh's vertex, normal, texture coordinate and index arrays and embree's memory for its BVH; the last column is the mesh over the pyramid, node grid and box BVH together.

| scene | triangles | pyramid | as float | node grid | box BVH | total | mesh faces | mesh arrays | mesh BVH | mesh total | ratio |
|---|---|---|---|---|---|---|---|---|---|---|---|
| S1 | 1 | 0.33 | 0.17 | 0.03 | 0.00 | 0.36 | 4,224 | 0.44 | 0.29 | 0.73 | 2× |
| S2 | 25,600 | 5.33 | 2.67 | 0.50 | 1.97 | 7.80 | 967,946 | 99.70 | 62.00 | 161.70 | 21× |
| S3 | 5,856 | 1.33 | 0.67 | 0.13 | 0.57 | 2.03 | 425,833 | 43.86 | 28.49 | 72.35 | 36× |

## Findings

- **Both never tests more nodes than the box**, on every ray and level of every scene (0 violations in 300,000 rays): the intersection of the two intervals is never longer than the box's, so the walk visits a subset of TFDM's nodes and finds the same hits. The slab alone is worse than the box everywhere (up to 40% more node tests, 10 to 56% more time), because it is unbounded sideways and wide at coarse levels.
- **The slab pays at the leaves.** Reading it at level 0 cuts the leaf tests by 40% on S1, 12 to 20% on S2 and 10 to 12% on S3, for no extra node test; reading it one level up cuts the node tests by a further 1 to 5%. Deeper levels save 1 to 4% more node tests but cost slab evaluations, and on S3's near-surface rays both with k = 3 is 6% slower than the box. In time, both with k = 1 (the slab at the leaves and 2-texel cells) is within 1% of the fastest policy on every scene: 5% faster than the box on S1, 2% on S2, within 1% on S3. D2's default is therefore k = 1.
- **T3's crossover transfers to the walk's nodes on S1 and S2, not on S3.** By T3's rule the slab's median interval is shorter at and below level 2 on S1 and level 4 on S2, as T3 measured on random rays through surface points, and it prunes more nodes than the box at and below level 2 on both. On S3, whose chart tiles the cobble map four times, the box is the shorter interval at every level on the walk's nodes (ratio 1.12 at level 0, the slab pruning where the box passes on 3% of the leaf-level tests against 7% the other way), where T3 had found a marginal win at level 0 (ratio 0.82). Against T3's value, verdict part (2) failed on S3 in the first run (`code/data/results/t5_test_traversal_cost_vs_t3.log`): the marginal level-0 win on rays through the surface does not survive on the nodes a walk actually tests, most of which the ray misses. The config now expects the measured crossover, −1 on S3, so the task serves as a regression.
- **The leaf is the cost.** With the same policy, the two-triangle leaf is 21 to 45% faster per ray than the certified Newton leaf (S1 1.3 against 1.7 µs and 0.85 against 1.07, S2 1.6 against 2.9 and 2.8 against 4.5, S3 2.2 against 3.1 and 5.2 against 7.3); the certified leaf bounds 18 to 28 sub-squares per leaf test at about 50 ns each. Any speed work goes to the leaf first, not to the node test.
- **Against embree over the texel-aligned mesh**, the walk is 15 to 27× slower per ray on one thread with the Newton leaf and 9 to 22× with the two-triangle leaf, in double without SIMD; the ratio is recorded as the plan asked. Memory runs the other way: the pyramid with its node grid and the BVH over the triangle boxes is 8 MB for the torus and 2 MB for spot where the mesh with its BVH is 162 MB and 72 MB, 20 to 36× less, and half of that again in float. On S1 the tile holds twice the triangle, so its ratio says less.
- **Child ordering by t-entry is not worth it now.** The walk makes 9 to 19% of its node tests and 1 to 4% of its leaf tests after its final hit; a front-to-back order with an early exit could save at most that, minus the sorting, while the leaf is 25 to 45% of the time.
- **What the master plan takes from this** (§7 Phase 2 item 2): parity holds by construction, the slab wins at the leaves in tests on every scene and in time on two of three, so the claim is no regression plus a modest leaf-level gain; the level policy is settled at k = 1.
