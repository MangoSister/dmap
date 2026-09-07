---
title: Result — A-MVP rendered images
tags: [project, result, phase2, sampling, rendering]
created: 2026-08-28
---

# Result — A-MVP rendered images

S8 of [[Plan — A-MVP sampling implementation]]: the displaced emitter as a light in a renderer, and the comparison images that seed the Phase 2 gate demo. Code: `code/cpp/src/displaced_emitter_light.{h,cpp}` (the light) and `code/cpp/src/render_displaced_emitter.cpp` (the experiment), config `code/data/configs/render_displaced_emitter.toml`. FLIP: `code/python/poc/experiments/flip_s8.py`. Images (PNG) and metric CSVs archived in `code/data/results/s8/`; linear EXRs in the task output directory.

## Setup

- Scene: the S7 asset (cc_torus triangle, rock displacement at amplitude 0.2, 65-node grid) as a two-sided emitter over a diffuse floor (albedo 0.7), direct lighting only. Emission: 8×8 checkerboard with exact-zero cells at radiance 8, warm tint — the zero cells make the certified-zero pruning visible in the picture.
- The light (`DisplacedEmitterLight`) mirrors the ks light contract: sample returns direction, distance, and a solid-angle pdf (pdf_area · r²/|cosθ_e|); the pdf-of-direction for MIS takes a BSDF hit's (u, v) from the S2 mesh and re-derives everything from the smooth surface, so both sides of the MIS weight use identical arithmetic and the mesh (n_tess = 128, finer than a texel) enters only through visibility.
- Two viewpoints: an overview (cam1, emitter and floor) and a grazing near-field view along the floor (cam2). Reference: MIS at 1024 spp per viewpoint. 384×288.

## (1) MIS correctness check (cam1, product-descent light, 64 spp)

| strategy | mean-luminance deviation from reference | relative MSE | mean FLIP |
|---|---|---|---|
| NEE only | 0.10% | 0.084 | 0.066 |
| BSDF sampling only | 0.74% | 3.36 | 0.64 |
| MIS | 0.09% | 0.025 | 0.065 |

All three converge to the same image (gate: 2%). **PASS.** The BSDF-only render is far noisier, as it must be for a small structured emitter; MIS beats NEE-only on relative MSE by 3.4× because BSDF sampling covers the floor region right under the emitter where the solid angle is large.

## (2) Equal-sample ladder (NEE only, 16 spp)

| light sampler                              | cam1 rel MSE | cam1 FLIP | cam2 rel MSE | cam2 FLIP |
| ------------------------------------------ | ------------ | --------- | ------------ | --------- |
| uniform (Ling line casting)                | 3.21         | 0.579     | 1.49         | 0.418     |
| emission-only table                        | 0.108        | 0.128     | 0.097        | 0.084     |
| product table                              | 0.087        | 0.117     | 0.049        | 0.078     |
| area-only descent                          | 0.233        | 0.207     | 0.138        | 0.139     |
| product descent                            | 0.130        | 0.124     | 0.052        | 0.079     |
| receiver-aware descent (no emitter cosine) | **0.085**    | **0.116** | **0.047**    | **0.076** |

- The image-space ordering reproduces the S7 variance study: uniform far behind everything (25–35× the relative MSE of the informed samplers, a visibly noise-dominated image), area-only next, product samplers close together, and the receiver-aware descent best on both viewpoints — it is the only sampler whose ranking a frozen table cannot match, and in the grazing view it leads on both metrics.
- The uniform baseline enters next-event estimation as one line per sample: cast a line through the emitter's bounding box, sum the direct-lighting integrand over its hits with the 2 × offset-area constant, reweighting each hit from mesh area to smooth area as in S7. It is NEE-only: a line yields a correlated hit set, not a point with a density, so it has no per-point pdf and cannot join the MIS comparison.
- Product descent trails the product table by the same coarse-level midpoint √det G margin measured in S7 (rel MSE 0.130 vs 0.087 in cam1); on FLIP the two are nearly tied — the residual noise the table wins on sits mostly in already-bright regions the perceptual metric discounts.

## Notes

- Renders are fast: the full task (two references at 1024 spp, three strategy renders, ten ladder renders) takes about 11 seconds with the parallel tile loop, so higher-spp gate figures are cheap when needed.
- `flip-evaluator` 1.7 was installed into the dmap conda env for the FLIP metric.

Related: [[Plan — A-MVP sampling implementation]] · [[Result — A-MVP receiver irradiance study]] · [[Log — Conservative metric queries]] · [[Project — Conservative metric queries without tessellation]]
