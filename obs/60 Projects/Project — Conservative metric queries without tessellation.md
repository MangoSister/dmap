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
- **Kill-test outcome:** criterion revised 2026-08-26 after a per-application review (log): conservativeness is absolute (0 violations), and tightness must hold at cells up to 8 texels per side, where the applications read bounds — **met**, worst median ratio 2.77. Coarse-cell tightness steers efficiency only; the two consumers flagged as sensitive for Phase 2 are the normal-cone localization of B2's feature-size bound and LoD drops deeper than about 3 levels. λmin has no meaningful per-node range certificate (structural; its width relative to its value is order 1). Measured under the identity chart; under general charts ([[Result — Chart dependence study]], 2026-09-06) conservativeness holds with 0 violations, √det G and λmax stay within 4%, and the normal cone loosens 19% under shear (3.09 at 8-texel cells, 3% over the kill ratio), with the frame fix recorded there.
- **Now:** Phase 2 — the sampling MVP (A) is **complete** ([[Plan — A-MVP sampling implementation]], all phases S0–S8): C++ core on ks, full baseline ladder measured ([[Result — A-MVP receiver irradiance study]]), rendered images with a passing MIS check ([[Result — A-MVP rendered images]]). Measured shape: far-field parity with the fused product table, near-field and occluded wins for receiver-aware descent, structural wins on edits and reuse, a ~27× per-footprint memory concession. The node grew a dedicated min-max height channel on 2026-08-30 (§2.2, eight channels) so that ray traversal is at parity with TFDM and RMIP by construction, and a second construction that builds each level by direct enumeration instead of folding; both are implemented and validated in the C++ core ([[Result — Pyramid channel study]] Part 3). What remains of §7's ray item is the traversal measurement itself, which has never been run; its implementation plan, together with the path tracer and scene format the demos need, was written on 2026-09-06 ([[Plan — Path tracer with displaced surfaces]], phases T1–T10, the measurement is T5). General charts were adopted on 2026-09-06 for the ray and sampling queries (§4, [[Result — Chart dependence study]]) and are implemented in the C++ core as of 2026-09-07 (T1 of the path tracer plan); T2, the displacement asset and the scene specification that every later phase reads, T3, the per-node box and slab tests ([[Result — Node bound tightness]]: conservative, the slab 2 to 4× shorter than the box at the leaves), T4, the traversal with a certified Newton leaf, verified against embree over pre-tessellated meshes, and T5, the traversal cost measurement ([[Result — Traversal cost study]]: both never tests more nodes than TFDM's box, the slab pays at the leaves, the walk 15 to 27× slower per ray than embree over the mesh on one thread in double and 20 to 36× smaller in memory) are done the same day, and so are T6, the displaced surface as embree user geometry inside ks's scene, validated against the standalone intersector and against pre-tessellated substitutes on the five scenes, T7, the ks interface changes for displaced lights behind a bit-identical regression gate, and T8, displaced surfaces as ks area lights inside ks's path tracer (`path_trace`), verified by the regression gate, by the MIS check on the two-emitter scene S6 (which also found and fixed a MIS weight bias in ks's next-event estimation) and against pre-tessellated substitutes on S4, S6 and S7, which exposed and fixed a rounding defect of the traversal's combined node test on perfectly flat cells and two defects of ks's textured mesh lights, and T9, the emitter sampling ladder inside those scenes ([[Result — Emitter sampling in scenes]]: every sampler unbiased; on the S6 panels the A-MVP's ordering holds with the product table first, but on the sci-fi step map the midpoint product descent trails the table 8× at 64 samples per pixel because its per-level √det G composition puts its density at 0.1× to 5.6× the target, the case the moment channels of the A-MVP open decisions are meant to fix; on the 4,416-triangle station ring every kind is within 25% under MIS, since ks's power-based selection among triangles, not the sampler within one, decides the variance); the layout for the PDE applications is deferred. The object-level light sampler, one light per displaced object with a receiver-aware descent from the object's root, is recorded as the next direction for A (§5A, 2026-09-08), outside the current plans. Then the B/C MVPs or the gate discussion (§7).
- **Blockers:** none. Six papers flagged "read in full before submission" (§3), not blocking.
- **Last update:** 2026-09-07 · full history in [[Log — Conservative metric queries]]

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

1. A **joint conservative bound structure**, a Taylor-model pyramid over $(h, \nabla h)$ alongside the min-max height channel it does not replace (§2.2), that yields certified per-region bounds on the metric, the area element, and the surface normal cone of an implicit displaced surface, while leaving the existing ray query at parity. The extra cost over the existing min-max pyramid is a few extra channels (eight versus two).
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

The structure is a **first-order Taylor model** per pyramid node, $(h_0, g_u, g_v, r, \rho_u, \rho_v)$, plus a dedicated min-max height channel $(h_{\min}, h_{\max})$: eight numbers. The Taylor part states that over the node's cell, the displacement lies within $r$ of the stored plane, and the gradient lies within $(\rho_u, \rho_v)$ of the plane's slope. The stored plane appears in both the $h$ model and the $\nabla h$ model. That shared appearance preserves the correlation between $h$ and $\nabla h$ that the metric needs. Independently stored min-max gradient channels destroy it: at worst case they let a determinant bound draw the same gradient twice, which triggers false degeneracy alarms exactly in the oblique regime.

The two height channels are not redundant, and the reason is the shape of the query, not tightness ([[Result — Pyramid channel study]]). Collapsing the Taylor node to a scalar height interval costs the plane's excursion across the cell, so as an *interval* it is always worse than a stored min-max — 1.06× at texel cells but 5–29× near the root, where a traversal starts. As a *region in space* the slab is thinner, $2r = O(s^2)$ against an axis-aligned $O(s)$, which is what a ray test or a distance lower bound consumes. So min-max serves coarse traversal and the LoD position budget; the slab serves fine traversal, closest point, and feature size. Storing both makes coarse-level traversal identical to TFDM and RMIP by construction rather than by measurement.

The channels also differ by **fold law**, which is what decides where each is tight. Min-max folds by min and max, and the range of a union is the union of ranges, so it is exact at every level. The Taylor slab folds a plane plus a remainder, so a child's slack becomes part of the parent's remainder and compounds — measured at 1.0× at leaves rising to 2.4× at coarse cells. That compounding is tolerable precisely because the slab's value is a fine-level property. Building levels directly from the texel grid instead of by folding removes the compounding exactly (the residual's extremes sit at texel nodes, so enumeration is exact, not sampled) at a build cost of $N^2\log N$ against $\tfrac43 N^2$; it is a recorded option for the slab, not a substitute for the min-max channel.

