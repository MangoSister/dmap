---
title: Result — Check-first literature sweep
tags: [result, phase0, literature, novelty]
created: 2026-08-26
---

# Result — Check-first literature sweep

Phase 0 sweep of [[Project — Conservative metric queries without tessellation]] §3, run 2026-08-26 as four parallel web searches (validated numerics and bounding structures; shells, CAD offsets, and normal fields; PDE operators and certified queries; sampling and LoD). Verdict up front: **no claim is dead, and no paper contains our combination — but every individual pillar has close prior art, so each claim narrows, and the paper must be sold as the certified system on displacement maps, not as any single mechanism.** The field is active right now (four adjacent papers from 2025–2026 below), which argues for keeping the schedule tight.

## Verdict by claim

| Claim | Biggest threat | Verdict |
|---|---|---|
| Taylor pyramid (contribution 1) | Hasselgren/Munkberg 2009–2010; Bán & Valasek 2025 | Survives, narrowed |
| Composition rule (§2.3) | TFDM itself; LEADR | Reframe: extension, not new architecture |
| A: emissive sampling | pbrt-v4 textured bilinear-patch emitter | Survives as enabler claim, not mechanism claim |
| B1: texture-space operator | Parametric FEM/IGA; Spira–Kimmel 2004; Williamson & Mitra 2025 | Survives, narrowed |
| B2: certified queries for PWoS | Huang 2025 (SIGGRAPH Asia TC) | Survives on the lfs lower bound; reposition |
| C: LoD stretch/spectral | Cohen 1998; Loukas 2019; Dodziuk 1982 | Survives with citations |
| D: diagnostics | Maggiordomo visibility; line-congruence theory | Survives, narrowed |
| Metric formula (§2.1) | Assemblable from shell theory | Already unclaimed; now correctly cited |

## Contribution 1 — the Taylor pyramid

The node representation (plane + remainder) is classical validated numerics: first-order Taylor models (Berz & Makino), centred forms and slope arithmetic (Krawczyk & Neumaier 1985), affine arithmetic (Comba & Stolfi). In graphics, Heidrich & Seidel 1998 bounded procedural displacement with affine arithmetic; **Hasselgren, Munkberg & Akenine-Möller 2009 ("Automatic Pre-Tessellation Culling") ran displacement shaders in genuine Taylor arithmetic and derived bounds on the displaced normal**; Munkberg et al. 2010 ("Efficient Bounding of Displaced Bézier Patches") combined min-max displacement mipmaps with hierarchical base-surface normal cones (patent US8144147). From the implicit-surface side, **Bán & Valasek 2025 (CGF, "Generalized Lipschitz Tracing") precompute and store per-region directional derivative bounds** of a scalar field from local polynomial fits; segment tracing (Galin et al. 2020) computes local Lipschitz bounds analytically. Cone step mapping stores a conservative slope-like quantity per texel.

So the sentence "nobody bounds the gradient" is falsifiable as stated and must be scoped: *no displacement pyramid stores conservative gradient bounds jointly with height*. What survives, found nowhere: the **joint** value+gradient node sharing one plane (the h–∇h correlation), the exact closed-form bilinear leaf, the conservative mipmap fold, and the propagation to certified first-fundamental-form, area-element, anisotropy, and normal-cone bounds of a displaced surface. Certified area/measure bounds on displaced surfaces appear nowhere (the safest deliverable in the whole project). LEADR (Dupuy et al. 2013) is the statistical ancestor: mipmapped gradient *moments*, base-independent, composed at shading time — position contribution 1 explicitly as "LEADR made certified and geometric."

The composition rule (§2.3) is TFDM's own architecture (base-independent texture statistics composed with per-face geometry at query time; RMIP does the same, with patents). Claim only the extension of that architecture to first-order and metric statistics, and say so before a reviewer does.

## Application A — product sampling

