---
title: Laplace–Beltrami on displaced surfaces
tags: [concept, differential-geometry, pde, laplacian, algorithm, texture-space]
---

# Laplace–Beltrami on displaced surfaces

[[The induced metric of a displaced surface|The metric]] is worth having because of what it unlocks, and almost all of that runs through one operator. This note is about getting $\Delta$ out of $\mathbf{G}$ and actually solving with it.

> [!note] Derivation, not a claim from any paper
> §1 is standard. §2–§4 apply standard machinery to this representation; §5 splits textbook from new. Notation follows [[The induced metric of a displaced surface]].

> [!info] Index conventions
> $i, j$ range over $\{u, v\}$ and repeated indices are summed, so $\mathbf{G}^{ij}\partial_j f$ means $\mathbf{G}^{iu}\partial_u f + \mathbf{G}^{iv}\partial_v f$. **Raised indices are inverse-matrix entries, not reciprocals**: $\mathbf{G}^{uu} = \mathbf{G}_{vv}/\det\mathbf{G}$, not $1/\mathbf{G}_{uu}$. Everything is written longhand in §1. In the pseudocode, $p$ and $q$ are **vertex** indices — unrelated to $i, j$.

---

## 1. What $\Delta$ is

**The intuition.** In the plane, $\Delta f = f_{uu} + f_{vv}$. The useful reading is not "second derivatives" but

$$
\Delta f(p) \;\;\approx\;\; \bigl(\text{average of } f \text{ on a small circle around } p\bigr) \;-\; f(p)
$$

up to a positive scale. So $\Delta f > 0$ means *$p$ sits in a dip*, $\Delta f < 0$ a bump, $\Delta f = 0$ that $f$ is exactly its neighbourhood average.

That one reading explains the reach: diffusion is $\partial_t f = \Delta f$, every point drifting toward its neighbours; the Poisson equation $\Delta\varphi = \rho$ is the final step of the heat method for geodesics; and the eigenfunctions of $\Delta$ are the surface's vibration modes — the Fourier basis, generalised.

**Why a curved surface needs the metric.** You cannot just take second partials in $(u,v)$, because equal steps in parameter space are not equal steps on the surface. A texel on a steep displacement ridge covers far more surface area than one on a flat patch. "Average of my neighbours" must mean *genuine surface neighbours, weighted by genuine surface area*, and that correction is exactly what $\mathbf{G}$ supplies:

$$
\Delta f \;=\; \frac{1}{\sqrt{\det \mathbf{G}}}\; \partial_i \!\left( \sqrt{\det \mathbf{G}} \; \mathbf{G}^{ij}\, \partial_j f \right)
$$

Read as $\operatorname{div}(\operatorname{grad} f)$ with both operators metric-aware. The $\mathbf{G}^{ij}\partial_j f$ part converts "change per unit parameter" into "change per unit *surface* distance" and rotates the steepest-ascent direction to account for stretching; the $\sqrt{\det\mathbf{G}}$ factors weight each parameter cell by its true area.

**Longhand**, with no summation convention and no raised-index shorthand:

$$
\Delta f = \frac{1}{\sqrt{\det \mathbf{G}}}
\left[
\partial_u \!\Bigl( \sqrt{\det \mathbf{G}} \,\bigl( \mathbf{G}^{uu} f_u + \mathbf{G}^{uv} f_v \bigr) \Bigr)
\;+\;
\partial_v \!\Bigl( \sqrt{\det \mathbf{G}} \,\bigl( \mathbf{G}^{vu} f_u + \mathbf{G}^{vv} f_v \bigr) \Bigr)
\right]
$$

$$
\det \mathbf{G} = \mathbf{G}_{uu}\mathbf{G}_{vv} - \mathbf{G}_{uv}^2,
\qquad
\mathbf{G}^{uu} = \frac{\mathbf{G}_{vv}}{\det \mathbf{G}},
\qquad
\mathbf{G}^{uv} = \mathbf{G}^{vu} = \frac{-\mathbf{G}_{uv}}{\det \mathbf{G}},
\qquad
\mathbf{G}^{vv} = \frac{\mathbf{G}_{uu}}{\det \mathbf{G}}
$$