The paper gets four properties from the node, one lemma each:

- It **carries the existing min-max channel**, so TFDM and RMIP traversal run unchanged and at parity. The Taylor part alone only *recovers* that channel, and loosely at coarse cells (above), which is why the channel is stored rather than derived; the prospective win over TFDM is the slab bound, thickness $O(s^2)$ where min-max boxes have $O(s)$.
- Leaf construction is **closed-form and exact** for bilinear interpolation.
- The upward **fold is conservative** by the triangle inequality, and costs one mipmap pass.
- Propagation through §2.1 yields certified per-node intervals on the entries of $\mathbf{G}$, on $\sqrt{\det\mathbf{G}}$ (area), on the eigenvalues of $\mathbf{G}_0^{-1}\mathbf{G}$ (anisotropy), and a **normal cone**. The normal cone is a free third consumer; it plugs the representation into many-light samplers (§5A).

Cost: eight channels instead of two, with the same pyramid topology. Two of the eight are the min-max channel any traversal already stores, so the increment over TFDM's structure is six.

The full specification lives in **[[Taylor-model bound pyramid]]**: the failure analysis of independent channels, the exact leaf formulas, the fold, the metric propagation, the LoD-semantics caveat against TFDM's adjusted min-max, and the query interactions. This plan carries only the recap.

### 2.3 Composition: texture statistics are base-independent, the metric is not

