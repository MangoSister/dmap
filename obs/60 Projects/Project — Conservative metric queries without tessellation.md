---
title: Project — Conservative metric queries without tessellation
tags: [project, plan, metric, displacement, bounds, sampling, pde, lod]
status: proposal
created: 2026-08-11
target-venue: SIGGRAPH 2027 (fallbacks — SIGGRAPH Asia 2027, SGP/CGF 2027)
---

# Project — Conservative metric queries without tessellation

**Working title:** *Intrinsic Geometry of Displacement Maps: Conservative Metric Queries without Tessellation.*

> [!abstract] What this note is
> The execution plan for turning the metric thread — [[The induced metric of a displaced surface]] · [[Obliquity and the integrability defect]] · [[Laplace–Beltrami on displaced surfaces]] · [[Taylor-model bound pyramid]] — into a SIGGRAPH-caliber paper and a research prototype. It recaps the technical core (§2, details delegated to the concept notes), makes the engineering choices the notes deferred (§4), curates four applications with baselines (§5), and sequences the work for fast, killable iteration (§7).
>
> Provenance: assessed 2026-08-11 in an outside review; derivations verified independently, corrections applied to the concept notes the same day. PWoS promoted to a first-class application (§5 B2) on the strength of its seam-indifference and certified-query fit; both PDE backbones read from their PDFs — [[Crane et al. 2013 — Geodesics in Heat]] · [[Sugimoto et al. 2024 — Projected Walk on Spheres]].

---

## 1. The story

A displacement map is a promise. A small texture stands in for billions of micro-triangles that are never built: [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] renders film-scale detail from 34 MB where pre-tessellation needs gigabytes, updates in microseconds after an edit, and tiles and instances for free — because the geometry exists only as a rule, `S = P + hN`, plus a hierarchy of conservative bounds on $h$.

But the promise is kept for exactly one question. Every structure in this vault — min-max mipmaps, RMIP, prisms, micromaps — answers *what does this ray hit*. Ask the surface anything else:

- How much **area** does this patch have, and where should I put samples on it?
- How **far apart** are two points, walking on the surface?
- Which **direction** loses detail first when I drop a mip level?
- Is this an emissive surface I can **importance-sample**?

— and today the only answer is to break the promise: tessellate, and give up the memory, the edit latency, and the instancing that made displacement attractive in the first place.

**The fix is one derivative.** Every structure bounds $h$; none bounds $\nabla h$ ([[Min-max mipmap and conservative bounds]]). And bounding $\nabla h$ does not merely buy the area — it buys the **first fundamental form** $\mathbf{G}$, the $2\times 2$ matrix that lets an ant on the surface measure lengths, angles and areas without ever leaving it. Area is one scalar functional of $\mathbf{G}$. The metric itself carries the Laplace–Beltrami operator, and with it geodesics, diffusion, spectra, and anisotropy — none of which can be assembled from area estimates at any sample count. That asymmetry is the thesis: **a scalar can be estimated stochastically (Ling et al. 2025 already do, by casting rays); an operator has to be *constructed*, and construction needs the metric.**

The intuition for why a *hierarchy* of metric bounds is the right structure generalises the ray-tracing story. In ray tracing the hierarchy answers a predicate — *could this ray hit this region?* — and a certain "no" skips the subtree. For everything above, the hierarchy answers **per-region certificates** — bounds on distance, area, stretch — and certificates feed three engines:

| Mechanism | Consumer | Ray-tracing analogue |
|---|---|---|
| **Prune** (branch-and-bound) | closest-point queries; certified empty balls for walk-on-spheres solvers | skip missed nodes |
| **Allocate** (proportional descent) | hierarchical CDF sampling; adaptive refinement; certified quadrature | skip empty space |
| **Precondition** (coarse-to-fine) | mip pyramid as multigrid; base mesh as coarsest operator | LoD traversal cutoff |

The genre template is *Spelunking the Deep* (Sharp & Jacobson, SIGGRAPH 2022): take a representation the field only knows how to render, apply conservative range analysis, and unlock a family of queries the representation never supported. They did it for neural implicits with interval arithmetic. We do it for displacement maps with a metric-aware bound pyramid — and where their queries end at closest-point, ours extend to measure, sampling density, and differential operators, because the displaced surface has a *parameterisation* and therefore a metric, which a level set does not.

**Contribution statement** (draft, one line per reviewer-visible claim):

1. A **joint conservative bound structure** — a Taylor-model pyramid over $(h, \nabla h)$ (§3.2) — that yields certified per-region bounds on the metric, the area element, and the surface normal cone of an implicit displaced surface, at two-channels-worth of extra cost over the existing min-max pyramid.
2. A set of **queries** built on it: certified area and integrals, product importance sampling of emissive displaced surfaces, surface PDE solves — texture-space heat method and certified queries for pointwise Monte Carlo (projected walk on spheres) — and anisotropic LOD error; each impossible or unpriced on this representation today (§5).
3. Two **pre-bake diagnostics** for the base mesh — obliquity and the integrability defect — with the corrected shell-validity criterion (§5D, from [[Obliquity and the integrability defect]]).

