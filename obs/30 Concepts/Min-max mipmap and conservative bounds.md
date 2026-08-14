---
title: Min-max mipmap and conservative bounds
tags: [concept, acceleration-structure, bounding, texture-space]
---

# Min-max mipmap and conservative bounds

The shared acceleration idea behind every tessellation-free displacement method here: **store conservative height bounds over regions of the displacement map, and use them to generate 3D bounding volumes on the fly rather than storing a BVH over the displaced surface.**

## The base structure

A **min-max mipmap** is a 2-channel pyramid where each texel at level `k` holds `[min h, max h]` over its uv domain, built by recursion from the level below. Its ancestor is [[Tevs et al. 2008 — Maximum Mipmaps]], which used maximum mipmaps for empty-space skipping over *planar* height fields.

Two properties make it powerful in [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]]:

- It is **independent of the base surface**, so it can be instanced, tiled and shared across meshes exactly like a texture.
- It defines an **implicit traversal graph** — a node is two integer texel coordinates plus a mip level, children are the four texels one level down. No pointers, no stack, and because the indices may be *signed*, the graph is infinite, which is how tiling falls out for free.

One subtlety: the plain recursion is **not conservative** once heights are bilinearly or B-spline interpolated at coarser levels. TFDM's *adjusted* min-max mipmap folds per-level sampling into the recursion to fix this.

## From 2D bounds to a 3D box

Knowing `[min h, max h]` over a uv region is not the same as knowing the displaced surface's extent in space, because the surface is `P(u,v) + h·N̂(u,v)` with `P` and `N̂` both varying. Two answers appear here:

**Affine arithmetic** ([[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]]). Represent each quantity as an affine combination of symbolic variables in `[−1,1]`, propagate through the surface equation, and extract an AABB. Affine rather than interval arithmetic **because correlated error terms cancel instead of accumulating**. Non-linear operators are linearised with Chebyshev (`clamp`) or Min-Range (`1/√x`). Boxes are built in a per-triangle uv-aligned tangent space to stay tight.

**Avoid 3D bounds entirely** ([[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]]). If the ray is expressed in *texture* space as a rational curve, you can intersect texture-space AABBs directly — four quadratics instead of a slab test, but no conservative padding, and crucially **texture-space AABBs do not overlap at all** while shell-space boxes do.

## The anisotropy problem

[[Thonat et al. 2023 — RMIP|RMIP]] identifies the min-max mipmap's structural weakness: **a ray segment's texture-space footprint is generally anisotropic, but quadtree texels bound only square regions**. Its answer, the **RMIP**, supplies min-max bounds over *arbitrary axis-aligned rectangles*, framed as a 2D range minimum query and decomposed into four power-of-two sub-queries. A compression scheme brings storage down to `N²` — the order of a classical mipmap — while the texture layout lets hardware trilinear sampling deliver fractional level of detail.

The measured payoff: swapping RMIP for a standard min-max mipmap at equal memory **degrades performance**, and RMIP needs on average an **order of magnitude fewer traversal iterations** than TFDM.

## Where hierarchies stop paying

Both RMIP and [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] eventually stop subdividing and **march**. RMIP switches to texel marching once the 2D bound is small enough, for two reasons: its rectangles are not texel-aligned so a texel is typically covered by several of them, and its finest resolution may be coarser than the displacement map, past which bounds stop tightening. PDM skips hierarchy entirely in favour of direct sampling — trading traversal cost for a fixed 108 bytes per base triangle and **zero rebuild** on edits.

The hardware equivalent of all this is the prismoid: [[Maggiordomo et al. 2023 — Micro-Mesh Construction|µ-meshes]] make the bound *part of the format*, self-bounding per base triangle, so the BVH covers only base triangles. See [[Shell, prism and prismoid]] and [[Hardware ray tracing and micro-geometry]].

## What is never bounded

Every structure on this page bounds `h`. **None bounds `∇h`** — verified against Munkberg et al. 2010, which bounds base-patch normals and min-max `h` but not the displacement gradient. Bounding `∇h` alongside `h` yields conservative bounds on the *metric* of the displaced surface rather than merely its extent — and hence on area, Laplace–Beltrami, and anisotropy. See [[The induced metric of a displaced surface]].

One warning on the mechanism: the obvious move — two more independent min-max channels for `h_u`, `h_v` — **does not work** for the metric. Independent intervals lose the correlation between `h` and its own gradient, and let determinant bounds draw the same gradient twice at worst case. The structure that keeps the correlation is a joint plane-plus-remainder node per texel region: the [[Taylor-model bound pyramid]], which also subsumes the min-max height channel and can tighten the existing ray traversal via slab bounds.
