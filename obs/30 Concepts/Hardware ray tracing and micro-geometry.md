---
title: Hardware ray tracing and micro-geometry
tags: [concept, hardware, bvh, rt-cores, performance]
---

# Hardware ray tracing and micro-geometry

Why the shape of the hardware determines the shape of every algorithm in this vault.

## The three things hardware does

1. **BVH traversal** — fixed-function descent through the acceleration structure.
2. **Ray–triangle intersection** — fixed-function, at the leaves.
3. **Everything else** — intersection shaders, any-hit shaders: *programmable*, and therefore an exit from the fast path.

Every method here is defined by which of those three it can use.

## The intersection-shader tax

A custom primitive is traced by supplying AABBs and an **intersection shader**. That shader runs on general compute, not the RT core. [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] names this as the main reason for its performance gap:

> Our current method is unable to take full advantage of this hardware as the D-BVH traversal uses an intersection shader which operates on generic compute shaders. This partially explains the large performance gap we see relative to pre-tessellation methods. A future extension would be to extend modern hardware to directly support displacement mapping.

That wish was granted by [[NVIDIA 2022 — Ada Lovelace Architecture|Ada]] and then withdrawn — see [[Graphics API and hardware support timeline]].

The equivalent tax on the opacity side is the [[Any-hit shader cost|any-hit shader]], which interrupts traversal rather than replacing it.

Later methods reduce the tax by pushing more work into the hardware BVH: [[Thonat et al. 2023 — RMIP|RMIP]] describes itself as "orthogonal to both the hardware TLAS/BLAS traversal and the final ray-surface intersection test", and [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] puts prisms in a hardware TRBVH and rides it down to the prism before marching.

## BVH cost is the real currency

Micro-geometry's value is not primarily fewer bytes for the geometry — it is a **smaller, cheaper acceleration structure**, because the BVH covers coarse base primitives rather than the displaced detail.

| Measurement | Source |
|---|---|
| BVH build time **÷4**, BVH size **÷6** (median), tracing **1.3× slower** | [[Maggiordomo et al. 2023 — Micro-Mesh Construction\|µ-meshes]] on an RTX 4090 |
| 7–15× build time, 5–20× size savings | vendor claims, [[NVIDIA 2022 — Ada Lovelace Architecture\|Ada whitepaper]] |
| Triangle leaf data **6–10× smaller**, total BVH **50% smaller** | [[Barczak et al. 2024 — DGF\|DGF]] |
| Build/refit proportional to base-mesh triangle count; refit often sufficient | [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes\|animated DMM]] |
| BLAS update **0.082 ms vs 35 ms** for explicit adaptive tessellation | [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)\|TFDM]] |

Note the recurring pattern: **compression buys build time and memory, and costs tracing time.** µ-meshes trace 1.3× slower; DGF decoding costs up to 2.4× more time while cutting L2 misses 40–50%; animated DMMs cost 2.3–2.5× static ones. The bet in every case is that the working set shrinks enough to free cache for BVH nodes.

## Hardware shapes the format

Concrete instances of hardware constraints appearing directly in a data structure:

- **DGF blocks are 128 bytes** because that is the target's cache line, and the x/y/z bit widths must sum to a multiple of 4 so a hardware mux can decode them.
- DGF measures a **quad rate** (>90%) because several architectures natively intersect *pairs* of edge-adjacent triangles.
- [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|CSM]] uses **16× fan-out** rather than 4 specifically to halve the number of *dependent* memory reads — latency, not bandwidth, is the constraint.
- The µ-mesh **prismoid** exists so the format is *self-bounding*: the BVH leaf and the bounding volume are the same object.

## Where the hardware went

The two hardware bets of [[NVIDIA 2022 — Ada Lovelace Architecture|Ada]] diverged completely. The **Opacity Micromap Engine** became a ratified cross-vendor standard. The **Displaced Micro-Mesh Engine** was withdrawn in favour of **cluster acceleration structures** — Blackwell's RT core upgraded its Triangle Intersection engine to a *Triangle Cluster Intersection Engine* with cluster compression.

The lesson the papers were converging on independently: [[Barczak et al. 2024 — DGF|DGF]] and [[Karis et al. 2021 — Nanite|Nanite]] both bet on **clusters of arbitrary triangles**, not displaced height fields, and that is the bet that won. Scalar displacement over a base triangle is too restrictive a topology for general content — see [[Base mesh quality objectives]].
