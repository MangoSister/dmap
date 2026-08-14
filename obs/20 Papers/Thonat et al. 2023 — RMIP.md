---
title: "RMIP: Displacement ray-tracing via inversion and oblong bounding"
authors: [Théo Thonat, Iliyan Georgiev, François Beaune, Tamy Boubekeur]
affiliation: Adobe
year: 2023
venue: SIGGRAPH Asia 2023 Conference Papers
doi: 10.1145/3610548.3618182
tags: [paper, displacement, ray-tracing, tessellation-free, range-minimum-query]
status: read
---

# RMIP: Displacement ray-tracing via inversion and oblong bounding

The direct successor to [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]], by the same lead author, and the strongest tessellation-free method in this vault by measured margin.

## Problem

TFDM is accurate but slow, and the paper names exactly why:

1. its tree structure is **fixed** — it doesn't adapt to the displacement data;
2. the texture-space footprint of a ray segment is generally **anisotropic**, but quadtree nodes bound only square regions;
3. pruning uses a **binary** yes/no answer from loose 3D box tests.

RMIP addresses all three.

## Core method

**Central idea: ping-pong between 2D and 3D.** Track ray-interval bounds simultaneously in texture space and object space. Subdividing a 2D uv bound tightens the scalar displacement bounds and hence the 3D surface bounds; intersecting those 3D bounds with the ray shrinks the valid ray interval, whose projection back into texture space shrinks the 2D footprint. A bidirectional communication channel between the two spaces.

This extends [[Patterson et al. 1991 — Inverse Displacement Mapping]] — previously limited to analytic base surfaces — to triangle meshes with interpolated displacement directions.

**Point-wise displacement inversion.** Projecting a 3D point back to texture space means solving `(P(u,v) − (O+tD)) × N(u,v) = 0`. An analytic cubic inverse exists, but the authors found **iterative Newton inversion faster and more stable**; a few iterations suffice. Crucially, the inverse isn't defined everywhere — but a **bounding prism is fully contained in the convergence region**, so every projection from inside a prism is guaranteed to land inside the base triangle. This is one of three jobs the prism does (see [[Shell, prism and prismoid]]); it also subdivides space (prisms of neighbouring triangles don't overlap in 3D, unlike AABBs) and supplies the initial traversal interval.

**Implicit ray projection.** Removing the `t` dependence gives the projected ray as a **quadric in texture space**, `ψ(u,v) = det(P(u,v) − O, N(u,v), D) = 0`. Each of `ψ_u = 0` and `ψ_v = 0` is a line; intersecting them with the projected ray gives the **turning points** where the curve's principal direction flips — at most four, in practice never more than one. Between turning points the sub-interval is monotone and therefore exactly bounded by the rectangle spanned by its endpoints.

**Two tightening mechanisms.** *Ray-interval projection*: after each 3D ray–box test, invert at the interval endpoints to get a much tighter 2D rectangle — rays near-parallel to the displacement directions project to something smaller than a texel, making traversal effectively constant-time. *Bound subdivision*: guaranteed shrinkage by splitting the 2D domain directly along the bisector of the rectangle's **longer** side. Patterson's original split projected the 3D interval midpoint, which lands anywhere along the curve and yields unbalanced splits.

**The RMIP structure.** A min-max mipmap gives excessively loose bounds for anisotropic queries, so RMIP supplies min-max bounds over **arbitrary axis-aligned rectangles** — the "oblong bounding" of the title. Framed as the 2D **range minimum query** problem: following Amir et al. [2007], any 2D range query decomposes into four possibly-overlapping power-of-two sub-queries. Naively this costs `N²(1+log₂N)²` values; a packing into a multi-layer, multi-level texture plus a compression scheme reduces storage to **`N²` — the order of a classical mipmap** — while the layer layout lets hardware trilinear sampling deliver **fractional LoD**, and queries may wrap the unit square for tiling. Contrast with [[Min-max mipmap and conservative bounds]].

**Texel marching.** Once the 2D bound is small enough (a tunable *marching scale*), stop subdividing and march along the projected ray through the texels it crosses; the sign of `ψ` at each texel corner tells which edge the ray exits. Justified because 2D bounds are arbitrary rectangles, not texel-aligned, so a texel is typically covered by several rectangles — marching amortises the redundancy.

**Front-to-back traversal** with children pushed back-then-front, so RMIP can **terminate on first hit** — explicitly contrasted with TFDM, which cannot.

## Results

16-core Ryzen 9 5950X + RTX 3080. Against TFDM at equal quality: **×5 speedup on average (σ = ×3)**, up to **×11** on the desert-tire teaser while using **3× less memory**. On average **an order of magnitude fewer traversal iterations** than TFDM.

The speedup **grows** with displacement scaling (to ~×14) and with tiling (to ~×14), confirming better asymptotic traversal complexity. RMIP rebuild after a map edit is ≈ **5 ms for a 4k map**; changing displacement *parameters* only recomputes prisms, **< 1 ms**.

Notable honest ablation: point-wise inversion helps clearly on **CPU** (0.86–0.98 relative render time) but is **marginal or negative on GPU** (0.90–1.15), hypothesised to be SIMD divergence — inversion only pays off in certain ray configurations, and a warp is unlikely to benefit uniformly. Inversion stays essential for *initialising* traversal; its use *during* traversal is optional.

## Limitations

The authors concede the hardware path outright:

> When using displacement maps with moderate resolution or limited tiling, and keeping the displacement content **static**, then combining pre-tessellation, pre-displacement, and hardware ray tracing remains the method of choice.

Design choices are explicitly biased toward *dynamic GPU displacement tracing*. RMIP is positioned as **orthogonal** to hardware traversal — it slots between the hardware TLAS/BLAS descent to the prism and the final ray–surface test. See [[Hardware ray tracing and micro-geometry]].

## Relation to other work

Cites [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] for using bounding prisms and barycentric grids to compress displacement in a way that *can* be hardware-traced without pre-tessellation, noting that in RMIP's case base triangles generally do not coincide with prism faces.

Does not cite [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] — the two appeared at the **same conference** and do not reference each other, which is worth remembering when reading either as "the" 2023 state of the art. [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] uses RMIP as its primary baseline, with measurements supplied by Thonat himself.

![[RMIP.pdf]]
