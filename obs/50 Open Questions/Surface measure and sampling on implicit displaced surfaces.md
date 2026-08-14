---
title: Surface measure and sampling on implicit displaced surfaces
tags: [open-question, research-direction, sampling, integration, priority]
priority: 2
rests-on: "Ogaki 2023, stated limitation"
---

# Surface measure and sampling on implicit displaced surfaces

> [!note] This is analysis, not a claim from any paper
> The *limitation* below is Ogaki's own; the proposed structure is mine. See [[MOC — Open Questions]].

> [!success] Confirmed open — 2026-08-10, with one piece of prior art to cite
> None of the three 2026 papers computes surface area or importance-samples a displaced surface. [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|The foliage paper]] uses projected area only as a bake-time weight for dithering, and importance-samples *lights*, never foliage; it has no emissive vegetation. Its critique that "quadric error does not penalize loss of aggregate surface area" names area as the *problem* and then fixes it with a feature-scale heuristic rather than an area-preserving metric.
>
> **But [[Zhang et al. 2026 — DJM|DJM]] must now be cited as prior art.** Its Displacement Jacobian Metric is a closed-form `Det(J)` of the base→displaced map — literally a *local* differential-area scaling factor, constrained to stay near 1. DJM measures pointwise area distortion but never integrates it, never computes total area, and never uses it as a sampling density. That is the exact gap this note proposes to fill, and DJM hands you the closed-form Jacobian to do it with.

