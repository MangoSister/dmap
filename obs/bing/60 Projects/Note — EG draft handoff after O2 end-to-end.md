---
title: Note — EG draft handoff after O2 isolated-query timing
tags: [paper, eurographics, ray-tracing, negative-result, handoff]
status: current
created: 2026-08-30
updated: 2026-08-30
---

# EG draft handoff after O2 isolated-query timing

## What is safe to write now

We developed a tessellation-free displaced-triangle query architecture in shell space. A world ray maps to a rational shell curve. The method constructs supported monotone height intervals, adaptively certifies piecewise chord topology, groups closed grid-edge/corner events, contracts isolated single-boundary events with outward interval Newton, traverses the resulting leaf-cell stream, tests the exact represented microtriangles, and terminates only when conservative suffix world-ray bounds exclude every later cell.

Correctness is strong on the frozen 864-ray practical corpus. The fused device path starts only from packed proxy/ray data and displacement windows. It has zero fallback, agrees with the packed exhaustive oracle and ordered NRT-QT checksum, preserves canonical seam/corner owners, and reports hit-coordinate differences below `3.70e-13`. Certified closest-hit termination stops 491 rays and skips 2,422/5,888 cells without changing any output.

The strongest positive mechanism result is conditional: with a CPU-precomputed contracted schedule, ordered DDA traversal is `0.5068x` NRT-QT on held-out ordinary rays. This supports the claim that linear segment order, contracted grid events, and exact leaf testing can make traversal efficient once the cell stream is known.

## What must not be claimed

The complete isolated candidate-triangle GPU query is not faster than the fair ordered NRT-QT query baseline. On 32,768 held-out ordinary records and 50 paired trials, `FULL/NRT = 1.8852x`; S1/S2 are `1.9067x/1.8538x`, and oblique/grazing families are `2.2417x/2.7783x`. Every checksum and timing-stability gate passes, so this is a query-kernel performance conclusion rather than a correctness bug.

Do not write that our ray tracer outperforms NRT-QT, Ogaki, RMIP, TFDM, or all baselines. Do not use the optimistic PRE-IN1 timing as complete-query or renderer performance. O2.4d excludes scene BVH traversal, multi-proxy behavior, OptiX ray-family scheduling, shading, and complete frames, so it must not be labeled renderer wall time or FPS. Renderer integration is planned in [[Plan — R1 full-renderer validation of ordered shell DDA]].

## Diagnosed cause

The raw-to-schedule stage dominates: held-out `DEV-EVENT/NRT = 1.4274x`. Fused FULL uses `22,656` stack bytes/thread versus `1,360` for NRT. However, stack layout is not the sole cause. An exact two-pass global schedule is 1.6–7.1% slower than fused FULL across the required cohorts. A certified direct-one-segment hybrid accepts 87.5% of held-out ordinary rays but is 38.0% slower than FULL. These negative ablations rule out simple schedule relocation and duplicated one-segment certification as sufficient fixes.

## Recommended paper positioning

Until R1 completes, treat ray tracing as an extrinsic conservative-query demonstration and correctness contribution, not the performance anchor. If space is tight, present the current result as an isolated-query architecture study or limitation: conservative topology and exact ownership are achieved, while fully general on-device event construction is more expensive than nonlinear min/max traversal on the measured replay corpus. Do not infer the sign of the whole-renderer result yet.

Primary artifacts:

- O2.4c correctness: `ray-o24c-full-e74578e59bd5`
- O2.4d timing: `ray-o24d-timing-f3235f8d772d`
- O3.1 global schedule: `ray-o24d-timing-a3ee67be022e`
- O3.2 direct-one: `ray-o24d-timing-8f4336c2b29e`
- full derivation and decision log: [[Plan — O2 on-device ordered shell DDA]]
