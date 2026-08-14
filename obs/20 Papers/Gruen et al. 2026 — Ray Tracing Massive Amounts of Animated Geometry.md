---
title: Ray Tracing Massive Amounts of Animated Geometry
authors: [Holger Gruen, Carsten Benthin, Michael Kern, David McAllister]
affiliation: AMD
year: 2026
venue: PACMCGIT 9(4), Article 49 — HPG 2026
doi: 10.1145/3820014
tags: [paper, animation, tetrahedral-cage, watertightness, ray-tracing, clusters]
status: read
---

# Ray Tracing Massive Amounts of Animated Geometry

The same AMD group that wrote [[Barczak et al. 2024 — DGF|DGF]] and [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|the animated DMM paper]], now abandoning displacement entirely for **tetrahedral cages**. Reading it next to their 2024 paper is the clearest evidence in the vault of where the field went.

## Problem

Ray tracing animated meshes requires animating triangles *and* rebuilding the acceleration structure every frame; both scale with triangle count. The idea: wrap geometry in a low-resolution **tetrahedral cage**, animate only the cage, and keep the per-tetrahedron geometry and its acceleration structures **permanently static**.

## Core method

**Preprocess.** Voxelise the object in its rest pose, split each occupied voxel into six tetrahedra, discard empty ones, then **clip every triangle against the four bounding planes** of each tetrahedron it overlaps — necessary "to avoid artifacts, as each tetrahedron will deform the same triangle differently". Clipped vertices are deduplicated and shared with neighbours. The per-tetrahedron mesh is a **µMesh**; the BLAS over it is a **µBLAS**, "created once and will remain static (never modified in any way)" and reusable across uniquely animated instances. The top-level structure over tetrahedra is the **tetLAS**.

**Runtime.** Only cage vertices are animated — a piecewise-linear spatial deformation — and only the tetLAS is updated. *"The cost of animating geometry per frame depends solely on the resolution of the tetrahedral cage and is decoupled from the density of the original geometry."*

**Variant 1, fast but leaky.** The rest-pose and animated tetrahedra give basis matrices `M` and `A`; setting the DXR instance transform to `A·M⁻¹` makes world→local a pure basis change, exploiting the fact that the API requires an inverse matrix anyway. Leaks are mitigated by dilating each tetrahedron before clipping, creating deliberate overlap. Measured: without mitigation ~0.01% of rays escape a closed sphere; with it, none.

**Variant 2, provably watertight.** The leak's root cause is that adjacent tetrahedra apply *different* world→local transforms to the same ray. The fix is a general recipe worth remembering: store every vertex and every bounding volume in **4D barycentric coordinates** relative to the tetrahedron's four vertices, **sort those four vertices by global unique ID** and always evaluate in that order — guaranteeing **bit-exact reconstruction from either adjacent tetrahedron** — then traverse a **4D BVH**, reconstructing world-space AABBs on the fly with the min-sum heuristic. Appendix A proves the bounding property survives any cage deformation via interval-arithmetic subdistributivity, "even if the tetrahedron is inverted, scaled to zero, or mirrored."

The catch: DXR cannot express 4D-relative geometry, so this variant runs as **procedural geometry with a software intersection shader** — see [[Hardware ray tracing and micro-geometry]].

## Results

RX 9070 XT, 1080p, two rays per pixel.

- **584M animated triangles, 2.8M tetrahedra, 12.43 ms/frame (~60 fps), 770 MB** — a scene that "cannot be ray traced with the standard approach due to the GPU memory limit of 16 GB".
- Versus animating every triangle: **16.1× less memory**, up to **9× faster** overall, though render time alone is slightly higher.
- Tetrahedron counts are two to three orders of magnitude below triangle counts; clipping inflates triangles 1.4–2.3×.
- **The watertight variant costs 19–80× render time and 2.3–3.2× memory**, so it is offered "as a validation reference" only.
- Frame breakdown: 78% goes to animating cage vertices and rebuilding the tetLAS.

## Limitations

Poorly suited to topology change (explosions, cutting), motion below cage scale (**cloth or facial folds**), sharp skinning weight changes near joints, and rigid objects. Shared edges spanning tetrahedra get tested more than once. The method "trades increased ray traversal and intersection costs for substantially reduced animation and acceleration-structure update costs."

## Relation to other work

The passage that matters most for this vault's arc:

> Displaced micro-meshes (DMMs) avoid the construction of the BVH over the geometry by providing an implicitly defined BVH over the hierarchically encoded micro-mesh. Gruen et al. [2024] proposed a method to improve the quality of DMMs in the context of animation. **DMMs have not been widely adopted, as their representation is tied to piecewise manifold objects, which limits their applicability when dealing with real-world content.**

That is the first author writing off his own 2024 paper's premise — and note the reason given is **topological**, the same objection [[Barczak et al. 2024 — DGF|DGF]] raised in 2024, not the API withdrawal. See [[Graphics API and hardware support timeline]].

The closest prior work is Luton & Tricard's HPG 2025 tetrahedral-cage rasterisation, extended here to ray tracing. Clusters are cited as the emerging direction. It does **not** cite [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|its first author's own opacity work]], [[Barczak et al. 2024 — DGF|DGF]], or any of the displacement ray-tracing papers.

> [!note] A tool worth borrowing
> [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al. 2024]] left "watertightness between DMMs with non-uniform subdivision levels" as future work. This paper does **not** solve that — but its ID-sorted 4D-barycentric bit-exactness recipe is a general method for consistent reconstruction of shared boundaries across independently transformed sub-domains, which is structurally the same problem. Nobody has made that connection. See [[Watertightness and cracks]].

![[Ray_Tracing_Massive_Amounts_of_Animated_Geometry.pdf]]
