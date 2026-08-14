---
title: Shell, prism and prismoid
tags: [concept, geometry, bounding]
---

# Shell, prism and prismoid

The container that appears in **almost every paper in this vault**, under three slightly different names. Understanding the differences explains most of the design disagreements here.

## The shell

Given a base mesh and its **offset mesh** (the base displaced by the maximum height along vertex normals), the **shell** is the layer between them. All displaced geometry lives inside it, so the shell is simultaneously a bounding volume, a coordinate system, and — in [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|shell mapping]] — a container for arbitrary instanced geometry rather than just a height field.

Per base triangle, the shell cell is a **prism**. Coordinates inside it are `(α, β, h)` — barycentrics plus height, which [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] calls *canonical space*.

## Prism vs prismoid vs parallel offset prism

| Variant | Used by | Definition |
|---|---|---|
| **Bounding prism** | [[Smits et al. 2000 — Direct Ray Tracing of Displacement Mapped Triangles\|Smits 2000]], [[Thonat et al. 2023 — RMIP\|RMIP]], [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping\|Ogaki]] | Base triangle extruded along interpolated vertex normals |
| **Bilinear prismoid** | [[Maggiordomo et al. 2023 — Micro-Mesh Construction\|µ-meshes]] | Hull between the base face and the face formed by the **tips of the displacement vectors** — the µ-mesh's self-bounding volume |
| **Parallel offset prism** | [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)\|PDM]] | Vertex normals scaled by a **normal factor** `1/(Nᵢ·N_g)` so the offset triangle is *parallel* to the base triangle, linearising the sample space |

A structural fact all three share: because adjacent base-vertex normals need not be coplanar, **the side faces are bilinear patches, not planar quads**. [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] and [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] both intersect them directly using [[Reshetov 2019 — Cool Patches]]; [[Porumbescu et al. 2005 — Shell Maps|Shell Maps]] instead decomposed prisms into tetrahedra, whose piecewise-linear interface causes the **buckling** artefacts PDM cites.

## The four jobs a prism does

1. **Bounding.** Tighter prisms mean fewer wasted intersection tests. [[Maggiordomo et al. 2023 — Micro-Mesh Construction|Micro-Mesh Construction]] measures this directly: optimising prismoid extents per base vertex rather than globally shrinks total displacement volume by **>3×** and yields a **3–4× rendering speedup**.
2. **Space subdivision.** Prisms of neighbouring triangles **do not overlap in 3D**, unlike their AABBs — a point [[Thonat et al. 2023 — RMIP|RMIP]] makes explicitly, and the reason it uses prisms as BVH leaves.
3. **Guaranteeing invertibility.** RMIP's Newton inversion is not defined everywhere, but a prism is **fully contained in the convergence region**, so every point inside one projects reliably back into its base triangle.
4. **Quantisation budget.** Shorter displacement vectors mean a fixed bit count (11 in the µ-mesh format) resolves finer detail. This is why shell volume is an *optimisation target* and not merely a bound — see [[Base mesh quality objectives]].

## Why shell volume is optimised, not just measured

[[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] make it a **loss term**, noting that although a prismoid's volume is the sum of three tetrahedra, differentiating that directly is deficient because min/max fitting only exposes the extreme parameters to the optimiser. Their surrogate is the **variance of displacements** around each base vertex. They also name the two hardware payoffs of a small shell: effective occlusion culling, and a lower ray–µ-face miss ratio inside the prismoid.

The staged pipeline's structural weakness is exactly here — shell volume is fixed the moment the base mesh exists, but its true value is only known after baking.

## Job 3, restated exactly

Invertibility (job 3 above) is usually treated as a property of Newton's method. It is not — it is a property of the geometry, and **for perpendicular displacement it has a closed form**. The metric of the displaced surface degenerates precisely when `h = 1/κ₁` or `h = 1/κ₂`, the **focal surface** of the normal congruence. That is the same event as the prism ceasing to be a valid coordinate system, so *"the prism is inside the convergence region"* and *"the metric determinant is nonzero"* are the same statement, and `h < 1/κ_max` is a computable shell-thickness criterion rather than a heuristic one.

> [!warning] The closed form does not survive interpolated vertex normals
> Which is to say, it does not survive the case every format here uses. Two things break at once: the curvature factorisation needs a Weingarten relation that interpolated normals violate, and the prism Jacobian and the surface metric determinant stop being the same quantity — they differ by a `cos ψ` obliquity factor. Three distinct determinants have to be separated, and `h < 1/κ_max` is not the criterion for any of them. See [[Obliquity and the integrability defect]].

The same quantity is the **reach**, or local feature size, that grid-free surface PDE solvers need to define a volumetric neighbourhood — so the shell bound this literature computes to avoid self-intersection is a number another literature computes for entirely unrelated reasons. See [[The induced metric of a displaced surface]] and [[Laplace–Beltrami on displaced surfaces]].