> [!tip] Two things people trip on
> **$\Delta f$ is a scalar** — one number per point, same type as $f$. The intermediate $\mathbf{G}^{ij}\partial_j f$ is a vector: $\operatorname{grad}$ raises a scalar to a vector, $\operatorname{div}$ drops it back.
> **Raised indices are inverse entries, not reciprocals.** $\mathbf{G}^{uu} \neq 1/\mathbf{G}_{uu}$ unless the metric is diagonal.

Sanity check: if $\mathbf{G} = \mathbf{I}$ then $\det\mathbf{G} = 1$, the off-diagonal inverse entries vanish, and it collapses to $f_{uu} + f_{vv}$.

**Why this is the payoff.** $\Delta$ depends on nothing but $\mathbf{G}$, and displacement changes nothing but $\mathbf{G}$. So $\nabla h \to \mathbf{G} \to \Delta \to$ geodesics, diffusion, spectra, smoothing. That chain is why "bound the gradient" is a bigger claim than "compute the area".

---

## 2. Assembling it: one recipe, two sources of edge lengths

A **stencil** is the pattern of neighbouring cells used to compute one output, plus their weights — an image convolution kernel, except the weights vary per cell because they depend on the local metric. The familiar flat case is the 5-point stencil, "sum of the four neighbours minus four times me, over $\Delta u^2$", which is §1's intuition discretised. What is built below is **7-point** — centre plus six neighbours — because a triangulated grid adds the diagonal, and those two extra arms are what carry off-diagonal $\mathbf{G}_{uv}$ anisotropy.

### The unifying observation

The **cotangent Laplacian** is *intrinsic*: its weights depend only on **edge lengths**, never on vertex positions, because the angles follow from the lengths by the law of cosines. And edge lengths follow from the metric — a parameter-space edge vector $e$ has true surface length

$$
\ell^2 = e^\top \mathbf{G}\, e .
$$

So both substrates share one assembly, and differ only in where the lengths come from.

```
// shared core: cotan Laplacian from edge lengths alone.
// p, q are VERTEX indices of the triangulation, unrelated to the
// tensor indices i,j of §1, which range over {u,v}.

for each triangle (A,B,C) with opposite edge lengths la, lb, lc:
    Ar   = heronArea(la, lb, lc)
    cotA = (lb*lb + lc*lc - la*la) / (4*Ar)   // angle at A, opposite edge a = BC
    cotB = (lc*lc + la*la - lb*lb) / (4*Ar)
    cotC = (la*la + lb*lb - lc*lc) / (4*Ar)

    w[B,C] += cotA / 2      // interior edges collect one term per adjacent
    w[C,A] += cotB / 2      // triangle, so two in total; boundary edges get one
    w[A,B] += cotC / 2

    for p in {A,B,C}:  area[p] += Ar / 3      // barycentric lumped mass

// p ranges over vertices, q over the 1-ring neighbours of p
Δf[p] = (1/area[p]) * Σ_{q ∈ N(p)} w[p,q] * (f[q] - f[p])
```

### Path A — µ-meshes: the triangulation already exists

Micro-vertex positions are $S(u,v) = P + hN$ evaluated at the barycentric micro-vertex parameters — cheap and exact, computed on demand rather than stored. Edge lengths are ordinary Euclidean distances.

```
Pa = S(ua,va);  Pb = S(ub,vb);  Pc = S(uc,vc)
la = |Pb - Pc|;  lb = |Pc - Pa|;  lc = |Pa - Pb|
→ shared core
```

**The metric is not needed for the operator here.** Its value on this substrate is elsewhere: bounds, level-of-detail error, shell validity. Worth being honest about — PDEs on µ-meshes are essentially available today, which makes this the cheap first result rather than the hard one.

### Path B — tessellation-free: there is no triangulation

$h$ is a continuous texture field with no canonical micro-mesh, so the operator must be built from the metric. Triangulate the texel grid — split each quad along a consistent diagonal — and take lengths from $\mathbf{G}$:

