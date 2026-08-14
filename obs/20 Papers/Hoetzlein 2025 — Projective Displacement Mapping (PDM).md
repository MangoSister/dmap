---
title: Projective Displacement Mapping for Ray Traced Editable Surfaces
authors: [Rama Hoetzlein]
affiliation: Quanta Sciences
year: 2025
venue: Pacific Graphics 2025 / Computer Graphics Forum 44(7), e70235
doi: 10.1111/cgf.70235
tags: [paper, displacement, ray-tracing, direct-sampling, interactive-editing]
status: read
---

# Projective Displacement Mapping for Ray Traced Editable Surfaces

The most recent displacement paper in the vault, and the only one that **benchmarks directly against the NVIDIA Micro-Mesh API**. Open access, CC-BY.

## Problem

Existing ray-traced displacement techniques target massive static scenes or asset compression, and all depend on acceleration structures needing preprocessing or costly rebuilds when displacement changes. PDM targets the opposite regime: **interactive look-development and sculpting** over low-poly base meshes, with ray-traced reflection, refraction and GI as live feedback. It therefore chooses direct sampling with **zero acceleration rebuild** on displacement edits.

## Core method

**Parallel offset prisms.** A classical offset prism has offset triangles that are *not* parallel to the base triangle, because vertex normals point in different directions — so projecting a world ray into it is nonlinear. PDM introduces a **normal factor** `n_f(u,v)`, an interpolant of `1/(Nᵢ·N_g)`, giving `R_pop(u,v,w) = P(u,v) + w·n_f(u,v)·N'(u,v)`. This "transforms the normal space to an orthogonal one where `w` can be interpreted as distance along the geometric normal", making the extruded triangles **parallel** to the base triangle and the sample space **linear**.

A useful invariant: any prism built as a scalar multiple along the same normals yields an **identical displaced surface**, so tightening the prism never changes the geometry. See [[Shell, prism and prismoid]].

**Prism intersection via bilinear patches.** Because adjacent vertex normals need not be coplanar, the interface between two prisms is a bilinear patch. Rather than splitting each face into two triangles and choosing a consistent diagonal, PDM ray-traces the three side faces directly as bilinear patches using [[Reshetov 2019 — Cool Patches]] — giving `C¹` continuity at the interface, no diagonal selection, and a GPU-efficient test.

**The PDM march.** The core trick: **sample the ray linearly in world space and map each sample to barycentrics via a "scanning triangle"** — a parallel offset triangle maintained at the current sample height — which avoids a per-sample tangent-space matrix transform. Each step advances the scanning triangle incrementally by linear vectors, takes barycentrics of the sample against it, interpolates uv, samples the height, and compares ray height against surface height. The paper flags the subtlety that you cannot simply accumulate the height delta, because sample heights are measured along the *interpolated* normal while the step is along the *geometric* normal.

**Shading normal correction.** The paper proves the finite-differenced displaced normal contains the flat geometric normal, `N_s = N_g + (∂D/∂v)(∂P/∂v) + (∂D/∂u)(∂P/∂u)`, which shows up as **faceting across base triangles** — an artefact it demonstrates is *also* present in Micro-Mesh and RMIP renders. The Phong-style fix `N'_s = N_s − N_g + N'` subtracts the geometric normal and adds the interpolated one, retaining micro-bumps, displacement shadows and correct silhouettes **without** an intermediate `C¹` surface.

**Thin features.** Ray marching misses features thinner than the step `dt`. The fix is to **stochastically jitter the first sample** along `t`, so thin features integrate over multiple samples at **zero overhead** — same ray count, same sample count.

**Watertightness** is "as good as the ray/bilinear patch algorithm"; sample continuity across a base triangle holds as long as `min(D(u,v)+ε) > dt`. This avoids the *buckling* artefacts that piecewise tetrahedral approximations of the prism interface produce.

**Degeneracy.** The offset formula has a singularity as `Nᵢ·N_g → 0` at sharp creases; the fix is inserting two triangles at any edge with interior angle below ~5°, needed for fewer than 10 triangles across two test models.

Implemented in **OptiX**, with prisms and AABBs in a hardware TRBVH and custom hit programs — so unlike TFDM this method does ride the hardware BVH down to the prism.

## Results

RTX 4090, 2048², `dt = 0.002`, 16-bit 4096² displacement maps.

- **40–60% faster than RMIP for primary rays, and 2×–13× for beauty images** on identical hardware, reaching **200–400 Mray/s** overall.
- **Memory overhead is a fixed 108 bytes per base triangle** — 0.1%–6.2% of scene total, against 17–39% for Micro-Mesh barycentric maps and 21–96% for RMIP.
- **Interactive editing** (Orb, 8192² map): RMIP 2.9 fps (3.4 ms acceleration rebuild + 329 ms trace) vs PDM **16.7 fps full / 70.4 fps** for a 256² brush region, with **zero** rebuild.
- Teaser: 7,710 base triangles ray traced at 3460×1024 in **28 ms** for one sample including primary, path tracing, reflection, refraction and shadows.

## Limitations

Stated plainly: limitations "are related to ray marching and sampling", and the method is **not intended for massive scenes, terrain, or compression of high geometry source meshes**. Quality degrades at grazing angles as `dt` grows. Larger changes to the *base mesh* (as opposed to the displacement) would still require BVH reconstruction.

## Relation to other work

The only paper here that treats micro-meshes as a **measured baseline** rather than a citation. It configured Micro-Meshes at 5-level maps with 5-level pre-tessellation and modified the [[NVIDIA 2022 — Ada Lovelace Architecture|Micro-Mesh Toolkit]] source to emit surface-normal images for comparison, concluding:

> Although Micro-Meshes relies on tessellation, with goals and techniques different than ours, we still achieve faster ray tracing performance on most models tested.

It cites [[Thonat et al. 2023 — RMIP|RMIP]] (primary baseline, measurements supplied by Thonat), [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]], and [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] — rejecting the latter because "the pre-computation of data structures and solving cubic equations preclude our goals for interactive editing".

Note the comparison is not apples-to-apples across the vault: PDM measures on an RTX 4090 with its own scene set, while RMIP measured on an RTX 3080 against TFDM. See [[MOC — Ray Traced Displacement Mapping]] for how to read these numbers together.

![[Projective Displacement Mapping for Ray Traced Editable Surfaces.pdf]]
