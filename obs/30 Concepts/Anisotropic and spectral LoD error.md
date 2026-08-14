---
title: Anisotropic and spectral LoD error
tags: [concept, derivation, lod, metric, distortion, spectral, texture-space]
created: 2026-08-11
---

# Anisotropic and spectral LoD error

The derivation behind application C of [[Project — Conservative metric queries without tessellation|the metric-queries project]]: what it means, geometrically, to render a coarser mip level of a displacement map, how to measure that as a $2\times2$ closed form, how the [[Taylor-model bound pyramid]] certifies it per region, and how the same certificate bounds the Laplace–Beltrami spectrum without computing one. Notation follows [[The induced metric of a displaced surface]].

> [!note] Derivation, not a claim from any paper
> The distortion machinery (singular values of a tangent map, log-based energies, quadratic-form eigenvalue sandwiches) is classical — §7 sorts ownership. Its application to the mip-level correspondence of a displacement map, and the certified bounds, are derived here.

---

## 1. LoD levels share a parameterization — that is the whole setup

Let $S = P + hN$ be the surface at the finest level and $S_k = P + h_k N$ the surface rendered at mip level $k$ ($h_k$ the level-$k$ interpolant). Both are maps from the **same** parameter domain, so there is a canonical correspondence between them — the identity in $(u,v)$ — and "what does level $k$ lose?" becomes a precise question: **how distorted is the map $\varphi_k = S_k \circ S^{-1}$ from the fine surface to the coarse one?**

No tessellated LoD scheme gets this for free: mesh decimation must *construct* a correspondence (and papers differ in how). Here the representation supplies it. All first-order distortion of $\varphi_k$ at a point is carried by one object, the **relative stretch tensor**

$$
\mathbf{D} \;=\; \mathbf{G}^{-1/2}\, \mathbf{G}_k\, \mathbf{G}^{-1/2}
\qquad\text{(SPD, eigenvalues } \sigma_1^2 \ge \sigma_2^2 > 0\text{)}
$$

where $\mathbf{G}, \mathbf{G}_k$ are the two metrics from the master formula — same base constants, different $(h, \nabla h)$ versus $(h_k, \nabla h_k)$. The $\sigma_i$ are the principal stretches of $\varphi_k$: $\sigma_i = 1$ means isometry (nothing lost), and the eigenvectors say **in which direction** the surface is being stretched or flattened. Everything is computable from two invariants of a $2\times2$ pencil, never from an eigen-solver:

$$
T = \operatorname{tr}(\mathbf{G}^{-1}\mathbf{G}_k),
\qquad
\Delta = \frac{\det \mathbf{G}_k}{\det \mathbf{G}},
\qquad
\sigma_{1,2}^2 = \tfrac12\!\left(T \pm \sqrt{T^2 - 4\Delta}\right).
$$

---

## 2. The error density, and its two orthogonal channels

Define the pointwise error as the log-Frobenius distortion

$$
\varepsilon(u,v) \;=\; \lVert \log \mathbf{D} \rVert_F ,
\qquad
\varepsilon^2 = 2\left(a^2 + s^2\right),
\qquad
a = \tfrac12 \log \Delta,
\quad
s = \log(\sigma_1/\sigma_2).
$$

The split is exact (from $\log\sigma_{1,2} = \tfrac{a \pm s}{2}$) and each channel means something:

- $a$ — the **area channel**: log ratio of area elements. Positive = the coarse level inflates area, negative = it erases it (the usual case: smoothing kills slope, slope carries area).
- $s$ — the **anisotropy channel**: how unevenly the two principal directions are treated. $s > 0$ with $a = 0$ is pure shear — area perfectly preserved while the geometry visibly warps.

Why the log form and not $\lVert \mathbf{G} - \mathbf{G}_k \rVert$: $\varepsilon$ is the affine-invariant (geodesic) distance on SPD matrices, so it is **symmetric in the two levels, penalises shrink and stretch equally, and is invariant to reparameterizing $(u,v)$** — both metrics transform congruently and $\mathbf{D}$'s eigenvalues survive. The error cannot depend on UV layout. A raw Frobenius difference fails all three.

**Why existing criteria are strictly weaker.**