```
for each texel-grid triangle with parameter-space edge vectors e1, e2, e3:
    Gm = metricAt(centroid)              // or a conservative bound over the cell
    la = sqrt(e1ᵀ * Gm * e1)
    lb = sqrt(e2ᵀ * Gm * e2)
    lc = sqrt(e3ᵀ * Gm * e3)
    → shared core
```

**This is where the metric earns its keep** — and §4 shows it is also the harder substrate.

> [!tip] Why not finite volume
> The natural first instinct is a finite-volume scheme: integrate over each texel, use the divergence theorem to turn the area integral into a flux through the cell's faces, and approximate each face flux as $w\,(f_q - f_p)$. That is a sound derivation and it is how computational fluid dynamics builds such operators. But the simple two-point flux is exact only when the metric is **diagonal in the grid directions**, and displaced metrics have $\mathbf{G}_{uv} \neq 0$ generically — the $\nabla h \nabla h^\top$ term is rank one and aligned with the *slope*, not the grid. Recovering the cross term needs a multi-point flux scheme. The cotan route handles that anisotropy natively and lands on machinery every geometry-processing library already has.

---

## 3. Where the unknowns live, and how you query

> [!important] $\Delta$ is not a pointwise query
> Every other query in this vault — ray hit, closest point, coverage — answers at a point and returns. $\Delta$ is a differential operator: it needs $f$ over a neighbourhood, and in practice you never want $\Delta f$ itself. You want to **solve** something containing it: $\partial_t f = \Delta f$ for diffusion, $\Delta\varphi = \nabla\!\cdot\! X$ for the heat method's last step. That means a linear system over many unknowns, not a lookup. This is the structural difference between this agenda and the ray-query agenda.
>
> One genuine exception: **pointwise Monte Carlo solvers.** [[Sugimoto et al. 2024 — Projected Walk on Spheres|Projected Walk on Spheres]] estimates the solution *at a single point* by repeated closest-point projections in a tubular neighbourhood — no global assembly, no cross-chart stencils, and therefore none of §4's seam problem. It consumes certified proximity queries and a valid tube radius rather than the operator itself. See [[Project — Conservative metric queries without tessellation|the project note]], application B2.

For tessellation-free displacement the real question is *where the degrees of freedom live*, and since no triangulation exists you must **choose**. The texel lattice at a chosen mip level is the obvious choice, and once chosen it **is** the mesh: a regular grid per base triangle, each quad split along a consistent diagonal.

- **Mip level is the discretisation resolution** — the refinement knob, with the multigrid hierarchy already built underneath it.
- **$f$ lives in texture space beside $h$.** Displacement texture carries $(h, h_u, h_v)$, a solution texture carries $f$; same lattice, same addressing.

The flow is assemble, solve, sample:

```
// if the query arrives as a 3D point, get into parameter space first
(tri, u, v) = invertToParametric(x)    // RMIP Newton inversion, converges inside the prism
level       = chooseMipLevel(...)       // = discretisation resolution
(s, t)      = texelCoord(u, v, level)

// 1. assemble   w[p,q] and area[p] over the region, edge lengths from G   (§2 Path B)
// 2. solve      matrix-free conjugate gradient, or multigrid over the mip pyramid
// 3. sample     interpolate the solution texture at any (u,v) — this part IS pointwise
```

Steps 1 and 2 are regional and carry the cost; step 3 is the cheap lookup.

**Local versus global decides how much the seam problem matters.** Short-time diffusion and small-$t$ heat-method solves are local — solve on a patch with Dirichlet data on its boundary and never touch a seam. Geodesic distance to a distant source needs a global solve, and therefore needs §4. Decide which you need early.

> [!warning] Two hazards specific to Path B
> Both arise from sampling $\mathbf{G}$ once per cell.
> **The triangle inequality can fail.** If $\mathbf{G}$ varies sharply across a grid triangle, the three computed lengths may not form a valid triangle and the Heron area goes imaginary. Refine the mip level, or evaluate $\mathbf{G}$ per edge rather than per triangle.
> **Cotan weights can go negative**, when the metric makes a grid triangle obtuse — which anisotropic displacement does even on a perfectly regular lattice. You lose the maximum principle and diffusion can undershoot. The standard remedy is intrinsic Delaunay flips.

### What both paths inherit

