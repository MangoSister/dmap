---
title: Note — EG draft handoff after ordered closest-hit O0
tags: [paper, eurographics, handoff, ray-tracing, DDA, closest-hit]
status: current
created: 2026-08-30
updated: 2026-08-30
---

# EG draft handoff after ordered closest-hit O0

## Correction to T1.5

T1.5 measured a globally merged, lexicographically sorted leaf set and enumerated every hit. It did not use the certified front-to-back DDA order or closest-hit termination. O0 corrects that query contract on the CPU before another CUDA port.

All 864 practical rays have a single accepted interval with monotone world-ray parameter. O0 emits closed event-owner batches in certified order and stops only when the conservative lower world-$t$ bound of every unprocessed cell is strictly behind the current closest hit. All closest hits, owner sets, reverse streams, and miss-ray behaviors match the exact oracle.

## Quantitative result

For 144 held-out ordinary hit rays:

| schedule | exact leaf-triangle tests | ratio to no stop |
|---|---:|---:|
| no early stop | `592` | `1.000×` |
| ideal exact event spans | `320` | `0.5405×` |
| current separator spans | `550` | `0.9291×` |

The exact topology order has large potential, but current event brackets preserve only `15.4%` of the ideal early-exit saving. Separator overlap adds a median two and p95 four leaf-triangle tests relative to the ideal schedule. Current family ratios are front `1.000×`, grazing `0.845×`, near-miss `0.949×`, and oblique `1.000×`.

## Paper-safe interpretation

> Topology-preserving chords establish a correct front-to-back cell order. An exact-event oracle shows that this order could remove 45.9% of held-out closest-hit leaf work, but the current root-free separator brackets remove only 7.1%. The limiting factor is conservative event-location width, not DDA order itself.

Do not cite T1.5 as ordered-DDA performance. Do not claim that current separator DDA is faster. It is safe to motivate an event-local bracket contraction ablation from the measured gap between `0.5405×` ideal and `0.9291×` current work.

## Authoritative artifacts

- [[Plan — T1.6 ordered closest-hit shell DDA]]
- `experiments/ray_ordered_closest_o0/ray-ordered-o0-972a8b4bd571/result.json`
- `experiments/ray_ordered_closest_o0/ray-ordered-o0-972a8b4bd571/summary.json`
- `experiments/ray_ordered_closest_o0/ray-ordered-o0-972a8b4bd571/traces.jsonl.gz`
- `scripts/ray_ordered_closest_o0.py`
