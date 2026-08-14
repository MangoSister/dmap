---
title: Tessellation-free vs pre-tessellation
tags: [concept, trade-off, ray-tracing, displacement]
---

# Tessellation-free vs pre-tessellation

The central trade-off of this vault's displacement cluster, and the axis along which every method here positions itself.

## The two ends

**Pre-tessellation.** Subdivide the base mesh to displacement-map resolution, displace the vertices, build a static BVH over the result. Full use of fixed-function ray tracing, best raw throughput — and a memory footprint that caps achievable fidelity, plus a rebuild measured in *seconds* whenever anything changes.

**Tessellation-free.** Keep the displacement in its native form and generate bounding volumes and intersections on the fly per ray. Tiny memory, instant edits, free tiling and instancing — and traversal that runs in an intersection shader on general compute rather than on RT cores.

## What the numbers actually say

Every paper here concedes pre-tessellation wins on raw speed. The honest framings:

[[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] is roughly **two orders of magnitude slower** at run time, while using **10–90× less memory** and updating **four orders of magnitude faster** (0.082 ms vs 35 ms per-frame BLAS update against adaptive tessellation). Its real argument is quality *at a fixed memory budget*: pre-tessellation at 125 MB, 512 MB and 2.0 GB all look worse than TFDM at 34 MB.

[[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] reaches **20–30% of pre-tessellated speed** at ~1/60 the memory — and *beats* it outright on fur (0.7 MB vs 0.7 GB, twice as fast), because pre-tessellated fur produces long thin world-space AABBs while texture-space boxes behave like oriented boxes.

[[Thonat et al. 2023 — RMIP|RMIP]] states the boundary condition most plainly:

> When using displacement maps with moderate resolution or limited tiling, and keeping the displacement content **static**, then combining pre-tessellation, pre-displacement, and hardware ray tracing remains the method of choice.

So the tessellation-free case rests on one or more of: **memory pressure**, **tiling and instancing**, **dynamic content**, or **interactive editing**. Absent all four, pre-tessellate.

## The third option: make the bound part of the format

[[Maggiordomo et al. 2023 — Micro-Mesh Construction|Micro-meshes]] refuse the dichotomy. Geometry is compressed into a **self-bounding** representation — a prismoid per base triangle — so the BVH is built over base triangles only, and micro-triangles are generated on demand *by hardware* during traversal. Measured: **BVH size ÷6, build time ÷4**, tracing a median 1.3× slower than the equivalent uncompressed mesh.

That is pre-tessellation's hardware utilisation with something close to tessellation-free memory. It is why half this vault orients itself around DMM — and why its withdrawal matters. See [[Graphics API and hardware support timeline]].

## Where the vault's methods sit

| Method | Approach | Uses hardware BVH? |
|---|---|---|
| [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)\|TFDM]] | texture-space quadtree + affine bounds | only for one AABB per base triangle |
| [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping\|Ogaki]] | nonlinear rays in texture space | standard BVH over prisms (CPU) |
| [[Thonat et al. 2023 — RMIP\|RMIP]] | rectangular RMQ + 2D↔3D ping-pong | yes, down to the prism; "orthogonal" to it |
| [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)\|PDM]] | direct marching, no hierarchy at all | yes, prisms in a hardware TRBVH |
| [[Maggiordomo et al. 2023 — Micro-Mesh Construction\|µ-mesh]] | compressed self-bounding format | fully — the format *is* the BVH leaf |
| [[Barczak et al. 2024 — DGF\|DGF]] | block-compressed arbitrary topology | designed for future fixed-function decode |

Note the drift over time: the later the paper, the more of the hardware BVH it uses. [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] abandons the acceleration hierarchy entirely for **direct sampling** — a fixed 108 bytes per base triangle, **zero rebuild** on edits — which is the tessellation-free argument taken to its limit for the interactive-editing use case.

## An unhelpful comparison to avoid

These papers do not form a single ranking. TFDM measured on an RTX 2080 Max-Q; RMIP on an RTX 3080 against TFDM; PDM on an RTX 4090 against RMIP; Ogaki on a CPU. Speedup claims chain through different hardware and scene sets. See [[MOC — Ray Traced Displacement Mapping]].