- *Height error* (what min-max mipmaps give): bounds extrinsic **position** drift, which is real (silhouettes shift) but blind to the damage that matters at LoD time. The canonical failure: smoothing a ripple of amplitude $\delta$ and frequency $\omega$ costs height error $\delta$ (tiny) but slope error $\omega\delta$ (huge) — normals, roughness, parallax, and area all collapse while the height criterion reports almost nothing. Keep height error as the *position* budget; $\varepsilon$ is the *geometry* budget. They are complementary, not rivals.
- *Determinant-only* ([[Zhang et al. 2026 — DJM|DJM]]-flavoured): sees $a$, blind to $s$. A pure shear ($\sigma_1 = 1/\sigma_2$) reports zero error.

**The secondary term.** $\varepsilon$ measures the tangent plane; the **normal tilt** $\theta_k = \angle(n, n_k)$ — both normals cheap from $(\nabla h, \nabla h_k)$ — is the shading-facing complement. A complete LoD criterion budgets $(\varepsilon, \theta_k, \text{height})$; and what filtering removes from the *distribution* of normals is the LEADR/appearance-prefiltering story (Dupuy et al. 2013; Wu et al. 2019) — geometric LoD certified by $\varepsilon$ pairs naturally with LEADR-style roughness compensation on the appearance side.

**One machinery, two comparisons.** Nothing above used that $\mathbf{G}_k$ is a mip level: $\mathbf{D}(\mathbf{G}_a, \mathbf{G}_b)$ compares any two metrics over the shared domain. With $\mathbf{G}_b = \mathbf{G}_0$ it measures *how much geometry displacement adds to the base* — a bake-time content diagnostic beside [[Obliquity and the integrability defect|obliquity and δ]]. With $\mathbf{G}_b = \mathbf{G}_k$ it measures *how much LoD removes*, which is this note.

---

## 3. Certified per-region bounds from the pyramid

Pointwise $\varepsilon$ is six dot products and two square roots. The pyramid upgrades it to a **certificate**: an interval $[\varepsilon_{\text{lo}}, \varepsilon_{\text{hi}}]$ over a region, so "no point in this region exceeds the budget" is a guarantee, not a sample.

Over one level-$k$ cell, both surfaces have Taylor models with a crucial asymmetry:

- the **coarse** surface: $h_k$ on its own cell is a *single bilinear patch*, so its Taylor model is **exact** and closed-form ([[Taylor-model bound pyramid|Taylor note]] §3);
- the **fine** surface: the folded pyramid node at level $k$ gives a *conservative* model of the finest interpolant over the same region.

Propagate both through the master formula with the **same** base constants and **shared** position symbols — the correlations cancel exactly where the two levels agree, so the certified interval collapses toward zero wherever the mip lost nothing. Affine forms for the entries of $\mathbf{G}, \mathbf{G}_k$ give intervals for $T$ and $\Delta$, hence for $\sigma_i^2$, $a$, $s$, $\varepsilon$. Fold cell certificates upward and any node of the pyramid carries a certified error for choosing level $k$ over its whole footprint.

This also resolves the [[Taylor-model bound pyramid|Taylor note]]'s LoD-semantics caveat for this application: no "adjusted" recursion is needed, because each level's own cells carry their own exact models — the caveat only bites structures that try to bound *all* levels from one channel.

---

## 4. Selection: a certified, view-aware level map

Selection runs on **two budgets**, because the two error kinds fail differently on screen:

| Budget | Bounds | Guards against | Test |
|---|---|---|---|
| **Position**, $\tau_{\text{px}}$ | certified $\max\lvert h - h_k\rvert$ per node (world units) | silhouette shift, contact gaps | $\alpha\, e_{\text{pos}}(n,k)\cdot \text{pxPerWorld}(z) \le \tau_{\text{px}}$ |
| **Stretch**, $\tau_\varepsilon$ | certified $\varepsilon$ (or its view-weighted form) per node | shading, parallax, texture warp, spectra | $\varepsilon_{\text{view}}(n,k) \le \tau_\varepsilon$ |

**Baked per $(n, k)$:** the invariants $(a, s)$ and the major eigendirection angle $\theta_D$ of $\mathbf{D}$ in parameter space (three scalars, quantizable to 8–16 bits — the log-stretches reconstruct as $\log\sigma_{1,2} = \tfrac{a\pm s}{2}$), plus the position bound $e_{\text{pos}}$. The normal cone $(\bar n, \beta)$ per node already exists for §5A of the project.