TFDM's pyramid is instanceable because min-max $h$ is a pure texture property. $\det\mathbf{G}$ is **not**: it depends on $\mathbf{G}_0, \mathbf{B}_0, \mathbf{C}_0, \mathbf{a}$, which vary per base triangle. So the pyramid must store only base-independent statistics of $(h, \nabla h, E)$, and every query **composes** them with the querying triangle's constants on the fly: its base forms and its chart, the one affine map from texture coordinates to the triangle (§4). This is the same approach as TFDM's on-the-fly box generation, applied to metric bounds. Exact node integrals do not compose, because the integrand is nonlinear in the base constants. *Bounds* compose. For sampling, composition error costs only variance, never correctness (§5A). The resulting architecture is two-level: a per-triangle outer structure (each triangle's composed totals) over a shared inner pyramid.

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
| Parameterization | **A general affine chart per triangle** (decided 2026-09-06): per-vertex texture coordinates into a shared tile, one constant map from texture coordinates to the triangle. Per-face barycentric grids, Ptex-style with shared edge rows, are the special case where the chart is the identity, and stay available | The bounds, the metric queries, and the samplers are covariant under the chart, derived and measured ([[Result — Chart dependence study]]: 0 violations under every chart, metric quantities within 4%, the normal cone 19% looser under shear). So authored UV-atlas assets are supported natively by the ray and sampling queries, as in TFDM, and nothing is lost, since the general chart contains the per-face layout | **Crucial** |
| Gradient source | **Analytic from the interpolant, never stored channels** | Stored $(h, h_u, h_v)$ channels drift mutually inconsistent under filtering; analytic derivatives are exact and free (Nießner & Loop precedent) | **Crucial** |
| Interpolant | **Bilinear** — decided 2026-08-26 ([[Result — Phase 0 numpy reference]], experiment 3: bilinear and biquadratic B-spline differ by under 5% of each other in toy heat-method solve error at every tested resolution, with zero invalid cells for either) | Matches TFDM; keeps the exact Taylor-pyramid leaf construction; the B-spline's $C^1$ advantage did not materialize in operator accuracy. Cheap to revisit if later operator work shows a $C^1$ need | **Decided** |
| Bound node | **Taylor model (§2.2)**, not two min-max channels | It *is* contribution 1; independent channels demonstrably cannot deliver the correlation claim | **Crucial** |
| Prototype substrate | CPU. Numpy reference through Phase 1 (`code/python/poc/dmapref`), kept as the permanent correctness oracle; from Phase 2, C++ on the user's research codebase ks (`code/cpp`, ks as a submodule; embree CPU ray tracing, task-based toml config, decided 2026-08-27 — see the log). Baselines are ported into the codebase rather than integrated into external renderers: the Ling et al. official sampler and PBRT-v4's textured bilinear-patch sampling (both vendored as submodules in `code/third-party` for reference). Python bindings only if experiment scripting needs them | Phase 1 is array-shaped and needed fast design iteration; Phase 2's per-sample and per-node loops are hostile to Python; ks is forward-looking (the project's eventual home) and already has the renderer infrastructure (embree, lights, MIS path tracer) | **Crucial to keep CPU-first** |
| Test corpus | Maggiordomo dataset (89 assets, shared with DJM) plus 3–4 authored emissive assets (lava or ember crevices, displaced neon) | Comparability with the construction literature; the emissive assets exercise the correlation between $E$ and slope | Important |
| Seam arithmetic | Float-consistent shared edge rows (same values, same order) | Bit-exactness machinery is production polish | Deferrable |
| Pyramid compression, GPU traversal, hardware | None | The paper's claims are memory, capability, and latency, not real-time throughput | Deferrable |
| Vector displacement, animation | Out of scope | Scalar $h$ is where all the structure lives; say so in limitations | Deferrable |
| Arbitrary UV atlas compatibility | Native for the ray and sampling queries (2026-09-06). For the PDE applications the layout question is **deferred**, not decided: per-face tiles remove the seam problem by construction, an atlas brings it back at chart boundaries, and supporting the general chart keeps both layouts available until that work starts | Under an atlas, nodes and faces are independent partitions, so base-dependent per-node quantities become per (node, face); bookkeeping, not validity. Cross-seam coupling for B1 and crack-free geometry for B2 are the unsolved parts; mortar-style coupling of non-conforming seams is a paper of engineering by itself | Decided for rendering; **deferred for PDEs, state it prominently** |

