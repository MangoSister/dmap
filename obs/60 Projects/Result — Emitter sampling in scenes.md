---
title: Result — Emitter sampling in scenes
tags: [project, result, phase2, sampling, rendering, ray-tracing]
created: 2026-09-07
---

# Result — Emitter sampling in scenes

T9 of [[Plan — Path tracer with displaced surfaces]]: the S8 ladder of [[Result — A-MVP rendered images]] run inside the path tracer's scenes, on multi-triangle displaced emitters, with the uniform line-casting baseline adapted to the traverser. Code: `code/cpp/src/test_emitter_ladder.cpp` (the task), `direct_lighting.{h,cpp}` (the integrator, shared with T8's MIS check), config `code/data/configs/test_emitter_ladder.toml`, FLIP `code/python/poc/experiments/flip_t9.py`. Log `code/data/results/t9_test_emitter_ladder.log`, tables `t9_ladder_s6.csv`, `t9_ladder_s7.csv`, `t9_flip_s6.csv`, `t9_flip_s7.csv`, images `t9_images/`. Reasoning in [[Log — Conservative metric queries]] (2026-09-07, T9).

## Setup

- Scenes: S6, two panels displaced with the sci-fi map at 512 texels, four emitting base triangles, over a diffuse floor and torus, no other light; S7, the station ring (4,608 base triangles, 4,416 with emission mass, the sci-fi tile repeated 16 × 4) around a brushed-metal bunny, plus a directional sun. Direct lighting at 200 × 150.
- Samplers: emission table, product table, area-only descent, product descent, receiver-aware descent (no emitter cosine), β = 0.05, all through the `AreaLight` interface of T8 (the sampler within a triangle; ks's power-based light sampler selects the triangle); the uniform baseline as one line per sample through the union box of the emitters, every crossing found by the scene's own intersection with the near distance advanced past each hit, next-event only.
- Strategies: next-event estimation alone and MIS, 64 samples per pixel; reference MIS at 4096 with the product descent. Delta lights are sampled under every strategy.
- Metrics: relative MSE on luminance and mean FLIP against the reference, over the whole frame, over the receivers (pixels whose first hit is not an emitter: 16,038 on S6, 1,413 on S7) and over the far receivers (at least 0.25 units from every emitter's base mesh: 15,716 and 1,056). Draw cost per light sample on the receivers, one thread, selection plus sample without the shadow ray (a line with all its crossings for the uniform baseline).
- Gate: MIS mean within 2% on the whole frame at 64 samples per pixel; next-event estimation alone within 2% on the far receivers at 2048, since it converges from below where an emitter lights a receiver at close range (T8); each plus two standard errors of its own noise, 2 √(relative MSE / pixels). **PASS on both scenes**, every sampler.

## S6, the panels

Relative MSE over the receivers and the far receivers, FLIP over the frame, at 64 samples per pixel.

| sampler | NEE only, receivers | NEE only, far | NEE FLIP | MIS, receivers | MIS, far | MIS FLIP | draw ns |
|---|---|---|---|---|---|---|---|
| uniform (Ling line casting) | 10.31 | 6.35 | 0.686 | (NEE only) | | | 1865 per line |
| emission-only table | 0.244 | 0.202 | 0.331 | 0.174 | 0.172 | 0.263 | 508 |
| product table | **0.143** | **0.114** | 0.308 | **0.109** | **0.104** | **0.251** | 509 |
| area-only descent | 6.09 | 5.97 | 0.579 | 1.52 | 1.54 | 0.489 | 2459 |
| product descent | 1.15 | 1.29 | 0.333 | 0.857 | 0.982 | 0.267 | 2053 |
| receiver-aware descent | 0.312 | 0.282 | **0.305** | 0.231 | 0.235 | 0.252 | 2483 |

Gate renders, next-event only at 2048 samples per pixel on the far receivers (mean deviation, relative MSE): emission table −0.48%, 0.011; product table −0.67%, 0.0066; area descent −0.05%, 0.267; product descent −0.99%, 0.0129; receiver descent −0.33%, 0.0175; uniform −0.54%, 0.290. The whole-frame relative MSE under next-event estimation alone is 4.7 to 267 at 64 samples per pixel and 0.65 to 22 at 2048: the panels light themselves at millimetre range (T8), and those pixels carry the tail.

- The A-MVP's ordering holds with one change: the product descent trails the product table 8× on the receivers instead of 1.5× (S8), and at 2048 samples per pixel still 1.9×. On FLIP the product descent ties the emission table, so the loss sits in a few bright samples, which the perceptual metric discounts. The receiver-aware descent, which shares those weights, comes third rather than first.
- The uniform baseline is 40 to 70× the informed samplers on the receivers and remains 20 to 40× at 2048 samples per pixel: the box around two leaning panels is 5.8 × 2.4 × 1.0 units for 8 units² of surface, of which 5% emits, so a line finds an emitting crossing once in 200 draws.

### Why the product descent trails: the density check

The task compares each kind's area density with the product table's at 20,000 random points of one emitting triangle (a receiver two units in front of the triangle for the receiver-aware kind). Percentiles of the ratio:

| kind | 1% | 10% | 50% | 90% | 99% | worst |
|---|---|---|---|---|---|---|
| emission-only table (sci-fi) | 0.20 | 0.40 | 1.66 | 2.19 | 2.19 | 2.19 |
| product descent (sci-fi) | 0.10 | 0.32 | 0.93 | 2.58 | 5.60 | 32.7 |
| receiver-aware descent (sci-fi) | 0.07 | 0.27 | 0.85 | 2.61 | 5.18 | 39.0 |
| area-only descent (sci-fi) | 0.007 | 0.02 | 0.08 | 0.32 | 1.54 | 96,000 |
| product descent (rock map, same scene) | 0.62 | 0.68 | 0.86 | 1.59 | 3.28 | 9.2 |

With the rock map in the same scene at the same relief (strength 0.1, midlevel 0.5) the product descent beats both tables on the receivers (0.105 against 0.128 and 0.129) and the receiver-aware descent is best (0.085), the S8 picture. The sci-fi map is plateaus and steps. The descent's weight at each level is the emission mass times √det G evaluated from the node's Taylor plane at the cell's centre; over a step the plane's slope grows as the cells shrink, so the ratios of the levels do not compose to the leaf's own √det G, which the table reads pointwise, and the density lands a factor 0.1 to 5.6 from the target. This is the coarse-level midpoint composition measured in [[Result — Pyramid channel study]] and the A-MVP's 1.3 to 3.1× concession on rough content, made worse by steps. The emission table's own spread (0.2 to 2.2) says the metric factor varies 10× across this map's emitting texels, so the factor is worth having; it needs a per-cell estimate better than the plane at the centre. The moment channels recorded among the open decisions of [[Plan — A-MVP sampling implementation]] fold exactly and are the remedy this measurement calls for.

## S7, the station ring

| sampler | NEE only, receivers | NEE only, far | NEE FLIP | MIS, receivers | MIS, far | MIS FLIP | draw ns |
|---|---|---|---|---|---|---|---|
| uniform (Ling line casting) | 9.02 | 12.1 | 0.121 | (NEE only) | | | 8401 per line |
| emission-only table | 1.86 | 2.48 | 0.112 | 0.388 | 0.466 | 0.064 | 495 |
| product table | 1.67 | 2.22 | **0.111** | **0.368** | 0.444 | 0.064 | 512 |
| area-only descent | 1.66 | 2.17 | 0.123 | 0.461 | 0.552 | 0.069 | 1226 |
| product descent | 58.8 | 78.8 | 0.112 | 0.372 | **0.433** | 0.064 | 973 |
| receiver-aware descent | 59.9 | 80.2 | 0.112 | 0.374 | 0.436 | 0.064 | 1099 |

Gate renders, next-event only at 2048 samples per pixel on the far receivers (mean deviation, relative MSE, allowance): emission table −0.92%, 0.49, 4.3%; product table +0.06%, 0.26, 3.1%; area descent −2.61%, 2.23, 9.2%; product descent −0.35%, 0.45, 4.2%; receiver descent −0.18%, 0.53, 4.5%; uniform −0.16%, 1.38, 7.2%. The 58.8 and 59.9 of the descents at 64 samples per pixel are single fireflies on the bunny (the same tail as on S6, once); at 2048 the receivers read 0.35 and 0.42 against 0.20 for the product table and 0.38 for the emission table.

- Under MIS every kind is within 1.1% of the reference and within 25% of each other in relative MSE; the product table, product descent and receiver-aware descent are within 2% of each other. The whole-frame relative MSE is 0.048 for all of them, since the frame is mostly the directly visible ring.
- The ring's base triangles are 0.4 units across and 0.5 units or more from the bunny, so the receiver term hardly varies within a triangle, and the variance is decided by ks's selection among 4,416 lights by power alone, which is the same for every kind. The receiver-aware descent works inside a triangle; a receiver-aware selection among triangles (a light BVH, master plan §5A baseline 3) is the missing layer, orthogonal to the per-triangle sampler.
- A line of the uniform baseline costs 8.4 µs here: it crosses the ring twice and the 144,000-triangle bunny, and every crossing restarts the intersection. Its relative MSE at 2048 samples per pixel (1.57 on the receivers) is still above the informed samplers' at 64 under MIS.

## Cost per footprint under sharing (D5)

| scene | kind | lights | build | owned by the triangles | per light | shared (pyramid, node grid, emission) | per light with sharing |
|---|---|---|---|---|---|---|---|
| S6 | tables | 4 | 15 to 17 ms | 4.1 MB | 1.0 MB | 34.0 MB | 1.0 MB |
| S6 | descents | 4 | 0.4 to 0.9 ms | 256 KB | 64 KB | 34.0 MB | 8.6 MB |
| S7 | tables | 4,416 | 540 to 610 ms | 140 MB | 32.5 KB | 28.7 MB | 32.5 KB |
| S7 | descents | 4,416 | 200 to 215 ms | 172 MB | 40 KB | 28.7 MB | 46.5 KB |

- A panel triangle's table covers the whole 512 × 512 box (1 MB) and its footprint tree only the diagonal (64 KB), but four triangles cannot amortize 34 MB of shared data. A ring triangle's table box is small (32.5 KB) while its footprint tree still holds every straddling cell along three edges at every level, 96 bytes each (40 KB), so on S7 the hierarchy costs 1.4× the table per footprint even with sharing.
- The hierarchy's memory case therefore needs both many triangles per tile and a compact footprint node (a float area and mass and a byte for the overlap class would be 4× smaller; recorded, not built). Build and edit costs favour it at any count, as in S7 of the A-MVP.
- Draw cost: tables 0.5 µs; descents 2.0 to 2.5 µs on S6 (nine levels of a 512-texel tile) and 1.0 to 1.2 µs on S7 (small triangles, shallow roots); lines 1.9 µs on S6 and 8.4 µs on S7.

## Conclusions for the gate

- Every sampler is unbiased in the integrated renderer, under next-event estimation alone and under MIS, on scenes with two and with 4,416 emitting base triangles.
- The product samplers beat the uniform baseline by 40 to 70× on the panels and by 5 to 10× on the ring at equal samples, and by more at equal time.
- The midpoint product descent is the weak sampler on step-like content: its per-level √det G composition is measured at 0.1 to 5.6× the target density on the sci-fi map, and it trails the product table 8× at 64 samples per pixel where on rock it wins. The moment channels of the A-MVP open decisions are the fix to test next if the sampling application stays the headline.
- With many small emitting triangles the choice of sampler within a triangle is secondary to the selection among triangles, which ks makes by power alone.

Related: [[Plan — Path tracer with displaced surfaces]] · [[Result — A-MVP rendered images]] · [[Result — A-MVP receiver irradiance study]] · [[Result — Pyramid channel study]] · [[Plan — A-MVP sampling implementation]] · [[Log — Conservative metric queries]] · [[Project — Conservative metric queries without tessellation]]
