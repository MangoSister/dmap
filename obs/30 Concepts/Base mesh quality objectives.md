---
title: Base mesh quality objectives
tags: [concept, construction, optimization, decimation]
---

# Base mesh quality objectives

What makes a *good* base mesh for a displaced representation — and why the answer is not "a good simplified mesh".

[[Maggiordomo et al. 2023 — Micro-Mesh Construction|Micro-Mesh Construction]] names four goals that **actively conflict**, which is the whole difficulty of the problem.

## The four objectives

**1. Coarseness.** The µ-mesh format supports amplification factors over **1000:1**, so the target is a base mesh up to three orders of magnitude coarser than the input.

**2. Reprojectability.** The input surface must be expressible as a warped height field over the base mesh **along the interpolated displacement directions**. Ideally those rays strike the input surface locally orthogonally. This is what a general-purpose simplifier does not give you — MeshLab and Simplygon base meshes produce self-intersections when displaced, because they do not guarantee coherently oriented displacement directions.

**3. Isotropy.** Roughly equilateral base triangles sample the final surface more efficiently.

**4. Minimal prismoid volume.** Shorter displacement vectors mean better accuracy from a fixed bit budget *and* tighter bounds for culling. See [[Shell, prism and prismoid]].

Goals 1 and 2 pull against each other directly; 2 and 3 also conflict, since the base triangulation that reprojects best is rarely the most equilateral.

## The visibility value

The key technical device for reprojectability. For a base vertex `v` with adjacent face normals `N`:

`V(v) = max_{d} min_{n∈N} (d·n)`

The maximiser is the **displacement direction**; the value predicts local quality, ranging from −1 (normals cover all directions) to +1 (adjacent faces coplanar). **Only strictly positive visibility is admissible** — a negative value means interpolated displacement directions vanish somewhere inside a base face.

Computed by a Welzl-style algorithm over an active subset of 2 or 3 normals, **more than 26× faster** than solving the equivalent quadratic program (0.64 s vs 17.24 s for a million instances).

## Three ways to optimise the same four goals

**Hand-designed collapse costs** ([[Maggiordomo et al. 2023 — Micro-Mesh Construction|Maggiordomo et al.]]). Aggregate cost = geometric quadric error **divided by** penalty factors for normal deviation, aspect ratio and visibility — so a zero in any factor makes the cost diverge and the collapse never happens. Isotropy is enforced *adaptively*, against each face's best-ever aspect ratio rather than a hard floor, which avoids locking simplification early. Prismoid volume is handled **afterwards**, as a post-process shifting and rescaling per base vertex.

**End-to-end gradients** ([[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]]). Replace surrogates with the real objective: an L₁ **image loss** as reprojectability in image space, plus differentiable regularisers for shell volume (variance of adjacent displacements) and isotropy (variance of a face's edge lengths). The base vertices stay **optimisable throughout** rather than frozen after initialisation.

Their critique of the staged approach is sharp and specific: **the shell volume is essentially fixed the moment the base mesh is produced, yet its true value is only known after displacement baking.** Optimising a surrogate at stage 2 for a quantity measured at stage 4 is the structural flaw.

They also make an argument worth remembering: their base meshes sometimes have **worse** geometric error and isotropy than the baselines, and they defend this — those metrics "only have rough correlations to the final µ-mesh quality". Judge the µ-mesh, not the base mesh.

**Analytic distortion** ([[Zhang et al. 2026 — DJM|DJM]]). Derives a **closed-form Jacobian** of the displacement map, so distortion is `Det(J)` measured against 1 and **bijectivity is readable from the same construction**. This replaces two of the four objectives outright: Maggiordomo's *normal deviation* and *approximate bijectivity* were heuristic stand-ins for exactly this quantity. It keeps quadric error only as a collapse *ordering* and triangle shape as a separate hard threshold, and explicitly rejects blending everything into one energy — "the challenge here would be to meaningfully balance the different terms as they measure hard to compare properties" — in favour of progressively relaxed thresholds.

It also **sidesteps shell volume entirely**: there is no prism or enclosure-volume term anywhere in the method, despite that being the objective with the largest measured payoff (3–4× rendering speedup). And it eliminates ray-cast baking in favour of a tracked correspondence, on the grounds that ray casting is unstable and ambiguous.

DJM beats both Maggiordomo and QEM on every error measure at all subdivision budgets. It does **not** compare against [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]], calling that method complementary and citing the absence of released code — so the four objectives now have three published answers and no head-to-head between the two newest.

## Two objectives none of the three measure

All four goals above, and all three published answers, are about how well the base mesh *carries* a displacement field. None measures whether the base mesh's **normal field is geometrically coherent with its geometry** — and for a flat triangle carrying interpolated vertex normals, it generally is not. Two defects fall out of the metric derivation, both computable from edge vectors and vertex normals alone, before any baking. Full treatment in [[Obliquity and the integrability defect]]; in brief:

- **Obliquity** `a = (P_u·N̂, P_v·N̂)`, with invariant magnitude `aᵀG₀⁻¹a = sin²θ` against the face normal. Nonzero means displacement is not perpendicular to the base, so the displaced surface is not an offset of it and the prism is sheared. It **grows with base coarseness** — worst exactly where µ-meshes are most aggressive — and it is the only term in the metric that can *shrink* it, so it brings shell degeneracy closer than the focal-surface bound predicts.
- **Integrability defect** `δ = P_u·N̂_v − P_v·N̂_u`. Nonzero means the normal field is not the Gauss map of any surface tangent to the base: the shape operator is not self-adjoint and no proper second fundamental form exists. A pointwise, quantitative form of the shading-normal artefact PDM found qualitatively in Micro-Mesh and RMIP output.

The two are **nested**: `a = 0 ⟹ δ = 0`, so there is one knob, not two. And the trade-off behind it is unavoidable — displacing along the geometric face normal gives `a = 0` and cracks at every base edge, so obliquity is precisely the price paid for [[Watertightness and cracks|watertightness]]. Note also that Maggiordomo's visibility value tests *admissibility* (do displacement directions vanish inside a face?) while obliquity measures *distortion*; a base mesh can pass the visibility test comfortably and still carry a large `a`.

Neither appears in Maggiordomo's four penalties, in Dou et al.'s regularisers, or in DJM. DJM's `Det(J)` is a *scalar*; the metric it is a determinant of has an eigenstructure, and the anisotropy is what governs which direction detail is lost in. So the objectives are less settled than [[MOC — Open Questions]] currently claims — DJM replaced two heuristics with an exact quantity, but a coarser one than the construction supports.

## The same idea outside triangles

[[Mendiratta et al. 2026 — NeuBase|NeuBase]] solves a recognisably similar problem for Catmull–Clark surfaces — a coarse quad base plus an offset field along control-mesh normals — and cites both micro-mesh papers as the triangular instance of the family. Its base mesh comes from quadrangulation rather than decimation, and it spends its first 2000 training iterations optimising **control vertex positions alone** before touching the offset field, which is the same instinct as Dou et al.'s ℓ=0 warm-up.

## Where a good base mesh stops being enough

[[Barczak et al. 2024 — DGF|DGF]] argues the objective is unachievable for some content at any quality of base mesh: scalar displacement along interpolated normals **cannot represent arbitrary topology**, so models with sharp features or high depth complexity need so many base triangles that the compression advantage disappears. Its Sponza failure case is the demonstration. [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al.]] concur: a DMM over one base triangle has **genus zero**, full stop.