The pointwise metric formula itself is *not* claimed — it is classical machinery instantiated for this representation, and the paper cites it as such (§8).

---

## 2. Technical core, recapped

Full derivations live in the three concept notes; this section is the paper-sized recap plus the new material the notes don't yet have.

### 2.1 The metric (recap — see [[The induced metric of a displaced surface]])

For $S = P + hN$ with $\lVert N\rVert = 1$:

$$
\mathbf{G} \;=\; \underbrace{\mathbf{G}_0 - 2h\,\mathbf{B}_0 + h^2\mathbf{C}_0}_{\mathbf{Q}(h)\ \text{— offset metric}} \;+\; \underbrace{\nabla h\,\nabla h^\top}_{\text{slope}} \;+\; \underbrace{\nabla h\,\mathbf{a}^\top + \mathbf{a}\,\nabla h^\top}_{\text{obliquity coupling}}
$$

unconditionally — no perpendicularity, no integrability. The coefficients $\mathbf{G}_0, \mathbf{B}_0, \mathbf{C}_0, \mathbf{a}$ are pure base geometry, constants or cheap closed forms per flat base triangle; $h$ and $\nabla h$ enter through separate channels. Pointwise evaluation costs a normalise and six dot products — cheaper than a ray–box test. Area is $dA = \sqrt{\det\mathbf{G}}\,du\,dv$ with closed-form determinants (rank-one lemma when $\mathbf{a}=0$, Sylvester when not).

Both 2026-08-11 review corrections are now folded into the concept notes themselves (µ-mesh metrics come from vertex positions, not the formula — the formula and its bounds are for the tessellation-free family; $\mathbf{G}$ and DJM's shell Jacobian are complementary, never "subsumed"). The paper inherits the corrected phrasing by construction.

### 2.2 The bound node — recap

The structure is a **first-order Taylor model** per pyramid node: six numbers $(h_0, g_u, g_v, r, \rho_u, \rho_v)$ meaning *the displacement lies within $r$ of the stored plane, and its gradient within $(\rho_u, \rho_v)$ of the plane's slope*, over the node's cell. The stored plane appears in both the $h$-model and the $\nabla h$-model — that shared appearance is what keeps the $h$–$\nabla h$ correlation that the metric needs and that independently stored min-max gradient channels destroy (they let determinant bounds draw the same gradient twice at worst case — false degeneracy alarms exactly in the oblique regime).

What the paper gets from it, one lemma each: it **subsumes** the existing min-max channel, so TFDM/RMIP traversal runs unchanged (and may tighten — slab bounds of thickness $O(s^2)$ where min-max boxes are $O(s)$); leaf construction is **closed-form and exact for bilinear** interpolation; the upward **fold is conservative** by triangle inequality and costs one mipmap pass; propagation through §2.1 yields certified per-node intervals on $\mathbf{G}$'s entries, $\sqrt{\det\mathbf{G}}$ (area), the eigenvalues of $\mathbf{G}_0^{-1}\mathbf{G}$ (anisotropy), and a **normal cone** — the free third consumer that plugs the representation into many-light samplers (§5A). Cost: 6 channels vs 2, same pyramid topology.

Full spec — the failure analysis of independent channels, exact leaf formulas, the fold, metric propagation, the LoD-semantics caveat vs TFDM's adjusted min-max, and query interactions — lives in **[[Taylor-model bound pyramid]]**; this plan intentionally carries only the recap.

### 2.3 Composition — texture statistics are base-independent, the metric is not

TFDM's pyramid is instanceable because min-max $h$ is a pure texture property. $\det\mathbf{G}$ is **not** — it depends on $\mathbf{G}_0, \mathbf{B}_0, \mathbf{C}_0, \mathbf{a}$, which vary per base triangle. So the pyramid must store only base-independent statistics of $(h, \nabla h, E)$, and every query **composes** them with the querying triangle's constants on the fly — the same philosophy as TFDM's on-the-fly box generation, applied to metric bounds. Exact node integrals do not compose (the integrand is nonlinear in the base constants); *bounds* compose, and for sampling that costs only variance, never correctness (§5A). The resulting architecture is two-level: a per-triangle outer structure (each triangle's composed totals) over a shared inner pyramid.

---

## 3. Believed new vs classical — the paper's honesty ledger

**Classical, cite generously:** the three fundamental forms, offset/parallel-surface metric and focal surfaces (any surface-theory text; shell mechanics knows $\mathbf{Q}$ as the *shifter*); oblique director fields (Cosserat shell theory); height-field metric; cotan Laplacian and its intrinsic character (edge lengths only); heat method; walk on spheres and its surface extension (Sawhney–Crane line; Sugimoto et al. 2024); affine arithmetic and Taylor models (validated numerics); hierarchical sample warping (environment maps, light trees); analytic displaced-surface derivatives ([[Nießner & Loop 2013 — Analytic Displacement Mapping]] — the closest graphics precedent for "the metric is nearly free once derivatives are in hand", and the smooth-base escape from the watertightness trilemma).