> [!danger] Outside check — 2026-08-10 — **substantially occupied**
> This was the "cleanest well-posed hole in the vault". It is not. **Ling, Madan, Sharp & Jacobson 2025** compute surface area *and* draw uniform samples on implicit surfaces using nothing but a ray-casting subroutine — which [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]], [[Thonat et al. 2023 — RMIP|RMIP]] and [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] all already have. Both headline deliverables are obtainable today with no new structure.
> Separately, **LEADR mapping (2013) already stores displacement-gradient moments in a mipmap**, so the proposed pyramid is not new either — only its *conservativeness* is.
> Something real survives, but it is much narrower. See [[#Prior art outside this vault]]. **Recommend re-ranking below [[Non-ray queries — slicing, volume and mass properties|direction 5]].**

**The gap.** ~~If you never materialise the micro-geometry, you cannot enumerate it — so you cannot compute its area, and you cannot draw a point uniformly from it.~~ **That premise is false** (Ling et al. 2025, below). The surviving gap is narrower: you cannot get *deterministic, conservative, localised* area — area bounded per region, tightening hierarchically, cheap enough to query per-node — and that is what LoD metrics, coverage and prefiltering actually need. Global area and global uniform sampling are solved; regional and hierarchical are not.

## Evidence it is open

[[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] names it directly as a limitation: surface area computation is hard because it requires integrals, unlike pre-tessellation where micro-triangles can simply be enumerated — and this **hurts light sampling of emissive displaced surfaces**.

No paper *here* addresses it — a named victim, a named cause, no solution **within this literature**. Outside it, most of the hole is filled; see below.

## Prior art outside this vault

Checked 2026-08-10. This is the direction the outside check hurt most.

**The one that takes the headline result.**

- **Ling, Madan, Sharp & Jacobson 2025**, *Uniform Sampling of Surfaces by Casting Rays* (Computer Graphics Forum 44(5)). Cast uniformly distributed random rays, collect **all** intersections, and the resulting points are uniformly distributed over the surface in a white-noise sense — via the **Cauchy–Crofton formula** from integral geometry. The same intersection counts give a surface-area estimator. It needs nothing but a ray-all-intersections subroutine, and is demonstrated on SDFs, neural implicits, analytic implicits, offset surfaces and Gaussian splats. Ogaki's stated limitation is, in the general case, answered.

  **What it does not do**, and this is where the remaining contribution lives:
  1. It is a **stochastic estimator**, not a conservative bound — no per-node bounds, no deterministic guarantee, error falls as `1/√n`.
  2. It needs **every** intersection along a ray. That is the expensive traversal mode; displacement structures are specifically optimised for first-hit with early-out. The cost argument on *this* representation is unexamined.
  3. It gives **global** area, not *area within a region*. Localised measure is the primitive behind LoD error, coverage and [[Prefiltered coverage from opacity hierarchies]] — none of which a global estimator serves.
  4. No **hierarchical CDF**, so no cheap area-proportional descent, and no importance sampling by radiance × area for emissive geometry.

**Prior art on the proposed mechanism itself.**

- **Dupuy, Heitz, Iehl, Poulin, Neyret & Ostromoukhov 2013**, *Linear Efficient Antialiased Displacement and Reflectance Mapping* (LEADR, ToG 32(6)), after **Olano & Baker 2010** (LEAN) for normal maps. LEADR stores the **first two moments of the displacement gradients in mipmaps** and builds a Beckmann NDF with masking and shadowing from them. A displacement-gradient mipmap already exists and shipped. It is non-conservative — moments and linear filtering, not min-max bounds — and serves reflectance rather than measure. **The novelty below is "conservative bounds on `∇h`", not "mipmap `∇h`".** Say so explicitly or a reviewer will say it first.
- **Wu, Zhao, Yan & Ramamoorthi 2019**, *Accurate Appearance Preserving Prefiltering for Rendering Displacement-Mapped Surfaces* (SIGGRAPH, ToG 38(4)). Jointly prefilters displacement maps and BRDFs into reduced-resolution SVBRDFs, with spatially varying NDFs and a 6D scaling function capturing shadowing, masking and interreflection. Masking and shadowing on a displaced surface **is** a projected-area computation — so "area of a displaced surface" has been computed, in statistical and directional form, for appearance. The geometric-area vs projected-area distinction must now be drawn up front, not deferred to *Risks*.
- **Munkberg et al. 2010** (already in this vault) — verified directly: it bounds the **base-patch normals**, via a normal-vector Bézier patch whose control vectors are normalised and solid-angle-bounded in an OBB frame, plus min-max of `h`. It does **not** bound `h_u`/`h_v`, and does not touch the Jacobian determinant. The gradient-bounding step proper is genuinely unclaimed.

**Concurrent-work risk is higher than this vault assumed.** [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] and [[Thonat et al. 2023 — RMIP|RMIP]] come from Boubekeur's group at Telecom Paris — which *also* owns the appearance-prefiltering line (MIPNet; "Hybrid mesh-volume LoDs for all-scale pre-filtering of complex 3D assets"). This direction sits inside their existing agenda rather than beside it. That is the opposite of the vault's assumption that direction 3 carried the concentrated risk.

## What it blocks

Far more than emissive surfaces:

- **Light sampling** — an emissive displaced surface cannot be importance-sampled by area, so any scene with glowing displaced geometry is biased or noisy.
- **Baking** — AO, lightmap, curvature and thickness bakes all need uniform surface samples.
- **Point-cloud and particle emission** — scattering grass, dust, wear or decals over a surface needs area-proportional sampling.
- **Physical quantities** — total surface area drives heat transfer, drag, wetting, coating and paint estimates, and material cost in manufacturing.
- **LoD error metrics** — how much *area* a coarser mip destroys is a better-motivated LoD criterion than height error, and nobody measures it.

## The technical shape of it

The area element of `S(u,v) = P(u,v) + h(u,v)·N̂(u,v)` is

`dA = |∂S/∂u × ∂S/∂v| du dv`

with `∂S/∂u = P_u + h_u·N̂ + h·N̂_u`. For a base triangle, `P_u` and `P_v` are constant and the normal terms are known analytically — so the integrand depends on the displacement **value and its gradient**.

That is the whole idea: today's structures bound `h`. To bound area you must also bound `∇h`.

> [!tip] The stronger framing — see [[The induced metric of a displaced surface]]
> Bounding `∇h` does not give you the area. It gives you the **first fundamental form**, of which area is one scalar functional. The full derivation shows `I = (I₀ − 2h·II₀ + h²·III₀) + ∇h∇hᵀ + sym(∇h aᵀ)` — the offset-surface metric plus a low-rank slope correction — from which `dA` factors into base × curvature stretch × slope stretch in closed form.
> This matters for the direction's survival. Area is a scalar, and a stochastic estimator of a scalar (Ling et al. 2025) beats a new data structure. A **metric is an operator**: it gives Laplace–Beltrami, hence geodesics, diffusion, spectra and anisotropy, none of which can be assembled from area estimates at any sample count. If this note is to be rescued from *substantially occupied*, that is the route.

**Proposal — a joint gradient-bound pyramid.** ~~Store conservative bounds on `h_u` and `h_v` per texel per level — two more channels, same pyramid.~~ **Superseded 2026-08-11:** independent min-max gradient channels lose the `h`–`∇h` correlation the metric needs and produce spuriously loose (even sign-wrong) determinant bounds in the oblique regime. The correct structure is a plane-plus-remainder node — the [[Taylor-model bound pyramid]] — which subsumes the min-max height channel on the same traversal. From it you get conservative per-node bounds on `dA`, and therefore:

1. **Hierarchical area bounds** — tightening as you descend, exact in the limit. **This is now the differentiator**: deterministic, conservative, *localised*, and available per node. Ling et al. give none of it.
2. **Total surface area** by summation over the pyramid, at mipmap-construction cost rather than per-query cost. Ling et al. 2025 is the baseline here, and it is a strong one — the claim must be *build-once and exact-in-the-limit* versus *per-query and stochastic*, measured, not asserted.
3. **Area-proportional importance sampling** — treat the pyramid as a hierarchical CDF: descend choosing children proportional to their area bounds, then sample within the leaf. The standard hierarchical-warping trick over a structure that already exists. Also has Ling et al. as a baseline for the *uniform* case; the defensible extension is warping by **radiance × area** for emissive displaced geometry, which nothing found addresses.

[[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM's]] affine arithmetic is the natural tool for the bounding step, since the normalisation of `N̂` makes the Jacobian nonlinear — and affine forms are exactly what TFDM already propagates through this equation.

## Why this one is attractive

It is small, self-contained, and falsifiable. Ground truth is trivially available (pre-tessellate and sum triangle areas), and the storage cost is the same order as structures these renderers already build.

The old success statement — *tessellation-free displacement becomes integrable, not just intersectable* — **is no longer available**: Ling et al. 2025 made it integrable, for any representation with a ray subroutine. The replacement is narrower and must be earned against that baseline: *displaced-surface area becomes conservatively bounded and hierarchically localised, at build time rather than per query.*

It also generalises. The same machinery answers "how much area lies within this region", which is the primitive behind coverage, LoD, and — interestingly — the aggregate opacity values in [[Prefiltered coverage from opacity hierarchies]].

## Risks

- **The baseline may simply be good enough** — the dominant risk now. If Ling et al. 2025 gives adequate area and uniform samples on a displaced surface at acceptable cost, two extra mipmap channels are a hard sell. This is cheap to settle and should be settled *first*: implement their estimator over [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] traversal and measure the cost of all-hits versus first-hit. If the answer is "fine", this direction is dead and two days were spent rather than two months.
- **Gradient bounds may be loose** where displacement is high-frequency, which is precisely where area is largest. Loose bounds still give valid importance sampling (just less efficient), so this degrades rather than fails.
- **Silhouette and self-occlusion** — sampling the surface uniformly is not the same as sampling *visible* surface; for light sampling you eventually want the solid-angle measure, which is harder. Wu et al. 2019 already occupies the statistical/directional version of this for appearance, so the harder follow-on is *also* partly taken.
- Displaced surfaces that self-intersect (permitted by [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]]) have ill-defined area — decide whether to count overlap once or twice.
- For explicit [[Maggiordomo et al. 2023 — Micro-Mesh Construction|µ-meshes]] area *is* enumerable, so this matters specifically for the tessellation-free family — though it returns for µ-meshes under LoD bias, where the area changes with subdivision level and nobody tracks it.

Related: [[Tessellation-free vs pre-tessellation]] · [[Min-max mipmap and conservative bounds]] · [[Micro-triangle and subdivision level]]