**Only first derivatives of $h$ are needed.** Edge lengths need $\mathbf{G}$, and $\mathbf{G}$ needs $\nabla h$ — not derivatives of $\mathbf{G}$. So the heat method for geodesics (diffuse, normalise the gradient field, solve one Poisson equation) runs on $\nabla h$ alone, while marching geodesics directly needs Christoffel symbols and therefore a second-derivative pyramid. Strong argument for preferring the heat method *in this representation specifically*.

**The mipmap is your multigrid hierarchy.** Restriction and prolongation are mip operations you already have, and the operator is matrix-free.

**The base mesh is your coarse operator.** $\mathbf{G} = \mathbf{G}_0 + \text{corrections}$ with $\mathbf{G}_0$ constant per triangle, so the base-mesh cotan Laplacian is a natural preconditioner — a few thousand triangles — while the correction stays texture-local.

---

## 4. Seams, and which substrate wins

Inside a triangle this is all smooth. Across a base edge two charts meet and the operator must couple them, which needs a correspondence between the boundary rows of the two grids.

- **µ-meshes hand it to you.** Bit-exact watertight edge subdivision with matched micro-vertex counts means the two sides share vertices outright, so the cotan assembly simply continues across the seam. A second instance of the argument in [[Proximity and contact queries against micro-geometry]] that [[Watertightness and cracks|watertightness]] is a capability rather than a quality metric — and here it is load-bearing, because without it the operator is not well defined at all.
- **Tessellation-free over arbitrary UV charts does not.** Seams carry mismatched sampling on both sides; you need seam-aware layout (see [[Authoring, UV layout and micromap efficiency]]) or a non-conforming mortar coupling. This is where the idea can die, so test it first.

> [!important] The tension, stated rather than smoothed over
> **The substrate where the operator is easy is not the substrate where the metric is necessary.** µ-meshes solve seams and need no metric to assemble $\Delta$; tessellation-free needs the metric for everything and has no seam story.
> Two consequences: build the µ-mesh version first because it is nearly free, and treat the tessellation-free seam problem as the actual research risk.

The agenda also survives the withdrawal of displaced micro-mesh hardware, since µ-meshes remain perfectly usable in software — the [[MOC — Open Questions|MOC]]'s own argument about optimised-then-withdrawn representations applies directly.

---

## 5. Classical vs new

**Classical — cite it.** Laplace–Beltrami in coordinates. The cotangent Laplacian and its intrinsic character — weights from edge lengths alone — which is the basis of the intrinsic triangulations line. The heat method for geodesic distance. Finite-volume discretisation and its two-point-flux limitation. Intrinsic Delaunay flips.

**Believed new here.**

1. Taking edge lengths from $\mathbf{G}$ via $\ell^2 = e^\top\mathbf{G}e$ so that a **tessellation-free displacement field carries a cotan Laplacian without any triangulation of its own** — with the texel lattice at a chosen mip level serving as the degrees of freedom.
2. **The mipmap as a ready-made multigrid hierarchy**, and the base-mesh cotan Laplacian as the natural coarse operator and preconditioner.
3. The observation that **the heat method needs only first derivatives of $h$**, so geodesics are reachable from the same two pyramid channels the metric already requires, with no second-derivative structure.
4. The **substrate tension** of §4: seams solved where the metric is unnecessary, metric necessary where seams are unsolved.

**Check first.** Intrinsic triangulations (Sharp, Soliman & Crane), which is where claim 1 is most likely to have been anticipated and which DJM's bibliography already cites. Geometry images (Gu, Gortler & Hoppe 2002) and descendants. Laplace–Beltrami on digital surfaces and point clouds. [[Sugimoto et al. 2024 — Projected Walk on Spheres]] — now read from the PDF — a consumer of exactly these queries; [[Crane et al. 2013 — Geodesics in Heat]] likewise, for the heat method this note leans on.

---

Related: [[The induced metric of a displaced surface]] · [[Obliquity and the integrability defect]] · [[Watertightness and cracks]] · [[Authoring, UV layout and micromap efficiency]] · [[Micro-triangle and subdivision level]] · [[Surface measure and sampling on implicit displaced surfaces]]