**Believed new:** the [[Taylor-model bound pyramid|joint Taylor pyramid]] over $(h,\nabla h)$ with certified metric/area/normal-cone bounds on an implicit displaced surface; the composition rule of §2.3; product importance sampling of emissive displaced surfaces with exact PDFs (§5A); surface PDEs on displacement maps by either solver — texel-lattice intrinsic Laplacian with metric edge lengths and mip-multigrid (§5 B1; anticipated in spirit by intrinsic triangulations — check first), and certified closest-point/lfs queries feeding PWoS, replacing its heuristic medial-axis pipeline (§5 B2); the obliquity/δ diagnostics and three-determinant separation ([[Obliquity and the integrability defect]]).

**Check-first list (cheap, do in Phase 0–1, parallel):** Cosserat/oblique-director shell strain measures; CAD literature on general offset surfaces; Sharp–Soliman–Crane intrinsic triangulations; LEADR/LEAN moment pyramids (the non-conservative ancestor — the delta is *conservativeness + localisation*, say so before a reviewer does); Conty Estevez & Kulla 2018 and successors for light-tree cluster metrics; Ling, Madan, Sharp & Jacobson 2025 (the uniform-sampling baseline).

---

## 4. Engineering decisions

| Decision | Choice | Why | Crucial? |
|---|---|---|---|
| Parameterisation | **Per-face barycentric grids, Ptex-style, shared edge rows** | Kills the seam problem *by construction* — turns the thread's named kill-risk into a design sentence with µ-mesh and Ptex precedent | **Crucial** |
| Gradient source | **Analytic from the interpolant, never stored channels** | Stored $(h, h_u, h_v)$ channels drift mutually inconsistent under filtering; analytic derivatives are exact and free (Nießner & Loop precedent) | **Crucial** |
| Interpolant | Decide in Phase 0: bilinear (matches TFDM, $\nabla h$ discontinuous at texel edges) vs quadratic/cubic B-spline ($C^1$, cleaner operator, Nießner & Loop used biquadratic) | The operator applications prefer $C^1$; the bound construction works for both | **Crucial to decide, cheap to switch** |
| Bound node | **Taylor model (§2.2)**, not two min-max channels | It *is* contribution 1; independent channels demonstrably cannot deliver the correlation claim | **Crucial** |
| Prototype substrate | CPU, C++ core + Python bindings for experiments; PBRT-v4 (or Mitsuba 3) integration **only** for §5A | Fast iteration; every application is offline-legitimate | **Crucial to keep CPU-first** |
| Test corpus | Maggiordomo dataset (89 assets, shared with DJM) + 3–4 authored emissive assets (lava/ember crevices, displaced neon) | Comparability with the construction literature; the emissive assets exercise $E$–slope correlation | Important |
| Seam arithmetic | Float-consistent shared edge rows (same values, same order) | Bit-exactness machinery is production polish | Deferrable |
| Pyramid compression, GPU traversal, hardware | None | The paper's claims are memory/capability/latency, not real-time throughput | Deferrable |
| Vector displacement, animation | Out of scope | Scalar $h$ is where all the structure lives; say so in limitations | Deferrable |
| Arbitrary UV atlas compatibility | Out of scope; per-face storage replaces it | Mortar/non-conforming seam coupling is a paper of engineering by itself; the honest sentence is "we adopt per-face parameterisation, as Ptex did, for the same reason" | Deferrable — **but state it prominently** |

The two rows that most often sink projects like this are the first and the fifth: adopting per-face storage *now* means no week is ever spent on chart transitions, and staying CPU-first means every experiment this fall runs in minutes.

---

## 5. Applications

Four areas, spanning rendering, geometry processing, content pipeline, and authoring — with the PDE area split into two sibling applications (B1, B2) that share one platform. Rule from the 2026-08-11 review: **this is one paper, not four** — after the Phase 2 gate (§7), A or the B pair becomes the headline, the other a strong section, C and D are always sections.

### A. Product sampling of emissive displaced surfaces — *rendering; headline candidate*

**Claim.** An emissive displaced surface becomes a first-class light source: samples drawn $\propto E \cdot dA$ with **exact PDFs**, plus per-node power, spatial bounds, and normal cones — i.e., the surface presents itself to a Conty–Kulla-style many-light sampler as a native cluster hierarchy, without ever being meshed.

**Mechanism.** Two-level hierarchical CDF descent — a light tree over base faces, pyramid descent within the chosen face, a uniform draw inside the leaf cell. The load-bearing split: descent weights only steer **variance**; the PDF is **exact** because the pointwise metric is closed-form.