> [!warning] View weighting must not discount silhouettes
> The "screen-compressed direction is less visible" argument is true for *interior* shading and parallax but **inverts at silhouettes**: grazing views are the most error-sensitive, not the least. Classify per node with the normal cone — if the cone straddles the grazing set ($|\bar n \cdot v| < \sin\beta + \zeta$), the node may contain a silhouette: apply the **full** $\varepsilon$ and lean on the position budget; only cleanly interior nodes earn the directional discount. Discounting everywhere is a subtle quality bug, not an optimization.

**Per-frame selection pass** (Nanite-style compute over visible nodes; rays then only read the result):

```
// bake, per face:  a(n,k), s(n,k), θD(n,k), epos(n,k);  cone(n) = (n̄, β)
selectLevels(face T, camera, τpx, τε):
  stack ← { root(T) }
  while n ← pop(stack):
    z   ← distance(camera, center3D(n));    ppw ← pxPerWorld(camera, z)
    sil ← |n̄(n)·v| < sin β(n) + ζ                       // cone straddles grazing?
    for k = coarsestAllowed(n) .. finest:                // try coarse first
      if I.α · epos(n,k) · ppw > τpx: continue           // position gate, always on
      (e1, e2, w1, w2) ← reconstruct(a, s, θD)(n,k)      // log-stretches + eigendirs
      if sil:
        ε ← √(2(a² + s²))                                // full error at silhouettes
      else:                                              // interior: view weighting
        p_i ← ‖P_view · (w_i,u S_u + w_i,v S_v)‖  i=1,2  // px per unit step along w_i
        c_i ← min(1, p_i / max(p_1, p_2))                // compression factors ≤ 1
        ε ← √2 · ‖(c₁e₁, c₂e₂)‖
      if ε ≤ τε:
        λ ← k + fraction from (ε_k, ε_{k+1}) crossing τε // fractional level for TFDM blend
        levelMap[n] ← λ;  break
    if none accepted and n above finest granularity: push children(n)
// hysteresis: switch stored levels only when ε crosses τε by a margin (both directions)
```

Costs, by regime: with the **fused bake** (static asset, $\alpha = 1$) the whole inner test is a fetch, a reconstruct, and a few dot products — negligible beside the per-node affine box math traversal already pays. Under **instancing with amplitude $\alpha \ne 1$**, $e_{\text{pos}}$ scales linearly but $(a, s, \theta_D)$ do not — recompose them in the selection pass from the two levels' Taylor models (two closed-form metric evaluations per node per candidate level); still a per-frame compute-pass cost, not a per-ray one.

**The anisotropic-footprint hook.** The square-texel pyramid forces one level per region; a *directional* error over *anisotropic* regions is exactly the query shape [[Thonat et al. 2023 — RMIP|RMIP]] was built for — its rectangular range queries could serve a per-direction level choice (the LoD analogue of anisotropic texture filtering, coarse along the compressed axis only). Extension, not baseline; noted here because the machinery lines up unusually well.

---

## 5. Simplification: metric-aware mip fitting

Standard mip generation box-filters $h$ — optimal for the *signal*, indifferent to the *geometry*. Since $\varepsilon$ is now measurable, coarse levels can be **fit** instead of filtered. Three constructors, each a drop-in producing the same texture format:

**(i) Box / B-spline filter** — the baseline; minimizes height $L^2$ and nothing else.

**(ii) $H^1$ fit — linear, gradient-preserving.** ($H^1$ = the Sobolev norm measuring closeness of *values and first derivatives* together — $L^2$ preserves where the surface is, $H^1$ also preserves which way it is facing.) Unknowns are the level-$k$ texels $c$; the level-$k$ interpolant is $h_c = \sum_j c_j B_j$ with $B_j$ the coarse-grid basis (bilinear hats), so both $h_c$ and $\nabla h_c$ are **linear in $c$**. Minimize, with quadrature points $x_q$ at finest-texel density,

$$
\mathcal{E}_{\text{fit}}(c) \;=\; \sum_q \; \beta\,\bigl(\nabla h_c - \nabla h\bigr)^\top \mathbf{G}_0^{-1} \bigl(\nabla h_c - \nabla h\bigr)\Big|_{x_q} \;+\; \alpha\,\bigl(h_c - h\bigr)^2\Big|_{x_q}
$$

