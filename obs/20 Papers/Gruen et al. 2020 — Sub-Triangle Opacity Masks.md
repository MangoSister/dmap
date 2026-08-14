---
title: Sub-triangle opacity masks for faster ray tracing of transparent objects
authors: [Holger Gruen, Carsten Benthin, Sven Woop]
affiliation: Intel
year: 2020
venue: PACMCGIT 3(2), Article 18 — HPG 2020
doi: 10.1145/3406180
tags: [paper, opacity, any-hit, alpha-test, micromap-origin]
status: read
---

# Sub-triangle Opacity Masks for Faster Ray Tracing of Transparent Objects

**The origin of the micromap.** [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] credit it directly: "The concept of micromaps were first presented by [Gruen et al. 2020]." Everything shipped since — the Ada opacity engine, `VK_EXT_opacity_micromap`, DXR 1.2 — is a descendant of this paper's proposal.

## Problem

BVH traversal and ray–triangle intersection are hardware-accelerated, but *transparent* surfaces force a programmable-shader excursion: every potential hit on alpha-tested geometry invokes an **any-hit shader** that fetches indices and texcoords and samples an alpha texture. The authors measure fully-opaque tracing as **~1.6× faster** than alpha-tested tracing.

The goal: skip the alpha test entirely for sub-triangle regions that are provably fully opaque or fully transparent, **without increasing geometric complexity** — and therefore without raising animation, simulation or BVH-refit cost. See [[Any-hit shader cost]].

## Core method

**Barycentric subdivision.** Each alpha-tested triangle's barycentric space is subdivided into **N×N uniform sub-triangles**, indexed `0…N²−1`. This is a **single, non-recursive level**, laid out like the control points of a degree-N Bézier triangle. No sub-triangle geometry is ever created. At N=8 that is 64 sub-triangles in **16 bytes** per partially-transparent triangle.

**STOC bits** — Sub-Triangle Opacity Control, **2 bits per sub-triangle**:

| Bits | Meaning |
|---|---|
| bit 0 set | fully **opaque** → any-hit trivially accepts |
| bit 1 set | fully **transparent** → any-hit trivially rejects |
| neither | mixed → do the real texture lookup |
| both | undefined |

A 3-state encoding in 2 bits — the direct ancestor of the shipped 4-state OMM, which spends the wasted state on distinguishing *unknown-opaque* from *unknown-transparent*. See [[Micromap]].

**Baking by conservative rasterisation**, offline or at level load, on the GPU, many triangles in parallel: enable highest-tier conservative rasterisation so no relevant alpha texel is missed; draw N×N instances of the mesh with a viewport large enough for a 1:1 pixel↔texel mapping; a geometry shader shrinks each input triangle to the sub-triangle selected by `SV_InstanceID` and moves it to the viewport origin; the pixel shader writes no render target but issues **interlocked binary OR**s into a STOC UAV. The shaders naturally compute the *negations* of the bits, so an extra pass inverts them. The shader samples several mip levels so the bits stay conservative with respect to whatever mip the runtime alpha test will access.

**Runtime.** One STOC buffer **per BLAS**, shared across all instances of that mesh, addressed from subdivision level, primitive ID and geometry ID. All sub-triangles of a triangle are contiguous, so one load fetches the current bits and lands neighbours in the same cache line. Works in DXR 1.0 any-hit shaders and inside a DXR 1.1 `RayQuery` loop.

**Alternative: pre-tessellation.** Use the bits in a compute shader to cull fully transparent sub-triangles at tessellation time and feed only survivors to the BVH builder — trading memory and refit cost for traversal efficiency.

## Results

RTX 2080 Ti, 54 copies of a ~300K-triangle chestnut tree, ~17M triangles, four foliage textures (two deliberately bad worst cases).

- **Up to 86% of all transparency tests skipped**, up to **40% speedup**, in a software-only DirectX proof of concept.
- Translucent shadow rays: 5.9–42.2% faster. Ambient occlusion (incoherent) rays: 4.6–29.7% faster — incoherence only slightly diminishes the benefit.
- **Memory:** 1.1 MB at N=4, 4.6 MB at N=8 per tree — constant in texture content, dependent only on triangle count and level. Pre-tessellation instead costs **344.7 MB** and an ~18× triangle-count increase, with BVH refit at 3.4 ms vs 0.2 ms.
- **Where it fails:** at mip level 4 the skip rate reaches 93% but the speedup collapses to 1–2% for the badly-authored textures, because they have almost no fully-opaque sub-triangles (0.64%) so rays never terminate early. Skip rate is not the metric that matters — early termination is.

## Limitations and the prediction that came true

The future-work section is effectively a specification of what NVIDIA's Ada opacity engine shipped two years later:

> Future ray tracing hardware implementations could store STOC bits **directly in the BVH**, together with triangle shading data of transparent geometry. The STOC bits would allow the hardware to skip all `anyHit` shader calls for all intersections corresponding to fully transparent or fully opaque sub-triangles.

The motivation given is precisely that any-hit execution **interrupts fixed-function traversal**. They also note the 2-bit encoding wastes a state and could be compressed below 2 bits per sub-triangle — a thread [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] pick up.

## Relation to other work

Cites nothing else in this vault — at July 2020 it predates all of it. It is the upstream node of the opacity cluster. One detail worth carrying: its indexing order **differs** from the Vulkan/DXR one that followed, including the rounding pattern at edges and corners.

The same first two authors reappear in 2026 at NVIDIA and AMD respectively — Gruen on "Ray Tracing Massive Amounts of Animated Geometry" (HPG 2026), Benthin on [[Barczak et al. 2024 — DGF|DGF]].

![[Sub-triangle_opacity_masks.pdf]]