**The PDF, term by term.** The procedure's density *with respect to surface area* at the sampled point $y = S(u,v)$:

$$
p_A(y) \;=\; \frac{\overbrace{P_{\text{tree}}\textstyle\prod_k P_{c_k}}^{\text{discrete path}}\;\cdot\;\overbrace{1/|\Omega_\ell|}^{\text{leaf uniform}}}{\underbrace{\sqrt{\det\mathbf{G}(u,v)}}_{\text{param}\to\text{area}}}
$$

- $P_{\text{tree}}\prod_k P_{c_k}$ — the probability of the **discrete path**: the light-tree face pick, then at each pyramid level $P_c = w(c)/\sum_{\text{siblings}} w(s)$. *Any* strictly positive weights give a valid sampler; if the weights were the exact node integrals $W_n = \int_{\Omega_n} E\sqrt{\det\mathbf G}$, the product telescopes to $W_\ell / W_{\text{root}}$.
- $1/|\Omega_\ell|$ — the uniform density **in parameter units** inside the leaf; $|\Omega_\ell|$ is the leaf cell's parameter-domain area clipped to the face's valid domain (cells straddling the face edge are partial).
- $\sqrt{\det\mathbf G(u,v)}$ — the **Jacobian of $S$**: $p_A\,dA = p_{\text{param}}\,du\,dv$ and $dA = \sqrt{\det\mathbf G}\,du\,dv$. Evaluated pointwise *at the sample* from the full interpolated $(h,\nabla h)$ — six dot products, never a node model. This denominator is the exactness lever: it is why weight looseness can never bias.

Consistency check: exact weights and shrinking leaves give $p_A \to E/W_{\text{root}}$ — the normalised target $\propto E\cdot dA$. For MIS against BSDF samples, convert to solid angle at the shading point as usual: $p_\omega = p_A\,\lVert y - x\rVert^2 / |n_y \cdot \omega|$.

**Baking, and what "base-independent" survives.** The metric depends on per-face constants, so there are exactly two bake regimes, both natural under per-face tiles (§4):

- *Statistics bake — base-independent, the default.* The tile pyramid stores only texture-channel statistics: the Taylor nodes of $h$ (§2.2) plus per-node emission statistics ($\bar E$ and $[E_{\min}, E_{\max}]$; a sum-pyramid of $E$ is exact at texel granularity). Face constants enter only at query time through `composedWeight`. This is what survives **sharing**: detail tiles reused across faces, instancing with per-instance displacement amplitude $\alpha$ (substitute $h \to \alpha h$, $\nabla h \to \alpha \nabla h$ — analytic), and runtime emission edits (refold the $E$ channels at mipmap cost). Composition error → variance only.
- *Fused product bake — per-face, optional.* A Ptex-style tile is bound to its face, so the constants **are** known at bake: precompute $w_n = \int_{\Omega_n} E\sqrt{\det\mathbf G}$ per node (per-texel quadrature at the finest level, summed upward). Tightest possible weights — the $E$–slope correlation is captured exactly at every level — at the price of the base-dependence: breaks under per-instance amplitude, non-uniform instance scaling, and emission edits (rebake). For static hero assets. Uniform world scaling multiplies all areas by $s^2$, cancels inside the asset, and only rescales its light-tree total.

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

**Demonstrate:** equal-sample-count variance and equal-time error on the emissive assets; memory and bake time vs the meshed alternative; **edit-to-converged latency** after moving a glow crack (the demo nobody else can do).
**Baselines:** (1) area-only descent (no $E$), (2) emission-only (no $\sqrt{\det\mathbf{G}}$), (3) pre-tessellate + light BVH (Conty–Kulla) — the quality reference, (4) Ling et al. 2025 uniform-area sampling adapted to the traverser — the "do you even need the pyramid" test, run **first**.
**Success:** ≥5–10× variance reduction vs (1)/(2) on correlated assets; within ~1.5× noise of (3) at an order of magnitude less memory; (4) measurably worse or costlier. **Known gap to state:** no visibility term — same as every light hierarchy; MIS handles it.

### B. Surface PDEs on displacement maps — *geometry processing; headline candidate, two applications*

PDEs on the displaced surface — geodesic distance, diffusion, Poisson — without tessellation, by **two solvers that share the platform and consume it in opposite ways**: B1 needs the hierarchy to be *good* (it optimizes and certifies a solve that would function without it); B2 needs the hierarchy to be *right* (its correctness rests on conservative queries). They fail differently (anisotropy hurts B1, variance hurts B2), win differently (dense global fields → B1, sparse point evaluations → B2), and the measured **crossover** between them is itself a contribution-grade figure. Each is the other's fallback; the section survives either failing alone. The deeper unity: [[Sugimoto et al. 2024 — Projected Walk on Spheres|PWoS §5.2.2]] runs [[Crane et al. 2013 — Geodesics in Heat|the heat method]] *inside* the Monte Carlo solver — so B1 and B2 are two discretisations of one algorithm, on one platform.