The first and fifth rows matter most; rows like these are what usually sink such projects. Supporting the general chart now means the rendering demos take authored assets as they are, and the per-face layout is still there for whatever the PDE work turns out to need. Staying CPU-first means every experiment runs in minutes.

---

## 5. Applications

Four areas, spanning rendering, geometry processing, content pipeline, and authoring. The PDE area splits into two sibling applications (B1, B2) that share one platform. Rule from the 2026-08-11 review: **this is one paper, not four.** After the Phase 2 gate (§7), A or the B pair becomes the headline, the other becomes a strong section, and C and D are always sections.

### A. Product sampling of emissive displaced surfaces (rendering; headline candidate)

**Claim.** An emissive displaced surface becomes a first-class light source: samples drawn proportional to $E \cdot dA$ with **exact PDFs**, plus per-node power, spatial bounds, and normal cones. The surface presents itself to a Conty–Kulla-style many-light sampler as a native cluster hierarchy, without ever being meshed.

**Mechanism.** Two-level hierarchical CDF descent: a light tree over base faces, pyramid descent within the chosen face, and a uniform draw inside the leaf cell. The essential split: descent weights only steer **variance**; the PDF is **exact** because the pointwise metric is closed-form. Positioning (from the sweep): the approximate-proposal-exact-Jacobian structure is pbrt-v4's textured bilinear-patch emitter, and the descent is Clarberg-style — cite both prominently; the contribution is targeting $E \cdot dA$ under conservative area bounds and plugging the unmeshed surface into a light tree. Pre-empt "just use ReSTIR": resampling has no closed-form PDF for MIS, and its candidate generation itself needs area sampling on this surface.

**Receiver-aware descent (analysis 2026-08-28, reasoning in the log).** Because the PDF is the product of the branch probabilities actually used, the weights may depend on the shading point — the true target per cell is $\int E \cos\theta_s\,|n_y\cdot\omega|/r^2\,dA$, and every factor has a per-node estimate computed on the fly: $\bar E$ from the emission mip, midpoint $\sqrt{\det\mathbf G}$, distance and receiver cosine from the cell's spatial bound (sub-triangle ⊕ certified height interval along the normal range), emitter cosine from the normal cone. Two conditions keep it valid: weights must be deterministic functions of (node, receiver) so `PdfEmissive` reconstructs them, and probabilities stay positive wherever the contribution can be positive. This is Conty–Kulla traversal quality *within* a face, with cluster bounds derived from the node instead of baked. Two sharp caveats: certified cones saturate at $\pi$ above ~8–16-texel cells on rough content (Phase 1), so the cosine factor discriminates only at fine levels; and this is not exact solid-angle sampling — the geometry factors are flattened only down to their residual variation within a leaf. Measured (A-MVP S7, [[Result — A-MVP receiver irradiance study]]): a point estimate of the emitter cosine from the midpoint normal is worse than omitting the factor entirely (heavy tails from misweighted rough cells); until a cone-aware bound exists, the receiver term is $\cos\theta_r^+/r^2$ only, and with that form the near-field and occluded-grazing wins materialize. **Certified-zero pruning:** probability zero for a subtree is unbiased only with a proof that it contributes nothing — a certified spatial bound fully below the receiver's tangent plane, or a certified cone fully backfacing on a one-sided emitter. Midpoint estimates cannot justify this; certified bounds can. This is a role conservativeness plays inside the sampler itself and requires the certified propagation in the C++ core.

**The PDF, term by term.** The procedure's density *with respect to surface area* at the sampled point $y = S(u,v)$:

$$
p_A(y) \;=\; \frac{\overbrace{P_{\text{tree}}\textstyle\prod_k P_{c_k}}^{\text{discrete path}}\;\cdot\;\overbrace{1/|\Omega_\ell|}^{\text{leaf uniform}}}{\underbrace{\sqrt{\det\mathbf{G}(u,v)}}_{\text{param}\to\text{area}}}
$$

