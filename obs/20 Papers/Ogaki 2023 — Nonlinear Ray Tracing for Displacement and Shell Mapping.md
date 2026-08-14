---
title: Nonlinear Ray Tracing for Displacement and Shell Mapping
authors: [Shinji Ogaki]
affiliation: ZOZO, Inc.
year: 2023
venue: SIGGRAPH Asia 2023 Conference Papers
doi: 10.1145/3610548.3618199
tags: [paper, displacement, shell-mapping, ray-tracing, texture-space]
status: read
---

# Nonlinear Ray Tracing for Displacement and Shell Mapping

The other 2023 answer to [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]]. It appeared at the **same conference** as [[Thonat et al. 2023 — RMIP|RMIP]] and neither cites the other, so they are best read as independent parallel attacks on the same problem.

Its distinguishing claim is **robustness**, not speed: it renders configurations that TFDM cannot render at all.

## Problem

Displacement mapping and shell mapping both generate geometry inside the **shell** between a base mesh and its offset mesh. Getting high performance, low memory, interactive feedback and implementation simplicity *simultaneously* is hard because the mapping between shell space and texture space is **nonlinear** — a straight ray in shell space becomes a curve in texture space.

Prior work either pre-tessellates (gigabytes), ray-marches tetrahedra (initialisation cost plus piecewise-linear artefacts), or bounds with affine arithmetic in world space (overlapping conservative boxes, tuning burden, and outright failure when a base triangle is **degenerate in uv space**).

## Core method

The answer: do **all** BVH traversal and primitive intersection **entirely in texture space**, by writing the nonlinear ray as a degree-2 rational function.

**Spaces.** *Shell space* is the layer between base and offset mesh. *Canonical space* is `(α,β,h)` — barycentrics plus height. *Texture space* replaces `α,β` with interpolated `u,v`.

**The nonlinear ray.** Substituting the ray into the shell parameterisation and dotting with two vectors perpendicular to the direction **eliminates `t`**, giving `α` and `β` as **quadratic rational functions of `h`** — a degree-2 rational curve **parameterised by the height**. Substituting the uv interpolation gives the same form in texture space with the *same denominator*. (The α–β relation is a conic and could be written as a quadratic rational Bézier, but that would make `h` itself rational, so height is kept as the parameter.)

**Ray–microtriangle intersection.** Substituting the ray into the microtriangle plane equation and clearing the denominator gives a **cubic in `h`**, solved with Yuksel's [2022] root finder. Solving for `α,β` directly is numerically fragile where the determinant vanishes (notably when `h_in = h_out`), so the plane is also converted to canonical space to give a third linear equation, and the implementation picks **the two equations with the largest absolute determinant**.

**Normals via the adjugate.** The world normal is `n = adj(M) n_αβh` rather than `M⁻ᵀ n_αβh` — using the adjugate avoids dividing by the determinant, so normals survive even when the prism degenerates to a plane. The paper enumerates the two genuinely undeterminable cases.

**Ray–AABB in texture space** requires solving four quadratics — more expensive than a slab test, but it avoids conservative affine boxes entirely, and crucially **texture-space AABBs do not overlap at all**, whereas shell-space boxes do.

**Two-level structure.** The TLAS is a 4-ary SAH BVH over **prisms** whose sides are **bilinear patches** (intersected with [[Reshetov 2019 — Cool Patches]]), traversed with ordinary rectilinear rays so standard traversal code is reused. The BLAS is traversed with nonlinear rays:

- for *displacement mapping*, a [[Min-max mipmap and conservative bounds|min-max mipmap]] whose finest leaves hold two microtriangles;
- for *shell mapping*, an **implicit quadtree that consumes no memory**, whose leaves all reference the BVH of a single instanced object. On hitting a leaf you shift in uv and traverse the instance. This is what makes tiled woven textiles, knitted fabric, fur and feathers practical.

**Optimisations.** Root selection and texel discard are inherited from TFDM and extended to shell mapping. *Range reduction* clamps the `t` and `h` search intervals using the prism entry/exit. Two exact fast paths replace LoD: if a texel's min equals its max, terminate and do a rectilinear ray/flat-rectangle test; if a microtriangle's three heights are equal, use a rectilinear ray/flat-triangle test.

Initialisation needs only the min-max mipmap, the instanced object's BVH, and the prism BVH — **base and offset meshes are never explicitly generated**.

## Results

CPU only — 20 cores of an Apple M1 Ultra. Displacement: **20–30% of pre-tessellated speed** at ~1/60 the memory. Shell mapping with fur: **0.7 MB vs 0.7 GB and more than twice as fast**, because pre-tessellated fur produces long thin world-space AABBs while texture-space boxes behave like OBBs. Combined scenes reach **757 million microtriangles**.

The headline robustness result renders **without artefacts in both single and double precision even when the base mesh is degenerate in uv space, degenerate in world space, or has vertex normals oriented to cause self-intersections** — the exact case TFDM fails on.

## Limitations

- **Not completely watertight** due to numerical error; wants a watertight ray/triangle test and robust traversal. See [[Watertightness and cracks]].
- **Surface area computation is hard** (it needs integrals), which hurts light sampling of emissive displaced surfaces — a real cost of never materialising the geometry.
- No vector displacement; fur flow can only be steered by varying base vertex normals.
- Smooth shell mapping is `C¹` inside a prism but only `C⁰` between prisms, and cracks appear where uvs are discontinuous — a limitation shared with TFDM.
- No SIMD packet traversal for nonlinear rays.

## Relation to other work

On micro-meshes, it is explicit and slightly sceptical:

> Micro-meshes are similar in principle, and each base triangle stores compressed scalar values on a barycentric grid. Hardware that natively supports the rendering of micro-meshes and a dedicated generation algorithm [Maggiordomo et al. 2023] are available. These methods can render unprecedentedly detailed geometry, but **it is unclear how they can be extended to shell mapping**.

The cited generation algorithm is [[Maggiordomo et al. 2023 — Micro-Mesh Construction]]. That reservation is the sharpest statement in this vault of what shell mapping buys that [[Micromap|micromaps]] don't: arbitrary *instanced geometry* in the shell, not a height field. It also notes TFDM's intersection shader prevents full use of hardware ray tracing — while itself being a CPU implementation that makes no RT-core claim, arguing instead that keeping a *standard* BVH means standard BVH machinery still applies.

Built on [[Porumbescu et al. 2005 — Shell Maps]] and [[Jeschke et al. 2007 — Smooth and Curved Shell Mapping]]. Cited as a baseline by [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]], which rejects it on the grounds that precomputation and cubic solving preclude interactive editing.

![[Nonlinear_Ray_Tracing_for_Displacement_and_Shell_Mapping.pdf]]