#### B1. Texture-space heat method — global fields

**What the heat method is, originally.** [[Crane et al. 2013 — Geodesics in Heat]] computes geodesic distance by refusing to solve the eikonal equation $|\nabla\varphi| = 1$ (nonlinear, hyperbolic, serial fast-marching updates, no prefactorization). The insight: heat diffusing from the source flows *along* geodesics, but its magnitude is numerically hopeless (Varadhan) while its **direction** is robust. So: use heat only for direction, and rebuild the magnitude by integration —

1. one backward-Euler heat step: solve $(\mathbf{M} - t\mathbf{L})\,f = \delta_\gamma$;
2. normalise: $X = -\nabla f / \lVert \nabla f \rVert_{\mathbf G}$, a unit field pointing down the distance gradient;
3. one Poisson step: solve $\mathbf{L}\,\varphi = \nabla\!\cdot\!X$ — the scalar field whose gradient best matches $X$; $\varphi$ is the distance.

Two sparse SPD solves, source-independent matrices, prefactor once — every new source is two back-substitutions. Crucially, the method is **substrate-agnostic by design**: it needs only a Laplacian, a gradient, and a divergence, and the paper runs it on meshes, polygonal surfaces, and point clouds precisely to make that point.

**The gap, and what we contribute.** On a displacement map the three operators don't exist — that is the whole gap, and the metric fills it: cotan weights are intrinsic (edge lengths only), lengths come from $\ell^2 = e^\top \mathbf{G}\, e$ on the texel lattice, cross-face coupling is exact through shared edge rows (§4), and the heat method needs **only first derivatives of $h$** — no Christoffel symbols, no second-derivative structure ([[Laplace–Beltrami on displaced surfaces]]). On top of the operator, the hierarchy contributes three certified decisions that tessellated pipelines hand-tune:

- **Multigrid from the pyramid.** Each mip node's Taylor plane *is* a coarse-cell metric, so every pyramid level carries its own operator for free; the base-mesh cotan Laplacian ($\mathbf{G}_0$ constant per triangle) is the coarsest level. Remainder magnitudes bound how far the true fine metric strays from a node's model — a certified local homogenization error that says where coarsening is safe.
- **A priori adaptive resolution.** Descend each region until the metric-bound width falls under tolerance; the resulting mixed-level leaves are the DOF lattice. Classical FEM adaptivity needs a solve to estimate error; here the geometry-induced component is certified *before any solve*, from bounds no tessellated mesh carries.
- **Validity certificates.** The two Path-B hazards — metric edge lengths violating the triangle inequality, and negative cotan weights (lost maximum principle) — are per-cell conditions on $\mathbf{G}$, certifiable from its bounds: descend until the certificate holds. The largest eigenvalue interval of $\mathbf{G}^{-1}$ per region also gives a certified stable time step for explicit diffusion.

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

**Demonstrate:** geodesic distance and isolines on an asset whose tessellation would not fit in memory; solve error vs lattice resolution; **re-solve latency after a displacement edit** — with the precise mechanism stated, because the heat method's amortization rests on prefactoring $\mathbf{L}$ and an edit invalidates the factorization: the texel lattice's **sparsity pattern is fixed under displacement edits** (only matrix values change), so the symbolic factorization survives and only the numeric phase or the multigrid re-setup reruns, versus remesh + reassembly + full symbolic-and-numeric refactorization for the tessellated baseline; pyramid rebuild is mipmap-cost. The PDE counterpart of TFDM's four-orders-of-magnitude edit win, and the latency table should report the numeric-refactor split explicitly. Also: diffusion-based decal spreading.
**Baselines:** (1) cotan on pre-tessellation at matched resolution (accuracy reference; 4× resolution = ground truth), (2) [[Laplace–Beltrami on displaced surfaces|Path A]] on a µ-mesh — the honest cheap alternative the operator note itself names, (3) intrinsic triangulations on a coarse extraction.
**Success:** within 1–2% of (1) at ≥10× less memory; edit-to-answer under a second where (1) needs minutes of remesh + rebuild.
**Risks:** negative weights and triangle-inequality failures under strong anisotropy — the certificates above turn these from silent wrong answers into refinement triggers, but pathological content could force refinement to the finest level (degrades to uniform, never lies).

#### B2. Projected walk on spheres — pointwise queries

**What PWoS is, originally.** [[Sugimoto et al. 2024 — Projected Walk on Spheres]] solves surface Poisson problems ($\Delta_{\mathcal S} f = f_{\mathcal S}$, Dirichlet data $g$ on curves $C$) **pointwise and discretization-free** — no meshing, no global system, evaluation only where the answer is wanted (their headline figure evaluates diffusion curves only at visible pixels). The mechanism is the **closest point extension**: extend $f$ constant along normals into a tube around the surface; inside the tube, the surface Laplacian becomes the ordinary ambient $\Delta$, so classic walk-on-spheres applies with one twist — **project every step back to the surface**:

