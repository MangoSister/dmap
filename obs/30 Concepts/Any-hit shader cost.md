---
title: Any-hit shader cost
tags: [concept, opacity, alpha-test, performance]
---

# Any-hit shader cost

The bottleneck that the entire opacity cluster of this vault exists to remove.

## The problem

BVH traversal and ray–triangle intersection are **fixed-function** on modern GPUs. Alpha-tested geometry breaks that: every potential hit must invoke an **any-hit shader** that fetches indices and texture coordinates, samples an alpha texture, and decides whether the hit counts.

The cost is not the arithmetic. It is that **any-hit execution interrupts fixed-function traversal** and hands control to the programmable units — a point [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]] make explicitly, and the reason their proposal was to put the data *inside the BVH*.

[[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] put it more bluntly: any-hit "sits in the innermost loop of the ray-tracing pipeline and is the major bottleneck for alpha-masked geometry", and is why ray tracing has struggled to beat rasterisation for transparency in practice.

## How large

- **~1.6×** — [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]]'s measurement of fully-opaque tracing against alpha-tested tracing, 2020, before any hardware support.
- **17% → 3%** — the share of periodic samples spent in any-hit shaders in *Indiana Jones and the Great Circle* after enabling opacity micromaps, with `TraceMain` falling **7.90 → 3.58 ms (−55%)**.
- **up to 2.3×** — Microsoft's claimed uplift for OMM in path-traced scenes.
- **~⅓** — the ray-tracing cost reduction Remedy reported for *Alan Wake 2* using OMM and Shader Execution Reordering together.

## The fix

Classify sub-triangle regions as provably fully opaque or fully transparent ahead of time, so traversal can accept or reject without a shader call. That is the [[Micromap]].

The chain is unusually clean: [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al. 2020]] proposed it and prototyped it in software; [[NVIDIA 2022 — Ada Lovelace Architecture|Ada]] built the fixed-function engine; Khronos and Microsoft standardised it; games shipped it. See [[Graphics API and hardware support timeline]].

## Two lessons the papers teach

**Skip rate is not the metric.** [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]] reach a **93% skip rate** on their worst-case texture at mip level 4 — and gain **1–2%**. The reason is that those textures have almost no *fully opaque* sub-triangles (0.64%), so rays never terminate early. What matters is early ray termination, not avoided shader calls.

**Software emulation can lose outright.** [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] measure software micromap emulation at anywhere from **−16% to +30%** frametime. Native micromaps range −29% to +2%. Doing the classification is only worth it if the traversal hardware consumes it.

That second point is why [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] measure their compressed lookup against an *alpha texture lookup* rather than against hardware — matching the texture path at 10 subdivision levels is the realistic bar for a software method, and hardware OMMs still beat all software variants.
