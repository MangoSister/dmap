---
title: Project — Conservative metric queries without tessellation
tags: [project, plan, metric, displacement, bounds, sampling, pde, lod]
status: proposal
created: 2026-08-11
target-venue: SIGGRAPH 2027 (fallbacks — SIGGRAPH Asia 2027, SGP/CGF 2027)
---

# Project — Conservative metric queries without tessellation

**Working title:** *Intrinsic Geometry of Displacement Maps: Conservative Metric Queries without Tessellation.*

## Status

- **Phase:** Phase 1 **complete** ([[Result — Phase 1 Taylor pyramid]]): the Taylor pyramid is implemented in the numpy reference (exact bilinear leaf, conservative fold, certified propagation to metric, area, eigenvalue, and normal-cone bounds; all bounds conservative in ~4 million checks; ablations never tighter).
- **Kill-test outcome:** criterion revised 2026-08-26 after a per-application review (log): conservativeness is absolute (0 violations), and tightness must hold at cells up to 8 texels per side, where the applications read bounds — **met**, worst median ratio 2.77. Coarse-cell tightness steers efficiency only; the two consumers flagged as sensitive for Phase 2 are the normal-cone localization of B2's feature-size bound and LoD drops deeper than about 3 levels. λmin has no meaningful per-node range certificate (structural; its width relative to its value is order 1).
- **Now:** Phase 2 — the three MVPs, the sampling MVP (A) first; the C++ core starts now (§4).
- **Blockers:** none. Six papers flagged "read in full before submission" (§3), not blocking.
- **Last update:** 2026-08-26 · full history in [[Log — Conservative metric queries]]

This block is the only fast-changing part of this note. Edit it in place. The rest of the note is the current plan: when a decision changes, rewrite the affected text in place, and record the reasoning as a log entry. Results get their own notes, linked from here.

> [!abstract] What this note is
> The execution plan for turning the metric thread ([[The induced metric of a displaced surface]] · [[Obliquity and the integrability defect]] · [[Laplace–Beltrami on displaced surfaces]] · [[Taylor-model bound pyramid]]) into a SIGGRAPH-level paper and a research prototype. §2 recaps the technical core; the details stay in the concept notes. §4 records the engineering choices the notes deferred. §5 describes four applications with baselines. §7 orders the work so the cheapest tests run first.
>
> Provenance: an outside review on 2026-08-11 verified the derivations independently; the concept notes were corrected the same day. Projected walk on spheres was promoted to a first-class application (§5 B2) because it is indifferent to seams and fits the certified-query design. Both PDE reference papers were read from their PDFs: [[Crane et al. 2013 — Geodesics in Heat]] and [[Sugimoto et al. 2024 — Projected Walk on Spheres]].

---

## 1. The story

A displacement map stands in for billions of micro-triangles that are never built. [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] renders film-scale detail from 34 MB where pre-tessellation needs gigabytes, updates in microseconds after an edit, and tiles and instances for free. This works because the geometry exists only as a rule, `S = P + hN`, plus a hierarchy of conservative bounds on $h$.

But the representation supports exactly one query. Every structure in this vault (min-max mipmaps, RMIP, prisms, micromaps) answers one question: what does this ray hit. Other questions have no answer:

- How much **area** does this patch have, and where should samples go on it?
- How **far apart** are two points, measured on the surface?
- Which **direction** loses detail first when a mip level drops?
- Can an emissive displaced surface be **importance-sampled**?

Today the only way to answer these is to tessellate, which gives up the memory, the edit latency, and the instancing that made displacement attractive in the first place.

The fix is one derivative. Every existing structure bounds $h$; none bounds $\nabla h$ ([[Min-max mipmap and conservative bounds]]). Bounding $\nabla h$ buys more than area. It buys the **first fundamental form** $\mathbf{G}$, the $2\times 2$ matrix that measures lengths, angles, and areas on the surface. Area is one scalar functional of $\mathbf{G}$. The metric also carries the Laplace–Beltrami operator, and with it geodesics, diffusion, spectra, and anisotropy. None of these can be assembled from area estimates at any sample count. That asymmetry is the thesis: **a scalar can be estimated stochastically (Ling et al. 2025 already do, by casting rays); an operator has to be *constructed*, and construction needs the metric.**

The reason a *hierarchy* of metric bounds is the right structure generalizes the ray-tracing case. In ray tracing the hierarchy answers a predicate: could this ray hit this region? A certain "no" skips the subtree. For the queries above, the hierarchy answers **per-region certificates**: bounds on distance, area, and stretch. Certificates feed three mechanisms:

| Mechanism | Consumer | Ray-tracing analogue |
|---|---|---|
| **Prune** (branch-and-bound) | closest-point queries; certified empty balls for walk-on-spheres solvers | skip missed nodes |
| **Allocate** (proportional descent) | hierarchical CDF sampling; adaptive refinement; certified quadrature | skip empty space |
| **Precondition** (coarse-to-fine) | mip pyramid as multigrid; base mesh as coarsest operator | LoD traversal cutoff |

The closest precedent in kind is *Spelunking the Deep* (Sharp & Jacobson, SIGGRAPH 2022): take a representation the field only knows how to render, apply conservative range analysis, and unlock a family of queries the representation never supported. They did this for neural implicits with interval arithmetic. We do it for displacement maps with a metric-aware bound pyramid. Their queries end at closest-point; ours extend to measure, sampling density, and differential operators, because the displaced surface has a *parameterization* and therefore a metric, which a level set does not.

**Contribution statement** (draft; one line per claim a reviewer sees):

1. A **joint conservative bound structure**, a Taylor-model pyramid over $(h, \nabla h)$ (§2.2), that yields certified per-region bounds on the metric, the area element, and the surface normal cone of an implicit displaced surface. The extra cost over the existing min-max pyramid is a few extra channels (six versus two).
2. A set of **queries** built on it: certified area and integrals; product importance sampling of emissive displaced surfaces; surface PDE solves (a texture-space heat method, and certified queries for pointwise Monte Carlo via projected walk on spheres); and anisotropic LoD error. Each is impossible or unpriced on this representation today (§5).
3. Two **pre-bake diagnostics** for the base mesh, obliquity and the integrability defect, with the corrected shell-validity criterion (§5D, from [[Obliquity and the integrability defect]]).