- at $x$: step radius $r = \min(\mathrm{lfs}(x),\ d_C(x))$ — stay inside the tube, don't cross the boundary;
- sample $y$ uniformly on the 3D sphere of radius $r$; accumulate the source from ball samples, projected through $\mathrm{cp}_{\mathcal S}$;
- recurse at $\mathrm{cp}_{\mathcal S}(y)$; within $\varepsilon$ of the boundary, read $g$ at the closest boundary point.

Unbiased at $O(1/\sqrt{N_P})$ over independent walks (up to their stated $g\!=\!0$ extension caveat), embarrassingly parallel, output-sensitive.

**Its two geometric dependencies — and how the paper meets them today.** (a) A closest-point query per step: their implementation calls Houdini's *mesh* closest point, so in practice the geometry is discretized after all — "discretization-free" describes the solver, not the geometry access. (b) A **conservative lower bound on local feature size**, so spheres stay inside the valid tube: their §3.1 builds it by preprocessing — a medial-axis point cloud via shrinking balls, scale-axis-style pruning, a ×0.9 safety factor, a corner clamp. That is *heuristic* conservatism, and the paper documents both failure directions: too small is valid but slow (their Fig. 3: average walk length **31 → 1819 steps** as the lfs estimate shrinks 0.99 → 0.0625), and aggressive pruning produces **visible residual bias** (their Fig. 4c). The correctness-critical quantity is estimated, not certified.

**What we contribute.** The platform replaces both dependencies with certified versions, and makes the method run on a representation it cannot touch today:

- **Certified $\mathrm{cp}_{\mathcal S}$ on the implicit displaced surface** — branch-and-bound over on-the-fly node slabs, prism inversion for the parametric coordinates of the projection. Conservative pruning is *unbiasedness* here: a non-conservative bound can discard the node holding the true closest point, and a wrong projection biases the walk silently. Today PWoS cannot run on a TFDM asset at all — there is no $\mathrm{cp}_{\mathcal S}$ to call without tessellating first.
- **Certified lfs** $= \min(\text{curvature reach},\ \tfrac12\,\text{self-separation})$: the local half from the per-triangle curvature scalar, *localized* (enlarged where it matters) by per-node normal cones; the global half — how close a distant sheet approaches — via hierarchical node-pair pruning, which **no local or pointwise estimate can see**. This replaces the medial-axis preprocessing outright; their Fig. 3 sensitivity is the argument for making the certified bound *tight*, which the cone localization addresses. Under-estimation only slows; nothing in the pipeline can bias.
- **Certified $d_C$** on the safe side, for boundary curves authored in texture space.
- **Source and boundary sampling** ∝ area via §5A's machinery, PDFs exact.
- Structural bonuses inherited from pointwise-ness: **seam-indifferent** (walks live in 3D — the pointwise-exception callout in [[Laplace–Beltrami on displaced surfaces]]), **zero rebuild on edit**, cost independent of surface size.

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

**Demonstrate:** solution-at-a-point cost vs accuracy on the same untessellatable asset as B1; walk-length statistics under certified lfs vs a deliberately loosened one (reproducing their Fig. 3 sensitivity on our terms); view-dependent evaluation (their diffusion-curves demo) on a displaced surface; zero-rebuild edits.
**Baselines:** (4) PWoS on the tessellated mesh with a BVH closest point and their medial-axis lfs pipeline — isolates exactly what certification and the representation buy; (5) B1 at matched accuracy, for the crossover figure.
**Success:** matching (4)'s accuracy with the memory advantage and no preprocessing pipeline; the crossover against B1 mapped as query density sweeps.
**Risks:** Monte Carlo variance; restriction to Poisson-type problems with Dirichlet data and the $g = 0$ extension bias for complex sources (both stated by the original paper — scope inherited, not fought); a loose curvature scalar shrinking the tube and slowing walks (degrades, never biases).

### C. Anisotropic and spectral LoD error — *content pipeline; always a section*

**Claim.** Because every mip level of a displacement map shares one parameterization, the fine↔coarse correspondence is *canonical* — something mesh-decimation LoD never has — and "what does level $k$ destroy?" becomes the distortion of that map: the stretch tensor $\mathbf{D} = \mathbf{G}^{-1/2}\mathbf{G}_k\mathbf{G}^{-1/2}$, a $2\times2$ closed form whose log splits exactly into an **area** channel and an **anisotropy** channel, $\varepsilon^2 = 2(a^2 + s^2)$. Three results ride on it (full derivation: [[Anisotropic and spectral LoD error]]):

