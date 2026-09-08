---
title: Note — EG draft handoff after T1.5 timing
tags: [paper, eurographics, handoff, ray-tracing, CUDA, performance]
status: current
created: 2026-08-30
updated: 2026-08-30
---

# EG draft handoff after T1.5 timing

## What was tested

We tested whether T1's `0.177×` structural leaf-fetch count could translate into GPU time despite its p95 seven-segment tail. The T1.5-A device kernel consumes the exact CPU-certified merged leaf/height schedule, performs leaf min/max rejection, and invokes the same exact cubic leaf tests as the same-buffer NRT-QT baseline. It deliberately excludes schedule construction, including `11561` separator signs and `768` splits, and is therefore favorable to the candidate.

The frozen protocol uses the existing development/decision split, 32,768 replayed rays per cohort, 20 warm-ups, 50 randomized paired trials, and calibrated batches of at least 10 ms on an RTX 5090.

## Results ready to cite internally

All device outputs are regular, every candidate/NRT status-hit checksum matches, regenerated T1 schedules have zero survivor omissions and zero fallback, and all timing CVs are below `0.66%`.

| held-out cohort | T1 schedule envelope / NRT-QT |
|---|---:|
| ordinary | `0.9557×` |
| S0 affine | `1.3512×` |
| S1 moderate | `1.0762×` |
| S2 stress | `0.7845×` |
| front hit | `0.3118×` |
| grazing hit | `0.9575×` |
| near miss | `0.9027×` |
| oblique hit | `1.7789×` |

The predeclared continuation gate required `≤0.80×` on held-out ordinary, `≤0.90×` on S1 and S2, and no ordinary family above `1.00×`. It fails. The full on-device separator segmenter was not implemented. This benchmark merged cells lexicographically and enumerated all hits; it must be described as unordered candidate-set throughput, not closest-hit DDA performance.

## Interpretation for the draft

The count result was not meaningless: stress-shell and front-facing rays show that direct topology-certified traversal has real favorable regimes. The general performance thesis fails because moderate and oblique rays do not provide enough nonlinear-quadtree work to amortize the candidate's leaf/survivor and per-ray costs. On held-out ordinary rays the optimistic margin is only 4.4%, before paying for segmentation.

Safe wording:

> Our separator-sign construction preserves exact DDA topology without solving grid-event roots. A GPU replay of its certified leaf schedule is faster for stress-shell and front-facing regimes, but reaches only `0.956×` the same-surface nonlinear quadtree on held-out ordinary rays before charging separator construction. We therefore report the topology result and regime analysis without claiming broad traversal acceleration.

Do not write that the implemented full method is 4.4% faster: the timed envelope excludes segmentation/certification, discards DDA order, and is not the end-to-end closest-hit algorithm. Do not compare these numbers to TFDM, RMIP, or published Ogaki timings. The ordered closest-hit question is tracked separately in [[Plan — T1.6 ordered closest-hit shell DDA]].

## Authoritative artifacts

- [[Plan — T1.5 measured separator-DDA performance]]
- `experiments/ray_separator_t15/ray-t15-envelope-f619b4ad5265/result.json`
- `experiments/ray_separator_t15/ray-t15-envelope-f619b4ad5265/summary.json`
- `experiments/ray_separator_t15/ray-t15-envelope-f619b4ad5265/schedule_summary.json`
- `ray_a1_math_probe/ray_t15_envelope_probe.cu`
- `scripts/ray_separator_t15_envelope.py`