- $P_{\text{tree}}\prod_k P_{c_k}$ is the probability of the **discrete path**: the light-tree face pick, then $P_c = w(c)/\sum_{\text{siblings}} w(s)$ at each pyramid level. *Any* strictly positive weights give a valid sampler. If the weights were the exact node integrals $W_n = \int_{\Omega_n} E\sqrt{\det\mathbf G}$, the product would telescope to $W_\ell / W_{\text{root}}$.
- $1/|\Omega_\ell|$ is the uniform density **in parameter units** inside the leaf. $|\Omega_\ell|$ is the leaf cell's parameter-domain area clipped to the face's valid domain; cells straddling the face edge are partial.
- $\sqrt{\det\mathbf G(u,v)}$ is the **Jacobian of $S$**: $p_A\,dA = p_{\text{param}}\,du\,dv$ and $dA = \sqrt{\det\mathbf G}\,du\,dv$. It is evaluated pointwise *at the sample* from the full interpolated $(h,\nabla h)$: six dot products, never a node model. This denominator is why weight looseness can never bias the estimator.

Consistency check: exact weights and shrinking leaves give $p_A \to E/W_{\text{root}}$, the normalized target proportional to $E\cdot dA$. For MIS against BSDF samples, convert to solid angle at the shading point as usual: $p_\omega = p_A\,\lVert y - x\rVert^2 / |n_y \cdot \omega|$.