The pointwise metric formula itself is *not* claimed. It is classical machinery instantiated for this representation, and the paper cites it as such (§3).

---

## 2. Technical core, recapped

Full derivations live in the three concept notes. This section is the paper-sized recap plus the material the notes do not yet have.

### 2.1 The metric (recap; see [[The induced metric of a displaced surface]])

For $S = P + hN$ with $\lVert N\rVert = 1$:

$$
\mathbf{G} \;=\; \underbrace{\mathbf{G}_0 - 2h\,\mathbf{B}_0 + h^2\mathbf{C}_0}_{\mathbf{Q}(h)\ \text{— offset metric}} \;+\; \underbrace{\nabla h\,\nabla h^\top}_{\text{slope}} \;+\; \underbrace{\nabla h\,\mathbf{a}^\top + \mathbf{a}\,\nabla h^\top}_{\text{obliquity coupling}}
$$

This holds unconditionally: no perpendicularity assumption, no integrability assumption. The coefficients $\mathbf{G}_0, \mathbf{B}_0, \mathbf{C}_0, \mathbf{a}$ are pure base geometry, constants or cheap closed forms per flat base triangle. $h$ and $\nabla h$ enter through separate channels. Pointwise evaluation costs a normalization and six dot products, which is cheaper than a ray–box test. Area is $dA = \sqrt{\det\mathbf{G}}\,du\,dv$, with closed-form determinants (a rank-one update lemma when $\mathbf{a}=0$; Sylvester's determinant identity otherwise).

Both corrections from the 2026-08-11 review are now folded into the concept notes: micro-mesh metrics come from vertex positions, not from this formula (the formula and its bounds are for the tessellation-free family), and $\mathbf{G}$ and DJM's shell Jacobian are complementary, neither subsuming the other. The paper inherits the corrected phrasing by construction.

### 2.2 The bound node (recap)

The structure is a **first-order Taylor model** per pyramid node: six numbers $(h_0, g_u, g_v, r, \rho_u, \rho_v)$. They state that over the node's cell, the displacement lies within $r$ of the stored plane, and the gradient lies within $(\rho_u, \rho_v)$ of the plane's slope. The stored plane appears in both the $h$ model and the $\nabla h$ model. That shared appearance preserves the correlation between $h$ and $\nabla h$ that the metric needs. Independently stored min-max gradient channels destroy it: at worst case they let a determinant bound draw the same gradient twice, which triggers false degeneracy alarms exactly in the oblique regime.

The paper gets four properties from the node, one lemma each:

- It **subsumes** the existing min-max channel, so TFDM and RMIP traversal run unchanged. Measured (Phase 1): the recovered interval is 1.06× the exact channel at texel cells but 5–29× near the root, so coarse-level ray traversal should keep the dedicated height channel; the prospective win is the slab bound, thickness $O(s^2)$ where min-max boxes have $O(s)$.
- Leaf construction is **closed-form and exact** for bilinear interpolation.
- The upward **fold is conservative** by the triangle inequality, and costs one mipmap pass.
- Propagation through §2.1 yields certified per-node intervals on the entries of $\mathbf{G}$, on $\sqrt{\det\mathbf{G}}$ (area), on the eigenvalues of $\mathbf{G}_0^{-1}\mathbf{G}$ (anisotropy), and a **normal cone**. The normal cone is a free third consumer; it plugs the representation into many-light samplers (§5A).

Cost: six channels instead of two, with the same pyramid topology.

The full specification lives in **[[Taylor-model bound pyramid]]**: the failure analysis of independent channels, the exact leaf formulas, the fold, the metric propagation, the LoD-semantics caveat against TFDM's adjusted min-max, and the query interactions. This plan carries only the recap.

### 2.3 Composition: texture statistics are base-independent, the metric is not

TFDM's pyramid is instanceable because min-max $h$ is a pure texture property. $\det\mathbf{G}$ is **not**: it depends on $\mathbf{G}_0, \mathbf{B}_0, \mathbf{C}_0, \mathbf{a}$, which vary per base triangle. So the pyramid must store only base-independent statistics of $(h, \nabla h, E)$, and every query **composes** them with the querying triangle's constants on the fly. This is the same approach as TFDM's on-the-fly box generation, applied to metric bounds. Exact node integrals do not compose, because the integrand is nonlinear in the base constants. *Bounds* compose. For sampling, composition error costs only variance, never correctness (§5A). The resulting architecture is two-level: a per-triangle outer structure (each triangle's composed totals) over a shared inner pyramid.

---

## 3. Believed new vs classical

Sweep done 2026-08-26; full findings and must-cite lists in [[Result — Check-first literature sweep]]. Summary: no claim died, every claim narrowed. The paper is the certified *system*; every individual mechanism has close prior art and gets cited.