($\mathcal{E}_{\text{fit}}$ is the **fitting energy** — the scalar objective, one cost number per candidate $c$ — named so that $J$ stays free for the actual Jacobians in this note: §4's $J_S$ and rung (iii)'s $\partial\mathbf{G}/\partial h$.) With $\alpha \ll \beta$: the $\beta$ term preserves the slope part $g\,g^\top$ of the metric to first order (the term box filtering destroys), the $\alpha$ term anchors position. The $\mathbf{G}_0^{-1}$ weighting makes the fit *base-aware* — parameter-space gradient error is measured as the surface stretch it actually induces on this face's (possibly anisotropic) base triangle; plain Euclidean weighting is the acceptable MVP. The normal equations $(\beta K + \alpha M)\,c = \text{rhs}$ are sparse SPD with a screened-Poisson-like stencil on the coarse grid — small per tile, direct or CG.

**(iii) Metric fit — Gauss–Newton reference.** Near identity, $\log \mathbf{D} \approx \mathbf{D} - \mathbf{I}$, so minimize the first-order surrogate $\sum_q \lVert \mathbf{G}^{-1/2}(\mathbf{G}_c - \mathbf{G})\mathbf{G}^{-1/2}\rVert_F^2$ — quadratic in $\mathbf{G}_c$, and $\mathbf{G}_c$ quadratic in $(h_c, \nabla h_c)$, so Gauss–Newton with the analytic Jacobian ($\partial\mathbf{G}/\partial h = -2\mathbf{B}_0 + 2h\mathbf{C}_0$, plus the slope and obliquity terms in $\nabla h$), initialized from (ii), a handful of iterations per tile. This variant also sees the offset and obliquity couplings that (ii)'s slope-only view misses.

**What each rung is tied to.** The constructors consume different amounts of face geometry, which decides where each may be used:

| Constructor | Base surface | Amplitude $\alpha$ | Shared / tiled maps |
|---|---|---|---|
| (i) box | no | equivariant | yes |
| (ii) $H^1$, Euclidean | no | **equivariant** — the fit is linear, so fitting $\alpha h$ gives $\alpha\times$ the fit of $h$; one texture serves all amplitudes | yes |
| (ii′) $H^1$, $\mathbf{G}_0^{-1}$-weighted | weighting only | equivariant | no — unless weighted by the *aggregate* $\sum \mathbf{G}_0^{-1}$ of the faces using the tile |
| (iii) metric GN | fully | **no** — $\mathbf{G}$ mixes degrees 1 and 2 in $h$; optimal only at the fitted $\alpha$ | no |

A texture has one set of texel values, so whatever shares it shares the fit: per-face Ptex tiles (1:1 binding) take (ii′)/(iii) naturally; shared detail tiles take (i)/(ii) only, or the aggregate-weighted compromise. This is the same fused-vs-composed axis as §5A's sampling weights — and it decouples from certification: the $\varepsilon$-certificates are *always* base-dependent, so base-agnostic mips still get certified, just with composed-at-query-time certificates.

**Watertightness under fitting — the one constraint that is not optional.** Per-face tiles stay crack-free by sharing edge rows (§4 of the project); an unconstrained per-tile fit would break that. Fit **edges first, interiors second**:

```
fitMips(asset):
  for k = 1..K:                                   // each level fit against the FINEST
    for each shared edge e (once per edge):        //   — no error cascade across levels
      c_edge(e) ← 1D H¹ fit along e, using fine h from BOTH adjacent faces
    for each face tile (parallel):
      solve (βK + αM) c = rhs   with edge rows fixed as Dirichlet values
      optional: Gauss–Newton metric polish (iii), edge rows still fixed
  rebuild min-max / adjusted / Taylor pyramids from the FITTED levels
  bake certificates (a, s, θD, epos)(n, k) against the finest level
```

The pipeline order matters twice: fitting every level against the *finest* (not the previous level) prevents error accumulating down the chain, and the bound pyramids and $\varepsilon$-certificates must be rebuilt **from the fitted values** — bounds describe content, and the content changed. (Fitted values may overshoot the local height range slightly; the rebuilt bounds cover this automatically, at worst thickening the shell — clamp to the local min-max if that matters.)