**Baking, and what "base-independent" survives.** The metric depends on per-face constants, so there are exactly two bake regimes. Both exist under per-face tiles; under a shared atlas tile (§4) the statistics bake is the natural one:

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
**Baselines:** (1) area-only descent (no $E$), (2) emission-only descent (no $\sqrt{\det\mathbf{G}}$; pbrt-v4's textured-emitter mechanism applied to $S$), (3) pre-tessellate plus light BVH (Conty–Kulla), the quality reference, (4) Ling et al. 2025 uniform-area sampling adapted to the traverser, which tests whether the pyramid is needed at all; run it **first**, (5) the fused product bake of §5A used as an adversary: a full-resolution $E\sqrt{\det\mathbf G}$ table per (face, tile) pair with the same exact-Jacobian PDF — the best any non-hierarchical sampler can place.
**Shape against (5), measured (A-MVP S7, [[Result — A-MVP receiver irradiance study]]):** footprint tables sum to one atlas, so there is no per-triangle blowup to claim; measured per footprint the table is in fact ~27× **smaller** than the pyramid (1 float per texel vs the 8 double node channels plus the emission mean and max and the triangle's boundary cache, 434 KB against 16 KB; ~14× under float32), so memory favors the hierarchy only above that reuse factor. Two recorded levers cut against it — precision per channel family, and storing a channel only at the levels where it pays. Far-field variance ties at low amplitude; at amplitude 0.2 the midpoint weights concede 1.3–3.1× (coarse-level $\sqrt{\det\mathbf G}$ misallocation, not the probability floor). The hierarchy's case is exactly four things: tile **reuse** (tables multiply per (face, tile) pair, the $h$ pyramid is per tile), **dynamics** (deformation and amplitude edits stale every table at O(atlas) per change; the pyramid never rebuilds — measured build 0.04 ms vs 0.17 ms per footprint, and the edit-loop demo exercises this), **receiver-aware descent** (above; the measured near-field and occluded wins a frozen area-domain table cannot have), and **certified pruning/coverage**.

**Object-level light sampler (recorded 2026-09-08; the next direction for A, not in the current plans).** The path tracer integration ([[Plan — Path tracer with displaced surfaces]], T8) registers one renderer light per emitting base triangle and leaves the selection among them to the renderer's power table, which ignores the receiver. That enumerates the emitting set at scene build, scales with the base face count, and cuts the hierarchy at triangle edges; measured on the 4,416-triangle station ring, every sampler kind lands within 25% of the others because the selection above them decides the variance ([[Result — Emitter sampling in scenes]]). The direction. One ks light per (instance, displaced object) instead of one per emitting base triangle, with a single hierarchical descent over the object's texture domain. Above the tile level the nodes are cells of the object's uv box (for the station ring, 16 × 4 tiles), each carrying what a light BVH cluster carries: a spatial bound from the base triangles' boxes and the height range, a normal cone from the vertex normals and the height range, the emission mass, and a precomputed metric-weighted mass summed over the base triangles it covers, so that only the receiver term is evaluated per sample; descent weights are receiver-aware from the object's root. At and below the tile level the nodes are the shared pyramid's cells, stored once per tile however many repeats, with the metric composed per copy at the leaf as now. Cells that base-triangle edges cross are stored once in the object's tree with a list of pieces (triangle, clipped area, clipped mass), instead of once per triangle per level as the footprint tree does today. Overlapping or mirrored uv charts add one discrete choice of the copy with a deterministic weight, so a surface point's density is pdf_uv × P(copy | uv) / √det G of that copy. The pdf of a hit walks the same tree from the object's root, as `pdf_hit` walks the triangle's tree today, so MIS is unchanged. The pyramid, the emission tile, the wrapped indexing, admissibility from clipped mass, the probability floor, the leaf draw and the pointwise metric at the leaf all stay; the midpoint composition of √det G and its moment-channel remedy are independent of this change. The per-triangle lights stay as the baseline and for the pre-tessellated substitutes. What it claims: receiver-aware selection over the whole emitter without enumerating emitting triangles ahead of time, at a per-sample cost proportional to the tree's depth, with memory that depends on the base mesh only through the uv tree and not on a per-triangle footprint or table. The measurement that would show it is S7's ladder run again with the object-level sampler in the row set: today every kind sits within 25% of each other there because the selection above them is the same.
**Success:** at least 5–10× variance reduction against (1) and (2) on correlated assets; within about 1.5× the noise of (3) at an order of magnitude less memory; (4) measurably worse or costlier; against (5), parity far-field plus a measured near-field win and the reuse/edit cost table. **Known gap to state:** no visibility term. Every light hierarchy shares this; MIS handles it.

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
- **Seams.** The ray and sampling queries take authored UV-atlas assets natively and inherit TFDM's open problem with them: the surface is continuous only where texture coordinates, base, and displacement all are, so chart boundaries can crack. Per-face tiles with shared edge rows avoid this by construction and remain available, since the general chart contains them. Whether the PDE applications need them is deferred until that work starts (§4).
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
  > Conservativeness is absolute: any bound violation kills the structure. The tightness requirement is scoped to the cells whose bounds the applications read for their answers: certified metric bounds must be within 3× of the true range at cells up to 8 texels per side, on typical (not adversarial) content. Coarser cells only steer efficiency — sampling descent allocation (variance, §5A), refinement depth (§5 B1), walk length and pruning (§5 B2), LoD drops deeper than about 3 levels (§5C) — and are priced per consumer in Phase 2. Outcome: **met** — 0 violations in ~4 million checks, worst median ratio 2.77 at criterion cells ([[Result — Phase 1 Taylor pyramid]]), under the identity chart; under general charts the metric quantities are unchanged and the cone reaches 3.09 under shear ([[Result — Chart dependence study]]).

### Phase 2 — three MVPs and the gate

Order (revised 2026-08-26: the sampling MVP moved to the front, ahead of the cheaper diagnostics; revised 2026-08-30: the ray query inserted second, because every later demo scene is bounded by how much geometry the visibility substrate can hold):

1. **A-MVP first**: single asset, implemented in C++ on the ks codebase (§4 substrate row); area-only descent against product descent against the **Ling et al. baseline, run first**. That baseline is the vault's own cheap test of whether the sampling story needs the pyramid at all. Second baseline: PBRT-v4-style textured bilinear-patch sampling (emission image distribution with exact pointwise Jacobian pdf), ported into the codebase. Implementation plan with phases and per-phase validation: [[Plan — A-MVP sampling implementation]].
2. **Ray query at parity** (**measured 2026-09-07**, [[Result — Traversal cost study]]): the min-max height channel is stored (§2.2, implemented 2026-08-30), and the traversal of [[Plan — Path tracer with displaced surfaces]] (T4, TFDM's walk in double) was measured in node tests and time against embree over the pre-tessellated mesh for the three bound choices: min-max only, slab only, both. Outcome: `both` never tests more nodes than the min-max box on any ray, so parity holds by construction; the slab wins at the leaves, 10 to 40% fewer leaf tests and 2 to 5% less time on two of three scenes with no loss on the third, and loses above 2-texel cells, so it is read at the leaves and one level up; the leaf, not the node test, is the cost. Per ray the walk is 15 to 27× slower than embree over the texel-aligned mesh on one thread in double without SIMD, and its memory is 20 to 36× smaller (8 MB against 162 MB for the 12.8k-quad torus), which is what makes larger demo scenes affordable: the walk's side scales, the visibility mesh does not. The claim is therefore no regression plus a leaf-level gain, not a traversal contribution. TFDM's level-of-detail scheme is not considered; the comparison is at full resolution. Evidence for the channel: [[Result — Pyramid channel study]]. The path tracer plan continues with T6 to T10 (engine integration, displaced lights, mixed scenes, the toml, OBJ and glTF scene format with a Blender workflow).
3. **D**: diagnostics over the corpus, correlation numbers. A guaranteed section regardless of what else survives.
4. **B1-MVP**: heat method on one face; then cross-face coupling via shared edge rows; then mip multigrid. Measure against pre-tessellated cotangent Laplacian.
5. **B2 feasibility spike** (**unconditional**; promoted from contingent on 2026-08-11 after reading the PWoS paper — its documented local-feature-size pain point and the enabling claim give B2 the strongest contribution structure in the project, so the gate must not decide without pricing it): branch-and-bound closest point over the pyramid, prism-inversion projection, and a fixed conservative tube on one asset. Enough to *price* PWoS before the gate, not to polish it.

> [!important] The gate — end of Phase 2
> Pick the headline (A or the B pair) on evidence: which MVP has the larger measured win and the more legible demo. Lock the paper skeleton: headline, plus the second application as a strong section, plus C and D. Anything not in the skeleton stops. If **both** MVPs disappoint but the bounds are tight, the fallback paper is "certified metric bounds + LoD + diagnostics", aimed at SGP/CGF. Decide that at the gate too, not at the deadline.

### Phase 3 — depth on the skeleton

- Headline application to full evaluation: all baselines from §5, the full corpus, memory/latency/variance tables, and the flagship demo (the emissive edit loop, or the untessellatable-asset geodesic). If the B pair is the headline, both B1 and B2 go to depth, including the crossover figure. If A is, B2 still ships as the certified-query demonstration section, budget permitting.
- C's FLIP correlation study across LoD, the comparison of $H^1$ against box-filtered mips, and the small-asset eigenvalue computation validating the spectral sandwich. All of this is mechanical and scriptable.
- Document failure modes as they happen: negative-weight cells, bound blowups. These become the limitations section, with numbers instead of hedges.
- If A is the headline: the object-level light sampler of §5A (one light per displaced object, receiver-aware descent from the object's root over a uv-space tree above the shared tile pyramid), measured on the station ring against the per-triangle lights and a light BVH over the pre-tessellated mesh.

### Phase 4 — writing (overlaps Phase 3)

- Derivations and conservativeness lemmas go to the supplemental. §1's story is the intro draft. Figures come from the Phase 2–3 harness, never bespoke.
- One full internal review pass against the §6 attack list before the deadline.
- Venue call: SIGGRAPH 2027 if the headline demo lands; otherwise SIGGRAPH Asia 2027 (later deadline, around May) or SGP for the fallback scope. This is a deliberate decision at the gate, not a scramble at the deadline.

---

Related: [[Log — Conservative metric queries]] · [[The induced metric of a displaced surface]] · [[Obliquity and the integrability defect]] · [[Laplace–Beltrami on displaced surfaces]] · [[Min-max mipmap and conservative bounds]] · [[Surface measure and sampling on implicit displaced surfaces]] · [[MOC — Open Questions]] · [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] · [[Zhang et al. 2026 — DJM]] · [[Shell, prism and prismoid]] · [[Watertightness and cracks]]