**Classical; cite generously:** the three fundamental forms, the offset-surface metric and focal surfaces (shell mechanics: $\mathbf{Q}$ is the *shifter*; Simo & Fox 1989 and Naghdi for oblique directors); the height-field metric; the normal-congruence obstruction behind $\delta$ (line-congruence theory; Frankot & Chellappa 1988 in vision); the cotangent Laplacian's intrinsic character and Delaunay-flip repairs (intrinsic triangulations); the heat method; walk on spheres and its surface extension (Sawhney–Crane line; Sugimoto et al. 2024); first-order Taylor models, centred forms, and slope arithmetic (Krawczyk & Neumaier 1985; Berz & Makino); affine arithmetic (Comba & Stolfi); Taylor-arithmetic bounds of displaced patches including displaced-normal bounds (Heidrich & Seidel 1998; Hasselgren et al. 2009; Munkberg et al. 2010); stored local derivative/Lipschitz bounds for implicit marching (Galin et al. 2020; Bán & Valasek 2025); certified range-analysis queries on implicits (Sharp & Jacobson 2022 *Spelunking*; Huang 2025 for walk on stars); operators from a parameterization's pulled-back metric (parametric FEM, Dziuk & Elliott 2013; isogeometric analysis on surfaces); geodesics on parameter-lattice grids from the metric (Spira & Kimmel 2004; Weber et al. 2008); surface multigrid (Liu et al. 2021); hierarchical sample warping with exact PDFs (Clarberg et al. 2005; pbrt-v4's textured bilinear-patch emitter — exact area PDF by pointwise Jacobian); light-tree cluster records (Walter et al. 2005; Conty Estevez & Kulla 2018); mipmapped gradient moments (LEAN/LEADR — the statistical ancestor); guaranteed screen-space LoD (Cohen et al. 1998); spectral-coarsening guarantees on graphs (Loukas 2019); the eigenvalue sandwich under quasi-isometry (Dodziuk 1982; Courant–Fischer); analytic displaced-surface derivatives ([[Nießner & Loop 2013 — Analytic Displacement Mapping]], also the smooth-base escape from the watertightness trilemma).

**Believed new (sharpened by the sweep; found nowhere):**

1. The [[Taylor-model bound pyramid|joint Taylor node]] over $(h,\nabla h)$ sharing one plane, with the exact bilinear leaf, the conservative fold, and propagation to certified metric, area, anisotropy, and normal-cone bounds, composed with an arbitrary base at query time. Not claimed: plane-plus-remainder nodes, gradient bounds per se, or base-independent composition — all prior. Certified area/measure bounds on displaced surfaces have no precedent at all.
2. An emissive displaced surface as an unmeshed light-tree cluster hierarchy, descent targeting $E \cdot dA$ under conservative area bounds (§5A). Not claimed: exact-PDF-by-Jacobian sampling (pbrt-v4) or hierarchical CDF descent (Clarberg).
3. The texel-lattice intrinsic cotangent Laplacian from the closed-form displacement metric, with mip-native multigrid and certified a-priori adaptivity and validity certificates (§5 B1). Not claimed: operators from a parameterization metric, or geodesics on parameter grids.
4. A certified *lower* bound on local feature size (curvature reach localized by normal cones, plus hierarchical self-separation), and conservative pruning framed as an unbiasedness requirement, feeding projected walk on spheres (§5 B2). Not claimed: certified branch-and-bound queries for WoS-family solvers (Huang 2025).
5. Certified per-region metric distortion (area and anisotropy channels) for displacement mip LoD, with the spectral sandwich as a corollary of standard arguments (§5C).
6. Obliquity and $\delta$ as *metric-derived* diagnostics, $\delta$ for displacement direction fields, and the three-determinant separation (§5D). Not claimed: the first tilt diagnostic — Maggiordomo's visibility exists; ours is the per-point quantity with exact geometric meaning.

**Read in full before submission:** Pottmann 1997 and Chen 2014 (general offsets — most likely hiding a general-offset first fundamental form); Bán & Valasek 2025 (is the Lipschitz field hierarchical?); Dodziuk 1982 (verify the exponent); Hoetzlein 2025; Moule & McCool 2002.

---

## 4. Engineering decisions

| Decision | Choice | Why | Crucial? |
|---|---|---|---|
| Parameterization | **Per-face barycentric grids, Ptex-style, shared edge rows** | Kills the seam problem by construction. Turns the thread's named kill-risk into a design sentence, with micro-mesh and Ptex precedent | **Crucial** |
| Gradient source | **Analytic from the interpolant, never stored channels** | Stored $(h, h_u, h_v)$ channels drift mutually inconsistent under filtering; analytic derivatives are exact and free (Nießner & Loop precedent) | **Crucial** |
| Interpolant | **Bilinear** — decided 2026-08-26 ([[Result — Phase 0 numpy reference]], experiment 3: bilinear and biquadratic B-spline differ by under 5% of each other in toy heat-method solve error at every tested resolution, with zero invalid cells for either) | Matches TFDM; keeps the exact Taylor-pyramid leaf construction; the B-spline's $C^1$ advantage did not materialize in operator accuracy. Cheap to revisit if later operator work shows a $C^1$ need | **Decided** |
| Bound node | **Taylor model (§2.2)**, not two min-max channels | It *is* contribution 1; independent channels demonstrably cannot deliver the correlation claim | **Crucial** |
| Prototype substrate | CPU. Numpy reference through Phase 1 (`code/dmapref`), kept as the permanent correctness oracle; C++ core from Phase 2, once the kill-test has frozen the node, with Python bindings and a coarse binding surface (build pyramid, query bounds over a grid, draw N samples, run N walks); PBRT-v4 (or Mitsuba 3) integration **only** for §5A | Phase 1 is array-shaped and needed fast design iteration; Phase 2's per-sample and per-node loops are hostile to Python, while experiment scripts belong in it | **Crucial to keep CPU-first** |
| Test corpus | Maggiordomo dataset (89 assets, shared with DJM) plus 3–4 authored emissive assets (lava or ember crevices, displaced neon) | Comparability with the construction literature; the emissive assets exercise the correlation between $E$ and slope | Important |
| Seam arithmetic | Float-consistent shared edge rows (same values, same order) | Bit-exactness machinery is production polish | Deferrable |
| Pyramid compression, GPU traversal, hardware | None | The paper's claims are memory, capability, and latency, not real-time throughput | Deferrable |
| Vector displacement, animation | Out of scope | Scalar $h$ is where all the structure lives; say so in limitations | Deferrable |
| Arbitrary UV atlas compatibility | Out of scope; per-face storage replaces it | Mortar-style coupling of non-conforming seams is a paper of engineering by itself. The honest sentence is "we adopt per-face parameterization, as Ptex did, for the same reason" | Deferrable, **but state it prominently** |

The first and fifth rows matter most; rows like these are what usually sink such projects. Adopting per-face storage now means no time is ever spent on chart transitions. Staying CPU-first means every experiment runs in minutes.

---

## 5. Applications

Four areas, spanning rendering, geometry processing, content pipeline, and authoring. The PDE area splits into two sibling applications (B1, B2) that share one platform. Rule from the 2026-08-11 review: **this is one paper, not four.** After the Phase 2 gate (§7), A or the B pair becomes the headline, the other becomes a strong section, and C and D are always sections.

### A. Product sampling of emissive displaced surfaces (rendering; headline candidate)

**Claim.** An emissive displaced surface becomes a first-class light source: samples drawn proportional to $E \cdot dA$ with **exact PDFs**, plus per-node power, spatial bounds, and normal cones. The surface presents itself to a Conty–Kulla-style many-light sampler as a native cluster hierarchy, without ever being meshed.

**Mechanism.** Two-level hierarchical CDF descent: a light tree over base faces, pyramid descent within the chosen face, and a uniform draw inside the leaf cell. The essential split: descent weights only steer **variance**; the PDF is **exact** because the pointwise metric is closed-form. Positioning (from the sweep): the approximate-proposal-exact-Jacobian structure is pbrt-v4's textured bilinear-patch emitter, and the descent is Clarberg-style — cite both prominently; the contribution is targeting $E \cdot dA$ under conservative area bounds and plugging the unmeshed surface into a light tree. Pre-empt "just use ReSTIR": resampling has no closed-form PDF for MIS, and its candidate generation itself needs area sampling on this surface.

**The PDF, term by term.** The procedure's density *with respect to surface area* at the sampled point $y = S(u,v)$:

$$
p_A(y) \;=\; \frac{\overbrace{P_{\text{tree}}\textstyle\prod_k P_{c_k}}^{\text{discrete path}}\;\cdot\;\overbrace{1/|\Omega_\ell|}^{\text{leaf uniform}}}{\underbrace{\sqrt{\det\mathbf{G}(u,v)}}_{\text{param}\to\text{area}}}
$$

- $P_{\text{tree}}\prod_k P_{c_k}$ is the probability of the **discrete path**: the light-tree face pick, then $P_c = w(c)/\sum_{\text{siblings}} w(s)$ at each pyramid level. *Any* strictly positive weights give a valid sampler. If the weights were the exact node integrals $W_n = \int_{\Omega_n} E\sqrt{\det\mathbf G}$, the product would telescope to $W_\ell / W_{\text{root}}$.
- $1/|\Omega_\ell|$ is the uniform density **in parameter units** inside the leaf. $|\Omega_\ell|$ is the leaf cell's parameter-domain area clipped to the face's valid domain; cells straddling the face edge are partial.
- $\sqrt{\det\mathbf G(u,v)}$ is the **Jacobian of $S$**: $p_A\,dA = p_{\text{param}}\,du\,dv$ and $dA = \sqrt{\det\mathbf G}\,du\,dv$. It is evaluated pointwise *at the sample* from the full interpolated $(h,\nabla h)$: six dot products, never a node model. This denominator is why weight looseness can never bias the estimator.

Consistency check: exact weights and shrinking leaves give $p_A \to E/W_{\text{root}}$, the normalized target proportional to $E\cdot dA$. For MIS against BSDF samples, convert to solid angle at the shading point as usual: $p_\omega = p_A\,\lVert y - x\rVert^2 / |n_y \cdot \omega|$.

**Baking, and what "base-independent" survives.** The metric depends on per-face constants, so there are exactly two bake regimes. Both are natural under per-face tiles (§4):

- *Statistics bake (base-independent, the default).* The tile pyramid stores only texture-channel statistics: the Taylor nodes of $h$ (§2.2) plus per-node emission statistics ($\bar E$ and $[E_{\min}, E_{\max}]$; a sum pyramid of $E$ is exact at texel granularity). Face constants enter only at query time, through `composedWeight`. This regime survives **sharing**: detail tiles reused across faces, instancing with per-instance displacement amplitude $\alpha$ (substitute $h \to \alpha h$, $\nabla h \to \alpha \nabla h$; analytic), and runtime emission edits (refold the $E$ channels at mipmap cost). Composition error becomes variance only.
- *Fused product bake (per-face, optional).* A Ptex-style tile is bound to its face, so the constants **are** known at bake time: precompute $w_n = \int_{\Omega_n} E\sqrt{\det\mathbf G}$ per node (per-texel quadrature at the finest level, summed upward). These are the tightest possible weights; the correlation between $E$ and slope is captured exactly at every level. The price is base dependence: this regime breaks under per-instance amplitude, non-uniform instance scaling, and emission edits (rebake). Use it for static hero assets. Uniform world scaling multiplies all areas by $s^2$, cancels inside the asset, and only rescales its light-tree total.

```
composedWeight(node n, face T, instance I):      // deterministic ⇒ PDF reconstructible
  (G0, B0, C0, a) ← baseFormsAt(T, centre(n))    // per-face constants, precomputed
  h ← I.α · h0(n);   g ← I.α · g(n)              // node Taylor plane, amplitude-scaled
  G ← G0 − 2h·B0 + h²·C0 + g gᵀ + g aᵀ + a gᵀ    // metric of the node-centre model
  w ← Ē(n) · sqrt(max(det G, 0)) · |Ω(n)|        // midpoint composition
  return max(w, ε · Ē(n) · |Ω(n)|)               // floor: a zero-weight child that still
                                                 //   holds target mass would bias — keep
                                                 //   w > 0 wherever E > 0 (variance only)
  // variants: conservative — interval of √det G from remainders (r, ρ) × [Emin, Emax];
  //           fused bake  — return bakedProductSum(n)   (exact per texel)

SampleEmissive(x_shade):
  T ← sampleLightTree(x_shade)                   // ∝ composed totals × cone × 1/d²
  n ← root(T);   P ← P_tree(T)
  while not leaf(n):
    w[] ← composedWeight(children(n), T, I)
    c ← categorical(w);   P ← P · w[c] / Σw;   n ← c
  (u,v) ← uniformIn(Ω(n) ∩ domain(T))
  y ← S_T(u,v)
  return y, Le(u,v),  P / ( |Ω(n) ∩ domain(T)| · sqrt(det G(u,v)) )

PdfEmissive(y):                                  // MIS: density of a BSDF-sampled hit
  (T, u, v) ← fromHitRecord(y)
  n ← root(T);   P ← P_tree(T)
  while not leaf(n):
    w[] ← composedWeight(children(n), T, I)      // same arithmetic as sampling ⇒
    c ← childContaining(u, v)                    //   identical branch probabilities
    P ← P · w[c] / Σw;   n ← c
  return P / ( |Ω(n) ∩ domain(T)| · sqrt(det G(u,v)) )
```

**Demonstrate:** equal-sample-count variance and equal-time error on the emissive assets; memory and bake time against the meshed alternative; **edit-to-converged latency** after moving a glow crack (a demo no other method can give).
**Baselines:** (1) area-only descent (no $E$), (2) emission-only descent (no $\sqrt{\det\mathbf{G}}$), (3) pre-tessellate plus light BVH (Conty–Kulla), the quality reference, (4) Ling et al. 2025 uniform-area sampling adapted to the traverser, which tests whether the pyramid is needed at all; run it **first**.
**Success:** at least 5–10× variance reduction against (1) and (2) on correlated assets; within about 1.5× the noise of (3) at an order of magnitude less memory; (4) measurably worse or costlier. **Known gap to state:** no visibility term. Every light hierarchy shares this; MIS handles it.

### B. Surface PDEs on displacement maps (geometry processing; headline candidate; two applications)

PDEs on the displaced surface (geodesic distance, diffusion, Poisson) without tessellation, by **two solvers that share the platform and consume it in opposite ways**. B1 needs the hierarchy to be *good*: it optimizes and certifies a solve that would function without it. B2 needs the hierarchy to be *right*: its correctness rests on conservative queries. They fail differently (anisotropy hurts B1, variance hurts B2) and win differently (dense global fields favor B1, sparse point evaluations favor B2). The measured **crossover** between them is itself a contribution-grade figure. Each is the other's fallback; the section survives either one failing alone. The deeper unity: [[Sugimoto et al. 2024 — Projected Walk on Spheres|PWoS §5.2.2]] runs [[Crane et al. 2013 — Geodesics in Heat|the heat method]] *inside* the Monte Carlo solver, so B1 and B2 are two discretizations of one algorithm, on one platform.

#### B1. Texture-space heat method (global fields)

**What the heat method is, originally.** [[Crane et al. 2013 — Geodesics in Heat]] computes geodesic distance without solving the eikonal equation $|\nabla\varphi| = 1$, which is nonlinear and hyperbolic, needs serial fast-marching updates, and admits no prefactorization. The insight: heat diffusing from the source flows *along* geodesics. Its magnitude is numerically hopeless (Varadhan's formula), but its **direction** is robust. So the method uses heat only for direction, and rebuilds the magnitude by integration:

1. one backward-Euler heat step: solve $(\mathbf{M} - t\mathbf{L})\,f = \delta_\gamma$;
2. normalize: $X = -\nabla f / \lVert \nabla f \rVert_{\mathbf G}$, a unit field pointing down the distance gradient;
3. one Poisson step: solve $\mathbf{L}\,\varphi = \nabla\!\cdot\!X$, the scalar field whose gradient best matches $X$. $\varphi$ is the distance.

These are two sparse SPD solves with source-independent matrices. Prefactor once; every new source costs two back-substitutions. The method is **substrate-agnostic by design**: it needs only a Laplacian, a gradient, and a divergence. The original paper runs it on meshes, polygonal surfaces, and point clouds precisely to make that point.

**The gap, and what we contribute.** On a displacement map, the three operators do not exist. That is the whole gap, and the metric fills it. Cotangent weights are intrinsic (they depend on edge lengths only); lengths come from $\ell^2 = e^\top \mathbf{G}\, e$ on the texel lattice; cross-face coupling is exact through shared edge rows (§4); and the heat method needs **only first derivatives of $h$**: no Christoffel symbols, no second-derivative structure ([[Laplace–Beltrami on displaced surfaces]]). On top of the operator, the hierarchy contributes three certified decisions that tessellated pipelines hand-tune:

- **Multigrid from the pyramid.** Each mip node's Taylor plane *is* a coarse-cell metric, so every pyramid level carries its own operator for free. The base-mesh cotangent Laplacian ($\mathbf{G}_0$ constant per triangle) is the coarsest level. Remainder magnitudes bound how far the true fine metric strays from a node's model. That is a certified local homogenization error; it says where coarsening is safe.
- **A priori adaptive resolution.** Descend each region until the metric-bound width falls under tolerance; the resulting mixed-level leaves are the lattice of degrees of freedom. Classical adaptive FEM needs a solve to estimate error. Here the geometry-induced component is certified *before any solve*, from bounds no tessellated mesh carries.
- **Validity certificates.** The two Path-B hazards are metric edge lengths that violate the triangle inequality, and negative cotangent weights (loss of the maximum principle). Both are per-cell conditions on $\mathbf{G}$, certifiable from its bounds: descend until the certificate holds. The largest eigenvalue interval of $\mathbf{G}^{-1}$ per region also gives a certified stable time step for explicit diffusion.

```
// build once per (asset, tolerance τ); reuse across all sources
lattice ← per base face: descend pyramid while
            metricBoundWidth(node) > τ            // certified a-priori adaptivity
            or not valid(node)                    // triangle ineq. + non-obtuse cert.
L, M    ← cotanAssembly(lattice)                  // ℓ² = eᵀ G e; LB note §2, Path B
mg      ← multigrid(coarse ops from node planes; coarsest = base-mesh cotan)

// per source set γ  (heat method, Crane et al. 2013)
f ← mg.solve( (M − t·L) f = δ_γ )                 // I.  one heat step
X ← −∇f / ‖∇f‖_G   per lattice cell               // II. direction only — metric-aware
φ ← mg.solve( L φ = ∇·X )                         // III. closest scalar potential
return φ                                          // a texture beside h: sample anywhere
```

**Demonstrate:** geodesic distance and isolines on an asset whose tessellation would not fit in memory; solve error against lattice resolution; **re-solve latency after a displacement edit**. The latency claim needs its mechanism stated precisely, because the heat method's amortization rests on prefactoring $\mathbf{L}$, and an edit invalidates the factorization. The texel lattice's **sparsity pattern is fixed under displacement edits** (only matrix values change), so the symbolic factorization survives; only the numeric phase, or the multigrid setup, reruns. The tessellated baseline needs remeshing, reassembly, and a full symbolic-and-numeric refactorization. The pyramid rebuild costs one mipmap pass. This is the PDE counterpart of TFDM's four-orders-of-magnitude edit win, and the latency table should report the numeric-refactorization split explicitly. Also: diffusion-based decal spreading.
**Baselines:** (1) cotangent Laplacian on pre-tessellation at matched resolution (accuracy reference; 4× resolution serves as ground truth), (2) [[Laplace–Beltrami on displaced surfaces|Path A]] on a micro-mesh, the honest cheap alternative the operator note itself names, (3) intrinsic triangulations on a coarse extraction.
**Success:** within 1–2% of (1) at 10× or more memory reduction; edit-to-answer under a second where (1) needs minutes of remeshing and rebuilding.
**Risks:** negative weights and triangle-inequality failures under strong anisotropy. The certificates above turn these from silent wrong answers into refinement triggers, but pathological content could force refinement to the finest level. The method then degrades to uniform resolution; it never lies.

#### B2. Projected walk on spheres (pointwise queries)

**What PWoS is, originally.** [[Sugimoto et al. 2024 — Projected Walk on Spheres]] solves surface Poisson problems ($\Delta_{\mathcal S} f = f_{\mathcal S}$, with Dirichlet data $g$ on curves $C$) **pointwise and without a discretization**: no meshing, no global system, evaluation only where the answer is wanted (their headline figure evaluates diffusion curves only at visible pixels). The mechanism is the **closest-point extension**: extend $f$ to be constant along normals in a tube around the surface. Inside the tube, the surface Laplacian becomes the ordinary ambient $\Delta$, so classic walk on spheres applies with one change: **project every step back to the surface**.

- At $x$: step radius $r = \min(\mathrm{lfs}(x),\ d_C(x))$, staying inside the tube and away from the boundary.
- Sample $y$ uniformly on the 3D sphere of radius $r$; accumulate the source term from ball samples, projected through $\mathrm{cp}_{\mathcal S}$.
- Recurse at $\mathrm{cp}_{\mathcal S}(y)$; within $\varepsilon$ of the boundary, read $g$ at the closest boundary point.

The estimator is unbiased at $O(1/\sqrt{N_P})$ over independent walks (up to the paper's stated $g\!=\!0$ extension caveat), embarrassingly parallel, and output-sensitive.

**Its two geometric dependencies, and how the original paper meets them.** (a) A closest-point query per step: their implementation calls Houdini's *mesh* closest point, so in practice the geometry is discretized after all. "Discretization-free" describes the solver, not the geometry access. (b) A **conservative lower bound on local feature size**, so spheres stay inside the valid tube. Their §3.1 builds it by preprocessing: a medial-axis point cloud via shrinking balls, scale-axis-style pruning, a ×0.9 safety factor, and a corner clamp. That is *heuristic* conservatism, and the paper documents both failure directions. An estimate that is too small is valid but slow: in their Fig. 3, average walk length grows from **31 to 1819 steps** as the local-feature-size estimate shrinks from 0.99 to 0.0625. Aggressive pruning produces **visible residual bias** (their Fig. 4c). The correctness-critical quantity is estimated, not certified.

**What we contribute.** The platform replaces both dependencies with certified versions, and makes the method run on a representation it cannot touch today. Positioning (from the sweep): Huang 2025 (SIGGRAPH Asia Technical Communications) already certifies closest-point and silhouette queries by interval branch-and-bound to run walk on stars on implicit boundaries — cite it prominently; our genuinely new ingredient is the certified local-feature-size *lower* bound and the unbiasedness framing, in the surface-PDE setting, on displacement maps:

- **A certified closest-point query** on the implicit displaced surface: branch-and-bound over on-the-fly node slabs, with prism inversion for the parametric coordinates of the projection. Conservative pruning is a matter of *unbiasedness* here: a non-conservative bound can discard the node holding the true closest point, and a wrong projection biases the walk silently. Today PWoS cannot run on a TFDM asset at all; there is no closest-point query to call without tessellating first.
- **Certified local feature size** $= \min(\text{curvature reach},\ \tfrac12\,\text{self-separation})$. The local half comes from the per-triangle curvature scalar, *localized* (enlarged where it matters) by per-node normal cones. The global half, how close a distant sheet approaches, comes from hierarchical node-pair pruning, which **no local or pointwise estimate can see**. This replaces the medial-axis preprocessing outright. Their Fig. 3 sensitivity is the argument for making the certified bound *tight*, which the cone localization addresses. Underestimation only slows the walk; nothing in the pipeline can bias it.
- **A certified boundary distance** $d_C$ on the safe side, for boundary curves authored in texture space.
- **Source and boundary sampling** proportional to area via §5A's machinery, with exact PDFs.
- Structural bonuses inherited from the pointwise character: the method is **seam-indifferent** (walks live in 3D; see the pointwise-exception callout in [[Laplace–Beltrami on displaced surfaces]]), needs **zero rebuild on edit**, and its cost is independent of surface size.

```
// estimate f(x₀) for Δ_S f = f_src, f = g on curves C   (adapted from PWoS Alg. 1)
walk(x):
  dC ← certifiedBoundaryDist(x)                  // pyramid lower bound — safe side
  if dC < ε:  return g( cp_C(x) )
  lfs ← min( curvatureReach(tri(x)) ⊕ coneLocalize(node(x)),   // local half
             ½ · selfSeparation(x) )             // global half: node-pair B&B
  r  ← min(lfs, dC)
  y  ← uniformOnSphere(x, r)
  src ← Σᵢ G(x, zᵢ) · f_src(cp_S(zᵢ)) / p(zᵢ)    // ball samples, projected;
                                                 // drawn ∝ area via §5A bounds
  return walk( cp_S(y) ) + src                   // cp_S: certified B&B over slabs
f̂(x₀) ← mean of N_P independent walks            // O(1/√N_P); no grid anywhere
```

**Demonstrate:** cost against accuracy for solution-at-a-point, on the same untessellatable asset as B1; walk-length statistics under the certified local-feature-size bound against a deliberately loosened one (reproducing their Fig. 3 sensitivity on our terms); view-dependent evaluation (their diffusion-curves demo) on a displaced surface; zero-rebuild edits.
**Baselines:** (4) PWoS on the tessellated mesh with a BVH closest point and their medial-axis pipeline, which isolates exactly what certification and the representation buy; (5) B1 at matched accuracy, for the crossover figure.
**Success:** matching the accuracy of (4) with the memory advantage and no preprocessing pipeline; the crossover against B1 mapped as sweeps over query density.
**Risks:** Monte Carlo variance; restriction to Poisson-type problems with Dirichlet data, and the $g = 0$ extension bias for complex sources (both stated by the original paper; the scope is inherited, not fought); a loose curvature scalar shrinking the tube and slowing walks (degrades, never biases).

### C. Anisotropic and spectral LoD error (content pipeline; always a section)

**Claim.** Every mip level of a displacement map shares one parameterization, so the correspondence between fine and coarse is *canonical*. Mesh-decimation LoD never has this. The question "what does level $k$ destroy?" becomes the distortion of that map: the stretch tensor $\mathbf{D} = \mathbf{G}^{-1/2}\mathbf{G}_k\mathbf{G}^{-1/2}$, a $2\times2$ closed form whose logarithm splits exactly into an **area** channel and an **anisotropy** channel, $\varepsilon^2 = 2(a^2 + s^2)$. Three results ride on it (full derivation: [[Anisotropic and spectral LoD error]]):

- **Certified selection.** Per level-$k$ cell, the coarse surface's Taylor model is *exact* (one bilinear patch), while the folded pyramid node bounds the fine surface. So the pyramid certifies $[\varepsilon_{\text{lo}}, \varepsilon_{\text{hi}}]$ per region, and LoD selection returns a **level map** with a guarantee ("no point exceeds the budget"), not a sampled heuristic. Height error stays as the *position* budget; $\varepsilon$ is the *geometry* budget. The two are complementary, and the ripple-smoothing case (height error $\delta$, slope error $\omega\delta$) is where the difference is an order of magnitude.
- **The spectral certificate.** A certified stretch bound $\delta$ sandwiches every Laplace–Beltrami eigenvalue: $\lambda_i^{(k)}/\lambda_i \in [e^{-3\delta}, e^{3\delta}]$, via Courant–Fischer. Spectrum preservation is guaranteed without computing a spectrum. By conformal invariance in 2D, the **anisotropy channel is the only spectrally dangerous one**: "anisotropic" and "spectral" are one claim, seen twice.
- **Metric-aware mips.** Coarse levels can be *fit* rather than box-filtered. An $H^1$ (gradient-preserving) fit is linear, tile-local, and preserves to first order exactly the slope term that box filtering destroys. A full metric fit (Gauss–Newton on $\Sigma\varepsilon^2$) is the offline reference.

**Demonstrate:** rank correlation of $\varepsilon$, height error, and determinant-only error against rendered-image error (FLIP/ΔE) across LoD switches; the certified level map on a full asset; $H^1$-fit mips against box mips at equal memory (both certified $\varepsilon$ and FLIP); an eigenvalue computation on a small asset validating the $e^{\pm3\delta}$ sandwich empirically; the anisotropy direction picking the mip-bias axis.
**Baselines:** TFDM's height-error LoD; determinant-only error (DJM-flavored); Hausdorff distance on tessellated geometry; box-filtered mips (for the constructor).
**Success:** strictly better rank correlation with image error at negligible extra cost; a measurable gap between $\varepsilon$ and FLIP for the $H^1$ mips. Small, clean, and quantitative.

### D. Pre-bake base-mesh diagnostics (authoring; always a section)

**Claim.** Obliquity $\sin^2\theta = \mathbf{a}^\top\mathbf{G}_0^{-1}\mathbf{a}$ and the integrability defect $\delta$ are per-triangle, pre-bake, dot-product-cheap predictors of metric distortion that no existing constructor measures, [[Zhang et al. 2026 — DJM|DJM]] and Maggiordomo included. Visibility tests admissibility, not distortion ([[Obliquity and the integrability defect]]).
**Demonstrate:** heatmaps at decimation time; correlation of the diagnostics with post-bake reconstruction error across the Maggiordomo corpus; a case that passes the visibility test with high obliquity and visible artifacts; the corrected shell criterion (three determinants) against $h < 1/\kappa_{\max}$.
**Baselines:** Maggiordomo's visibility value; DJM's $\det J$.
**Effort:** small; the highest insight per hour of any section in the paper.

---

## 6. Limitations and non-goals

State these in the paper. Decided now:

- **Scalar displacement only.** Vector displacement has no height-field structure to bound. Non-goal.
- **The metric is that of the smooth interpolated surface.** Renderer leaf geometry (local triangulation, bilinear patches) differs at the leaf scale. Quantify the discrepancy once (Phase 0) and state it. It is not fixable, only priceable.
- **No visibility term in sampling** (§5A). Inherited from all light hierarchies; MIS mitigates it.
- **Obliquity pushes $\mathbf{G}$ toward indefiniteness:** $\det\mathbf{G}$ can vanish before the focal surface. We diagnose this (§5D) rather than repair it. Self-intersecting displacement has ill-defined area; it is counted by multiplicity, and the paper says so.
- **No real-time claim, no GPU traversal, no compression, no hardware story.** The claims are capability, memory, and edit latency. The DMM withdrawal ([[Graphics API and hardware support timeline]]) makes software-first the defensible posture anyway.
- **No arbitrary-atlas compatibility.** Per-face parameterization is the representation, as in Ptex. Converting authored atlas assets is a resampling preprocess, demonstrated once, not optimized.
- **No second-derivative pyramid** (per-node curvature bounds, Christoffel marching, spectral computations at scale): future work. Application B2 needs only a single conservative curvature scalar per base triangle for its tube radius; the heat method (B1) needs no second derivatives at all.

**Pre-empted reviewer attacks** (each gets a sentence in the paper): *"Just tessellate"* — answered by the memory, edit-latency, and instancing tables, plus the LoD-bias point that enumeration changes area silently. *"The formula is classical"* — agreed and cited (shell theory, Nießner & Loop); the contribution is the certified bound structure and the queries. *"Ling et al. already sample"* — uniform is not product; they have no localization, no certificates, and no PDFs with respect to a target density. *"LEADR already mipmapped gradients"* — moments, not bounds; appearance, not measure.

---

## 7. Execution plan

Bias throughout: **cheapest kill-test first, nothing polished before the gate.** The only external anchor is the SIGGRAPH deadline (late January 2027); the gate decides whether that target or the fallback is real.

### Phase 0 — harness and hygiene

- Python/numpy reference: pointwise $\mathbf{G}$ against finite differences and against dense-tessellation Gram matrices. The sanity checks from the notes (face-normal case: area independent of $h$; sphere: $\delta = 0$) become unit tests.
- Decide the interpolant (bilinear vs B-spline) by measuring the impact of gradient discontinuities on a toy cotangent-Laplacian solve.
- Quantify the smooth-vs-rendered-surface discrepancy (limitation 2) once, on two assets.
- ~~Note hygiene~~ **done 2026-08-11**: the three concept-note overclaims are fixed, [[Nießner & Loop 2013 — Analytic Displacement Mapping]] is in `40 Reference/`, and the bound structure has its own spec in [[Taylor-model bound pyramid]].
- Launch the check-first literature sweeps (§3) in parallel; they cost reading time, not build time.

### Phase 1 — the Taylor pyramid, and its kill-test

- Implement §2.2: leaf construction from the interpolant, the conservative fold, and affine propagation through §2.1 to per-node bounds on the entries of $\mathbf{G}$, on $\sqrt{\det\mathbf{G}}$, on the stretch eigenvalues, and on the normal cone.
- **Tightness study** across the Maggiordomo corpus: bound width against true range, per level. Ablations: the Taylor node against (a) two independent min-max gradient channels, (b) interval arithmetic, (c) TFDM-style plain affine; min-max $h$ recovered from the Taylor node against the adjusted min-max recursion.
- > [!danger] Kill criterion — revised 2026-08-26 after the Phase 1 study (reasoning in the log)
  > Conservativeness is absolute: any bound violation kills the structure. The tightness requirement is scoped to the cells whose bounds the applications read for their answers: certified metric bounds must be within 3× of the true range at cells up to 8 texels per side, on typical (not adversarial) content. Coarser cells only steer efficiency — sampling descent allocation (variance, §5A), refinement depth (§5 B1), walk length and pruning (§5 B2), LoD drops deeper than about 3 levels (§5C) — and are priced per consumer in Phase 2. Outcome: **met** — 0 violations in ~4 million checks, worst median ratio 2.77 at criterion cells ([[Result — Phase 1 Taylor pyramid]]).

### Phase 2 — three MVPs and the gate

Order (revised 2026-08-26: the sampling MVP moved to the front, ahead of the cheaper diagnostics):

1. **A-MVP first**: single asset, CPU path tracer or PBRT hook; area-only descent against product descent against the **Ling et al. baseline, run first**. That baseline is the vault's own cheap test of whether the sampling story needs the pyramid at all.
2. **D**: diagnostics over the corpus, correlation numbers. A guaranteed section regardless of what else survives.
3. **B1-MVP**: heat method on one face; then cross-face coupling via shared edge rows; then mip multigrid. Measure against pre-tessellated cotangent Laplacian.
4. **B2 feasibility spike** (**unconditional**; promoted from contingent on 2026-08-11 after reading the PWoS paper — its documented local-feature-size pain point and the enabling claim give B2 the strongest contribution structure in the project, so the gate must not decide without pricing it): branch-and-bound closest point over the pyramid, prism-inversion projection, and a fixed conservative tube on one asset. Enough to *price* PWoS before the gate, not to polish it.

> [!important] The gate — end of Phase 2
> Pick the headline (A or the B pair) on evidence: which MVP has the larger measured win and the more legible demo. Lock the paper skeleton: headline, plus the second application as a strong section, plus C and D. Anything not in the skeleton stops. If **both** MVPs disappoint but the bounds are tight, the fallback paper is "certified metric bounds + LoD + diagnostics", aimed at SGP/CGF. Decide that at the gate too, not at the deadline.

### Phase 3 — depth on the skeleton

- Headline application to full evaluation: all baselines from §5, the full corpus, memory/latency/variance tables, and the flagship demo (the emissive edit loop, or the untessellatable-asset geodesic). If the B pair is the headline, both B1 and B2 go to depth, including the crossover figure. If A is, B2 still ships as the certified-query demonstration section, budget permitting.
- C's FLIP correlation study across LoD, the comparison of $H^1$ against box-filtered mips, and the small-asset eigenvalue computation validating the spectral sandwich. All of this is mechanical and scriptable.
- Document failure modes as they happen: negative-weight cells, bound blowups. These become the limitations section, with numbers instead of hedges.

### Phase 4 — writing (overlaps Phase 3)

- Derivations and conservativeness lemmas go to the supplemental. §1's story is the intro draft. Figures come from the Phase 2–3 harness, never bespoke.
- One full internal review pass against the §6 attack list before the deadline.
- Venue call: SIGGRAPH 2027 if the headline demo lands; otherwise SIGGRAPH Asia 2027 (later deadline, around May) or SGP for the fallback scope. This is a deliberate decision at the gate, not a scramble at the deadline.

---

Related: [[Log — Conservative metric queries]] · [[The induced metric of a displaced surface]] · [[Obliquity and the integrability defect]] · [[Laplace–Beltrami on displaced surfaces]] · [[Min-max mipmap and conservative bounds]] · [[Surface measure and sampling on implicit displaced surfaces]] · [[MOC — Open Questions]] · [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] · [[Zhang et al. 2026 — DJM]] · [[Shell, prism and prismoid]] · [[Watertightness and cracks]]