- **Certified selection.** Per level-$k$ cell, the coarse surface's Taylor model is *exact* (one bilinear patch) while the folded pyramid node bounds the fine surface — so the pyramid certifies $[\varepsilon_{\text{lo}}, \varepsilon_{\text{hi}}]$ per region, and LoD selection returns a **level map** with a guarantee ("no point exceeds the budget"), not a sampled heuristic. Height error stays as the *position* budget; $\varepsilon$ is the *geometry* budget — complementary, and the ripple-smoothing case (height error $\delta$, slope error $\omega\delta$) is where the difference is an order of magnitude.
- **The spectral certificate.** A certified stretch bound $\delta$ sandwiches every Laplace–Beltrami eigenvalue: $\lambda_i^{(k)}/\lambda_i \in [e^{-3\delta}, e^{3\delta}]$, via Courant–Fischer — spectrum preservation guaranteed without computing a spectrum. And by 2D conformal invariance the **anisotropy channel is the only spectrally dangerous one** — "anisotropic" and "spectral" are one claim, seen twice.
- **Metric-aware mips.** Coarse levels can be *fit* rather than box-filtered: an $H^1$ (gradient-preserving) fit is linear, tile-local, and preserves to first order exactly the slope term box filtering destroys; a full metric fit (Gauss–Newton on $\Sigma\varepsilon^2$) is the offline reference.

**Demonstrate:** rank correlation of $\varepsilon$ vs height error vs det-only against rendered-image error (FLIP/ΔE) across LoD switches; the certified level map on a full asset; $H^1$-fit mips vs box mips at equal memory (both certified $\varepsilon$ and FLIP); small-asset eigenvalue computation validating the $e^{\pm3\delta}$ sandwich empirically; anisotropy direction picking the mip-bias axis.
**Baselines:** TFDM's height-error LoD; det-only (DJM-flavoured); Hausdorff on tessellated geometry; box-filtered mips (for the constructor).
**Success:** strictly better rank correlation with image error at negligible extra cost; a measurable $\varepsilon$/FLIP gap for $H^1$ mips. Small, clean, quantitative — reviewer-proof.

### D. Pre-bake base-mesh diagnostics — *authoring; always a section*

**Claim.** Obliquity $\sin^2\theta = \mathbf{a}^\top\mathbf{G}_0^{-1}\mathbf{a}$ and the integrability defect $\delta$ are per-triangle, pre-bake, dot-product-cheap predictors of metric distortion that no existing constructor measures — [[Zhang et al. 2026 — DJM|DJM]] and Maggiordomo included; visibility tests admissibility, not distortion ([[Obliquity and the integrability defect]]).
**Demonstrate:** heatmaps at decimation time; correlation of the diagnostics with post-bake reconstruction error across the Maggiordomo corpus; a case that passes the visibility test with high obliquity and visible artefacts; the corrected shell criterion (three determinants) vs $h < 1/\kappa_{\max}$.
**Baselines:** Maggiordomo's visibility value; DJM's $\det J$.
**Effort:** days, not weeks — highest insight-per-hour section in the paper.

---

## 6. Limitations and non-goals

State in the paper, decided now:

- **Scalar displacement only.** Vector displacement has no height-field structure to bound. Non-goal.
- **The metric is of the smooth interpolated surface.** Renderer leaf geometry (local triangulation, bilinear patches) differs at the leaf scale; quantify the discrepancy once (Phase 0) and state it. Not fixable, only priceable.
- **No visibility in sampling** (§5A) — inherited from all light hierarchies; MIS mitigates.
- **Obliquity makes $\mathbf{G}$ indefinite-adjacent:** $\det\mathbf{G}$ can vanish before the focal surface; we diagnose (§5D) rather than repair. Self-intersecting displacement has ill-defined area — counted by multiplicity, stated.
- **No real-time claim, no GPU traversal, no compression, no hardware story** — the claims are capability, memory, and edit latency. The DMM withdrawal ([[Graphics API and hardware support timeline]]) makes software-first the defensible posture anyway.
- **No arbitrary-atlas compatibility** — per-face parameterisation is the representation, as in Ptex; converting authored atlas assets is a resampling preprocess, demonstrated once, not optimised.
- **Second-derivative pyramid** (per-node curvature bounds, Christoffel marching, spectral computations at scale) — future work. Application B2 needs only a single conservative curvature scalar per base triangle for its tube radius; the heat method (B1) needs no second derivatives at all.

**Pre-empted reviewer attacks** (each gets a sentence in the paper): *"Just tessellate"* → memory/edit-latency/instancing tables, and the LOD-bias point that enumeration changes area silently. *"The formula is classical"* → agreed, cited (shell theory, Nießner & Loop); the contribution is the certified bound structure and the queries. *"Ling et al. already sample"* → uniform ≠ product; no localisation, no certificates, no PDFs w.r.t. a target density. *"LEADR already mipmapped gradients"* → moments, not bounds; appearance, not measure.

---

## 7. Execution plan

