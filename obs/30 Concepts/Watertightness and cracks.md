---
title: Watertightness and cracks
tags: [concept, robustness, seams, lod]
---

# Watertightness and cracks

Where a displaced surface can leak, and how differently the format-based and ray-based approaches handle it.

## Two kinds of guarantee

**Bit-exact, by construction.** [[Maggiordomo et al. 2023 — Micro-Mesh Construction|µ-meshes]] are watertight *as a property of the format*: shared base vertex positions, shared displacement vectors, duplicated displacement values on shared edges, and a per-edge **decimation control bit** that halves the segments along an edge. The price is the constraint that **adjacent subdivision levels may differ by at most one** — which propagates into every level-assignment algorithm here as a correction phase. It survives uniform LoD reduction exactly, which is what makes the hardware's LoD bias safe.

**Numerical, approximately.** The tessellation-free ray tracers have no such guarantee. [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] states his method is "not completely watertight due to numerical errors" and wants watertight ray/triangle tests and robust traversal. [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]]'s is "as good as the ray/bilinear patch algorithm", plus a sampling condition `min(D(u,v)+ε) > dt` guaranteeing continuity across a base triangle.

## The unsolved one: UV seams

[[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] is candid that its surface is continuous only where texture coordinates, base surface and displacement all are — and **chart boundaries are none of those**. Seam-erasure preprocessing doesn't survive tiling or a map shared across meshes, and is too slow for interactive design. Its verdict:

> Supporting computationally efficient seam free displacement mapping in a more general context remains an open and challenging problem.

[[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] inherits the same limitation and names an additional one: smooth shell mapping is `C¹` *inside* a prism but only `C⁰` *between* prisms.

This is a direct consequence of putting the acceleration structure in **texture space** — the thing that buys free tiling and instancing is exactly the thing that makes seams hard. See [[Min-max mipmap and conservative bounds]].

## Crack prevention in compressed formats

[[Barczak et al. 2024 — DGF|DGF]] faces the same problem from the quantisation side: independently quantised blocks would not agree on shared vertex positions. Its answer is a **single global quantisation factor** over a 24-bit signed integer grid for the whole model, which guarantees watertight decode across block boundaries — and which is also the source of its San Miguel failure case, where huge backdrop triangles force a coarse grid that starves the small geometry of precision.

## Where it is still open

[[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al. 2024]] support static LoD across an object but state plainly:

> Watertightness between DMMs with non-uniform subdivision levels is currently not supported and planned as future work.

So for animated micro-meshes, the format guarantee that makes static µ-meshes watertight has not yet been extended to the adaptive case. See [[Micro-triangle and subdivision level]].