The evaluation question is then concrete: at equal memory, how much lower do the certified $\varepsilon$ and the rendered FLIP error sit for (ii)/(iii) mips than for (i) — and how much coarser can the certified level map go under the same budgets?

---

## 6. The spectral certificate — why "anisotropic" and "spectral" are one claim

Suppose the certified bound gives $\max_i |\log \sigma_i^2| \le \delta$ everywhere on the rendered level map. That is precisely the quadratic-form sandwich $e^{-\delta}\,\mathbf{G} \le \mathbf{G}_k \le e^{\delta}\,\mathbf{G}$, and eigenvalues of the Laplace–Beltrami operator obey Courant–Fischer over Rayleigh quotients

$$
R_k(f) = \frac{\int \nabla f^\top \mathbf{G}_k^{-1} \nabla f \,\sqrt{\det\mathbf{G}_k}}{\int f^2 \sqrt{\det\mathbf{G}_k}} :
$$

the energy is sandwiched within $e^{\pm 2\delta}$ (one $e^{\pm\delta}$ from $\mathbf{G}_k^{-1}$, one from $\sqrt{\det}$), the mass within $e^{\pm\delta}$, so **every** eigenvalue satisfies

$$
e^{-3\delta} \;\le\; \frac{\lambda_i^{(k)}}{\lambda_i} \;\le\; e^{3\delta}
\qquad \text{for all } i .
$$

A certified stretch bound is simultaneously a certificate that the coarse level preserves the *entire* vibration spectrum — diffusion behaviour, heat-method geodesics, spectral descriptors — to a known factor, **without ever computing a spectrum**. (The constant 3 is a worst case, not tight.)

The refinement worth a highlighted line: in 2D the Dirichlet energy is **conformally invariant** — a pure area change ($s = 0$, $\mathbf{G}_k = e^{2a}\mathbf{G}$) leaves the energy untouched and perturbs only the mass, tightening the sandwich to $e^{\pm\delta}$. It is the **anisotropy channel $s$ that attacks the conformal structure and hence the spectrum**; the area channel only rescales. So the split of §2 is not cosmetic: $s$ is the spectrally dangerous component, $a$ the benign one — "anisotropic LoD error" and "spectral LoD" are the same quantity seen from two sides.

Scope honestly: the eigenvalue statement is global, so $\delta$ is the sup over the rendered level map; per-region budgets shape *where* the sup binds.

---

## 7. Classical vs new

**Classical — cite it.** Singular values of a tangent map and log-based distortion energies (the parameterization literature: MIPS, symmetric Dirichlet, ARAP and successors); the affine-invariant SPD distance; Courant–Fischer and eigenvalue comparison under quadratic-form sandwiches; 2D conformal invariance of Dirichlet energy; appearance prefiltering of displacement (LEADR — Dupuy et al. 2013; Wu et al. 2019); appearance-preserving simplification via texture deviation (Cohen et al. 1998); spectral-preserving operator coarsening (Liu et al. 2019, given a fine mesh).

**Believed new here.** 1) The mip-level correspondence of a displacement map as the measured object — the shared parameterization makes the fine↔coarse map canonical, which decimation-based LoD never has. 2) **Certified** per-region $[\varepsilon_{\text{lo}}, \varepsilon_{\text{hi}}]$ via the exact-coarse-model / folded-fine-model asymmetry of §3. 3) The spectral certificate for mip selection, with the $s$-is-the-spectral-threat refinement. 4) $H^1$/metric-fit mip construction for displacement maps.

**Check first.** Terrain-rendering and clipmap literature for gradient-preserving downsampling (claim 4's most likely precedent); Nanite's screen-space geometric error and the game-LoD literature for anything metric-based; the spectral-coarsening line (Jacobson et al.) for claim 3's discrete cousin; LEADR follow-ups for slope-moment filtering framed as geometry rather than appearance.

---

Related: [[The induced metric of a displaced surface]] · [[Taylor-model bound pyramid]] · [[Laplace–Beltrami on displaced surfaces]] · [[Obliquity and the integrability defect]] · [[Min-max mipmap and conservative bounds]] · [[Zhang et al. 2026 — DJM]] · [[Project — Conservative metric queries without tessellation]]