Bias throughout: **cheapest kill-test first, nothing polished before the gate.** Wall-clock ~22 weeks from 2026-08-17 to the SIGGRAPH deadline (late Jan 2027); the gate decides whether that target or the fallback is real.

### Phase 0 — harness and hygiene (2 wk, → Aug 31)

- Python/numpy reference: pointwise $\mathbf{G}$ vs finite differences vs dense tessellation Gram matrices. The sanity checks from the notes (face-normal case: area independent of $h$; sphere: $\delta = 0$) as unit tests.
- Decide the interpolant (bilinear vs B-spline) by measuring gradient-discontinuity impact on a toy cotan solve.
- Quantify smooth-vs-rendered-surface discrepancy (limitation 2) once, on two assets.
- ~~Note hygiene~~ **done 2026-08-11**: the three concept-note overclaims are fixed, [[Nießner & Loop 2013 — Analytic Displacement Mapping]] is in `40 Reference/`, and the bound structure has its own spec in [[Taylor-model bound pyramid]].
- Launch the check-first literature sweeps (§3) in parallel; they cost reading time, not build time.

### Phase 1 — the Taylor pyramid, and its kill-test (3 wk, → Sep 21)

- Implement §2.2: leaf construction from the interpolant, conservative fold, affine propagation through §2.1 to per-node bounds on $\mathbf{G}$ entries, $\sqrt{\det\mathbf{G}}$, stretch eigenvalues, normal cone.
- **Tightness study** across the Maggiordomo corpus: bound width vs true range per level. Ablations: Taylor node vs (a) two independent min-max gradient channels, (b) interval arithmetic, (c) TFDM-style plain affine; min-max-$h$ recovered from the Taylor node vs the adjusted min-max recursion.
- > [!danger] Kill criterion
  > If metric bounds are >3× loose at the levels applications traverse, on typical (not adversarial) content — stop and rethink the node before building anything on top. Loose bounds poison every application at once.

### Phase 2 — three MVPs and the gate (6 wk, → Nov 2)

Order by cost:

1. **D first** (≤1 wk): diagnostics over the corpus, correlation numbers. Guaranteed section regardless of what else survives.
2. **A-MVP** (2 wk): single asset, CPU path tracer or PBRT hook, area-only vs product descent vs the **Ling et al. baseline run first** — the vault's own two-day test that decides whether the sampling story needs the pyramid at all.
3. **B1-MVP** (2–3 wk): heat method on one face; then cross-face via shared edge rows; then mip multigrid. Measure against pre-tessellated cotan.
4. **B2 feasibility spike** (≤1 wk, **unconditional** — promoted from contingent 2026-08-11 after reading the PWoS paper: its documented lfs pain point and the enabling claim give B2 the strongest contribution structure in the project, so the gate must not decide without pricing it): branch-and-bound closest point over the pyramid + prism-inversion projection + a fixed conservative tube on one asset — enough to *price* PWoS before the gate, not to polish it.

> [!important] The gate — 2026-11-02
> Pick the headline (A or the B pair) on evidence: which MVP has the larger measured win and the more legible demo. Lock the paper skeleton: headline + second app as strong section + C + D. Anything not in the skeleton stops. If **both** MVPs disappoint but bounds are tight, the fallback paper is "certified metric bounds + LOD + diagnostics" aimed at SGP/CGF — decide that here too, not in January.

### Phase 3 — depth on the skeleton (8 wk, → Dec 28)

- Headline app to full evaluation: all baselines from §5, full corpus, memory/latency/variance tables, the flagship demo (emissive edit loop, or the untessellatable-asset geodesic). If the B pair is the headline, both B1 and B2 go to depth including the crossover figure; if A is, B2 still ships as the certified-query demonstration section, budget permitting.
- C's FLIP correlation study across LoD, the $H^1$-vs-box mip comparison, and a small-asset eigenvalue computation validating the spectral sandwich (all mechanical, scriptable).
- Failure-mode documentation as it happens — negative-weight cells, bound blowups — these become the limitations section with numbers instead of hedges.

### Phase 4 — writing (4 wk, overlapping from Dec 1)

- Derivations and conservativeness lemmas to supplemental; §1's story is the intro draft; figures from the Phase 2–3 harness (never bespoke).
- One full internal review pass against the §6 attack list before the deadline.
- Venue call: SIGGRAPH 2027 if the headline demo lands; else SIGGRAPH Asia 2027 (May deadline buys 14 weeks) or SGP for the fallback scope — a deliberate decision at the gate, not a January scramble.

---

Related: [[The induced metric of a displaced surface]] · [[Obliquity and the integrability defect]] · [[Laplace–Beltrami on displaced surfaces]] · [[Min-max mipmap and conservative bounds]] · [[Surface measure and sampling on implicit displaced surfaces]] · [[MOC — Open Questions]] · [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] · [[Zhang et al. 2026 — DJM]] · [[Shell, prism and prismoid]] · [[Watertightness and cracks]]