**pbrt-v4's textured bilinear-patch emitter already contains the central structural insight**: it samples a curved parametric patch proportional to an emission texture and gets an exact area PDF by dividing by the pointwise Jacobian — approximate proposal, exact density. Hierarchical pyramid descent with exact PDFs is Clarberg et al. 2005 (wavelet importance sampling) and standard environment-map machinery; Hart et al. 2020 composes warps the same way. The many-light interface (power, bounds, cone per cluster) is exactly Conty Estevez & Kulla 2018.

What survives, confirmed absent from the literature: no many-light or textured-light system handles **emissive displaced (or procedural/implicit) geometry without meshing it first**; none targets E·dA where dA varies and is bounded conservatively; none presents an un-tessellated surface as a native cluster hierarchy. Ling et al. 2025 ("Uniform Sampling of Surfaces by Casting Rays," SGP) is uniform-area only, no emission weighting, samples not independent, no rendering application, displacement never mentioned — cite-only, weaker as a rival than the plan assumed, still the mandatory baseline. One objection to pre-empt in writing: "why exact PDFs at all — use ReSTIR/RIS." Answer: RIS has no closed-form PDF for MIS with BSDF sampling, and its candidate generation itself needs area sampling on the displaced surface.

## Application B1 — texture-space operator

Three layers of prior art. (1) "Operator from the parameterization's pulled-back metric, no remeshing" is classical numerics: parametric surface FEM (Dziuk & Elliott 2013) and isogeometric analysis on surfaces. (2) **Geodesics on a parameter-lattice grid from the metric tensor is 20 years old**: Spira & Kimmel 2004 (fast marching on parametric manifolds), Weber et al. 2008 (geometry-image distance maps on GPU). (3) Recent graphics: Williamson & Mitra 2025 (Spherical Neural Surfaces) build Laplace–Beltrami directly from a neural parameterization without meshing; Liu et al. 2021 own "surface multigrid"; the intrinsic-triangulations line owns "lengths suffice" and Delaunay-flip repairs. Noma et al. 2026 (EG, "Mesh Processing Non-Meshes via Neural Displacement Fields") even owns the framing "geometry processing on displacement fields over coarse base meshes" — but they extract a fine mesh, which is exactly what we avoid; that contrast helps us.

What survives: authored displacement textures as the representation; the texel-lattice intrinsic cotan operator from the closed-form metric; the mip pyramid as the native multigrid; **certified a-priori adaptivity and per-cell validity certificates from conservative bounds** (no prior work certifies its discretization from bounds); the fixed-sparsity edit-latency story; and the observation that the heat method needs only first derivatives. The B1 demo must not be framed as "first geodesics from a parameter-domain metric" — it is not — but as the certified, adaptive, editable version on a representation FEM never touched.

## Application B2 — certified queries for projected walk on spheres

**The sharpest single collision of the sweep: Huang 2025, "Geometric Queries on Closed Implicit Surfaces for Walk on Stars" (SIGGRAPH Asia 2025 Technical Communications)** — certified closest-point, closest-silhouette, and Robin-radius queries via interval branch-and-bound, built precisely to run a WoS-family solver mesh-free on implicit boundaries. That is C5's elevator pitch, published. Spelunking the Deep (Sharp & Jacobson 2022) anticipates the methodology (range analysis → guaranteed closest-point). The numerics community is also hardening PWoS (Hui et al. 2026, modified PWoS with error estimates).

What survives, found nowhere: a **certified lower bound on local feature size** (curvature reach localized by normal cones + hierarchical self-separation via node-pair pruning) — reach literature gives upper bounds (Cotsakis 2023), which is the wrong direction for a conservative solver; the **unbiasedness framing** (conservative pruning as a correctness requirement of the estimator, not an optimization); the surface-PDE (projected) setting rather than volumetric boundaries; and the displacement representation. Confirmed from the PWoS paper: their lfs pipeline is shrinking-ball medial axis ×0.9 with a corner floor, acknowledged heuristic — the gap is real. B2 must be positioned as the PWoS analogue of Huang with the lfs lower bound as the genuinely new ingredient, and it should cite Huang prominently.

## Application C — LoD error

