---
title: Tessellation-Free Displacement Mapping for Ray Tracing
authors: [Théo Thonat, François Beaune, Xin Sun, Nathan Carr, Tamy Boubekeur]
affiliation: Adobe
year: 2021
venue: ACM TOG 40(6), Article 282 — SIGGRAPH Asia 2021
doi: 10.1145/3478513.3480535
tags: [paper, displacement, ray-tracing, tessellation-free, affine-arithmetic]
status: read
---

# Tessellation-Free Displacement Mapping for Ray Tracing (TFDM)

The **common ancestor of this vault's displacement cluster** — every later ray-traced displacement paper here ([[Thonat et al. 2023 — RMIP|RMIP]], [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]], [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]]) uses it as the baseline to beat.

## Problem

Ray tracing a displaced surface conventionally means **pre-tessellating the base mesh to displacement-map resolution** before a static BVH can be built. Memory and build cost then cap achievable fidelity, and tiled high-resolution displacement becomes intractable for physically-based rendering. TFDM wants displacement to stay in its **native 2D map form** at trace time, so tiling, instancing, rescaling and interactive editing are all free.

The key insight is to **decouple the acceleration structure from the base domain**: define it in texture space and map it onto meshes exactly like a texture. See [[Tessellation-free vs pre-tessellation]].

## Core method

The displaced surface is `S(u,v) = P(u,v) + h(u,v) N̂(u,v)`, with `P` and `N` linearly interpolated per base triangle.

**D-BVH (Displacement BVH).** A 2D hierarchical structure in *texture* space that generates 3D bounding volumes **on the fly, per ray**. The BVH over the displaced surface is never stored.

- **Min-max mipmap** — a 2-channel pyramid `M^k_{i,j} = [min h, max h]` over each texel's uv domain. It is independent of the base surface, which is exactly what makes it instanceable and tileable. See [[Min-max mipmap and conservative bounds]].
- **Adjusted min-max mipmap** — the plain recursion is *not* conservative once heights are bilinearly or B-spline interpolated at coarser levels, so the recursion is modified (Eq. 4) to fold in per-level sampling.
- **Implicit traversal graph** — a node is just two integer texel coordinates plus an integer mip level; children are the four texels one level down. Traversal is **stackless and pointerless** depth-first via `down`/`up`/`next`. Because mip levels and texel coordinates may be *signed*, the graph is infinite — which is how texture coordinates outside `[0,1]²` (tiling) fall out for free.
- **Conservative boxes via affine arithmetic** — at each traversed texel, affine forms for `uv` (exact, since the domain is an axis-aligned box), for `h` (one min-max fetch), and for interpolated `P` and `N̂` are combined through Eq. 1 to yield an AABB. Affine arithmetic rather than interval arithmetic because correlated error terms cancel instead of accumulating. `clamp` is linearised with Chebyshev, `1/√x` with Min-Range. Boxes are built in a per-triangle **uv-aligned tangent space** to stay tight.
- **Leaf intersection**, selectable per instance: leaf box (piecewise-constant normals, good for shadow rays), on-the-fly **local triangulation** into two micro-triangles per texel — an *implicit per-ray adaptive tessellation* — or bilinear / bicubic B-spline height sampling solved by Newton iteration.
- **Level of detail** — integer LoD stops traversal above a target mip level. Because `S` is linear in `h`, fractional LoD is just a blend of two consecutive integer-LoD surfaces, implemented by fetching the parent texel.

**GPU realisation.** Vulkan Ray Tracing. The displaced object is a *custom geometry* whose BLAS is one object-space AABB per base triangle (computed in a compute shader in **< 0.3 ms** for all test scenes); a **custom intersection shader** performs the whole D-BVH traversal. Closest-hit and any-hit shaders are untouched.

**Traversal optimisations,** in order of importance: **texel discard** (skip texels outside the triangle's uv domain, via a bespoke triangle–square 2D collision test), **multiple roots** (up to four sibling texels instead of one large ancestor), and ray-direction-sorted traversal order.

## Results

Laptop RTX 2080 Max-Q. Against uniform pre-tessellation given the full 8 GB:

| | Pre-tessellation | TFDM |
|---|---|---|
| Memory | 0.5–3.1 GB | **34–164 MB** (10–90× less) |
| Ray throughput | 20–133 Mray/s | 0.7–4.8 Mray/s (~2 orders of magnitude slower) |
| Update after edit | 0.6–4.6 s | **0.040–0.265 ms** (~4 orders of magnitude faster) |

The paper is candid that pre-tessellation is far faster at run time; TFDM wins on memory, on edit latency, and on **quality at any fixed memory budget** — tessellation at 125 MB, 512 MB and 2.0 GB all look worse than TFDM at 34 MB. Against explicit adaptive tessellation, per-frame BLAS update is **0.082 ms vs 35 ms**.

Ablation: texel discard is the single biggest win (disabling it costs ×0.06–×0.43). Traversal, not leaf intersection, dominates cost.

## Limitations

- **No watertightness guarantee across UV chart boundaries.** Seam-erasure preprocessing doesn't survive tiling or map sharing and is too slow for interactive design; the authors call general seam-free displacement "an open and challenging problem". See [[Watertightness and cracks]].
- **Cannot use fixed-function RT cores** — the intersection shader runs on generic compute, which the authors name as the main reason for the performance gap. They explicitly wish for hardware displacement support; [[NVIDIA 2022 — Ada Lovelace Architecture]] delivered exactly that a year later. See [[Hardware ray tracing and micro-geometry]].
- No vector displacement; no formal watertightness proof under ray-dependent LoD.

## Relation to other work

Predates micro-meshes entirely, so it cites no [[Micromap]] work. It builds directly on [[Tevs et al. 2008 — Maximum Mipmaps]] (generalising from planar height fields to arbitrary polygon base meshes), [[Smits et al. 2000 — Direct Ray Tracing of Displacement Mapped Triangles]] and [[Porumbescu et al. 2005 — Shell Maps]].

Downstream: [[Thonat et al. 2023 — RMIP|RMIP]] is by the same lead author and supersedes it (×5 average, ×11 best) by fixing three named weaknesses — the fixed tree structure, the isotropic square nodes, and the binary yes/no pruning. [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] is a *parallel* 2023 answer from a different angle and additionally handles base triangles that are **degenerate in uv space**, which TFDM cannot render at all. [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] cites it as costly per pixel.

![[TFDM_lowres.pdf]]