Three pieces have owners: guaranteed LoD bounds exist in screen space for mesh simplification (Cohen, Olano & Manocha 1998, appearance-preserving simplification); provable spectral-coarsening guarantees exist for graphs (Loukas 2019, JMLR); and the eigenvalue sandwich under quasi-isometric metrics is standard spectral geometry (**Dodziuk 1982** — cite it and present the e^{±3δ} bound as a standard Courant–Fischer argument, with our contribution the cheap certified per-region δ; verify the exact exponent against the paper before writing it down). MIPNet (Gauthier et al. 2022) is the precedent for optimized non-box mip construction (appearance-targeted); the prefiltering line (LEADR through Wu et al. 2019 and neural prefiltering 2023) is statistical throughout. What survives: certified per-region *metric* distortion with the area/anisotropy split for never-tessellated displacement mips, under the canonical shared parameterization.

## Application D — diagnostics

**Maggiordomo et al. 2023 already have a named, computed, pre-bake, tilt-adjacent diagnostic driving base-mesh construction** (per-vertex visibility V(v), a Welzl-style max-min-dot), and DJM 2026 adds direction-vs-normal angle thresholds and Jacobian determinant bounds. "First tilt diagnostic" is not defensible. The mathematical content of δ is the classical normal-congruence obstruction (line-congruence theory; Pottmann & Wallner) and appears in vision as gradient-field integrability (Frankot & Chellappa 1988). What survives: sin²θ = aᵀG₀⁻¹a as a *metric-derived, per-point* quantity with exact geometric meaning (vs the combinatorial visibility proxy); the integrability defect applied to displacement direction fields (absent from the entire displacement/micro-mesh literature); the three-determinant separation (at best implicit in Jiang et al. 2020, "Bijective Projection in a Shell"); and the "obliquity is the price of watertightness" framing, which appears open.

## The metric formula (§2.1)

As expected, assemblable from textbook shell mechanics: the shifter gives G₀ − 2hB₀ + h²C₀; Reissner–Mindlin/Cosserat transverse shear *is* the obliquity vector (s = Fᵀd in the coordinate-free shell literature); substituting variable t = h(u,v) produces the coupling terms. No single source states the identity, but a mechanics-literate reviewer can assemble it at review speed. The plan's posture (cite, don't claim) is confirmed correct. Cite: Simo & Fox 1989, Naghdi, a shifter source, Mikkelsen 2010/2020 (bump mapping computes the same ingredients for normals only), Schüssler et al. 2017 (shading normals as impossible geometry).

## Read in full before submission

1. Pottmann 1997 "General offset surfaces" and Chen 2014 "Generalized offset curves and surfaces" — the two inaccessible places most likely to already contain a general-offset first fundamental form.
2. Bán & Valasek 2025 full text — is the Lipschitz field hierarchical?
3. Dodziuk 1982 — verify the sandwich exponent quoted as ours.
4. Hoetzlein 2025 (PDM) full text — validity theory beyond bounds, and the normal-factor reading.
5. Moule & McCool 2002 — from memory only, verify.
6. TFDM's exact LoD selection rule, section by section.

## Overall answer to "do we hold unique novelty?"

Yes, on five specific items found nowhere: (1) the joint (h, ∇h) Taylor node with propagation to certified metric/area/anisotropy/normal-cone bounds; (2) certified area and measure queries on displaced surfaces at all; (3) the certified local-feature-size lower bound; (4) an emissive displaced surface as an unmeshed light-tree cluster hierarchy targeting E·dA; (5) the integrability defect and three-determinant separation as displacement diagnostics. Every surrounding mechanism — Taylor bounds, exact-PDF descent, parameter-domain operators, certified branch-and-bound queries, tilt diagnostics — has close prior art from 1985 through 2026 and must be cited, not claimed. The one-line identity that survives everywhere: *the first certified, conservative query system for the intrinsic geometry of displacement maps.*

Related: [[Project — Conservative metric queries without tessellation]] · [[Log — Conservative metric queries]] · [[Taylor-model bound pyramid]] · [[The induced metric of a displaced surface]] · [[Obliquity and the integrability defect]] · [[Laplace–Beltrami on displaced surfaces]]
