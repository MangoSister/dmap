---
title: Taylor-model bound pyramid
tags: [concept, bounding, acceleration-structure, affine-arithmetic, derivation, texture-space]
created: 2026-08-11
---

# Taylor-model bound pyramid

The bound structure behind [[Project — Conservative metric queries without tessellation|the metric-queries project]]: a pyramid whose nodes carry a **joint first-order model of $h$ and $\nabla h$**, alongside the min-max height channel it deliberately does not replace. It replaces the naive "two more min-max channels for $h_u, h_v$" proposal that earlier notes floated. This note is the technical spec: why independent gradient channels fail under interval evaluation, the node, exact leaf construction, the conservative fold, propagation to metric bounds, and what changes for existing queries.

> [!note] Derivation, not a claim from any paper
> Taylor models and affine arithmetic are classical validated numerics (§8). Their application to displacement pyramids, the closed-form leaf construction, the fold, and the metric propagation are derived here. Notation follows [[The induced metric of a displaced surface]].

---

## 1. Why independent min-max channels fail

[[Min-max mipmap and conservative bounds|TFDM's argument]] for affine over interval arithmetic is that correlated error terms cancel — $P$, $N$, $h$ all co-vary through position $(u,v)$, and affine forms sharing the position symbols $\varepsilon_u, \varepsilon_v$ keep that. The same argument applies with more force to the metric, where $\nabla h$ appears **twice with different signs available**: positively semi-definite in the slope term $\nabla h \nabla h^\top$, sign-varying in the obliquity coupling $\nabla h\,\mathbf{a}^\top + \mathbf{a}\,\nabla h^\top$. Evaluate $\det \mathbf{G}$ with an *interval* for $\nabla h$ and the two terms take the worst case **independently** — the same physical gradient is assumed large-positive in one term and large-negative in the other, a point that exists nowhere on the surface. The lower bound on $\det\mathbf{G}$ collapses, and can spuriously cross zero: **a false degeneracy alarm precisely in the oblique regime every format uses.**

Phase 1 measured this and sharpened it ([[Result — Phase 1 Taylor pyramid]]). Interval evaluation is 1.6–1.9× wider at fine levels with about twice the false-degeneracy alarms — real, but not the collapse the worst-case argument suggests, because exact interval squares partially compensate. And the failure belongs to interval *evaluation*, not to channel *storage*: min-max channels promoted to centred affine forms at query time (each channel then enters every term with one consistent value) match the joint node's metric bounds. What the joint node still provides is the exact bilinear leaf, the $O(s^2)$ slab of §5–§6 for spatial queries, and one structure serving both; the argument against "two more min-max channels" holds only when they are consumed as intervals.

The second failure is about **width scaling**. Over a cell of half-extent $s$, smooth content has $h$-range dominated by slope: min-max width is $O(s)\cdot\lVert\nabla h\rVert$ — first order, large wherever the surface is tilted, which is everywhere interesting. A model that stores the local plane explicitly pays only the **curvature residue**, $O(s^2)$: on a tilted-but-smooth region the bound width drops by an order of the cell size. This is the same win twice over: tighter metric bounds, and (as a side effect) tighter spatial bounds for the *existing* ray query (§6).

> [!warning] Neither failure argues against *storing* $(h_{\min}, h_{\max})$
> The eight-channel node of §2 is not the ablation this section rejects, and the distinction is easy to lose. A1 — the configuration measured at 1.6–1.9× with twice the false alarms — is three min-max channels **consumed as intervals**, and it carries no slab. The eight-channel node is A3 (the same channels consumed as centred affine forms, measured equivalent to the joint node within 1% at every level) **plus** the slab **plus** the height channel. It adds information and removes none. Note too that midpoint-and-radius *is* the form centred affine propagation wants, and that $(g_u, \rho_u)$ is already a lossless re-parameterization of an exact $[h_{u,\min}, h_{u,\max}]$ — so the gradient channels this section discusses are, as stored, exact min-max channels; what §1 rejects is evaluating them term by term as intervals.
>
> The width-scaling argument likewise does not bite, because it compares two different objects. As a *scalar interval* the slab is always worse than min-max, since collapsing a tilted region to an axis-aligned range costs back the whole $O(s)\cdot\lVert\nabla h\rVert$ term. As a *region in space* the slab is thinner. The $O(s^2)$ win is real for consumers that can use a tilted region, and unavailable to consumers that need a range.

---

## 2. The node

Over a pyramid cell $\Omega$ with centre $(u_0, v_0)$ and half-extents $(s_u, s_v)$, store **eight numbers**: the Taylor model $(h_0, g_u, g_v, r, \rho_u, \rho_v)$ and the exact height range $(h_{\min}, h_{\max})$. The Taylor part has the semantics: for all $(u,v) \in \Omega$, writing $\Delta u = u - u_0$, $\Delta v = v - v_0$,

$$
\bigl|\,h - (h_0 + g_u \Delta u + g_v \Delta v)\,\bigr| \le r,
\qquad
|h_u - g_u| \le \rho_u,
\qquad
|h_v - g_v| \le \rho_v .
$$

Equivalently, as affine forms over noise symbols $\varepsilon_\bullet \in [-1,1]$:

$$
h = h_0 + g_u s_u\,\varepsilon_u + g_v s_v\,\varepsilon_v + r\,\varepsilon_h,
\qquad
h_u = g_u + \rho_u\,\varepsilon_{g_u},
\qquad
h_v = g_v + \rho_v\,\varepsilon_{g_v},
$$

where $\varepsilon_u, \varepsilon_v$ are the **same position symbols** TFDM already uses for the cell's uv domain — this is what stitches $h$ into the correlation structure of $P$ and $N$. The plane coefficients $g$ appear in both $h$ and $\nabla h$: that shared appearance *is* the stored correlation. Where the plane explains the data ($r, \rho \to 0$ — locally smooth displacement), every downstream bound collapses toward the exact value; a pure ramp gives zero-width metric bounds at any cell size.

The remaining two numbers state $h_{\min} \le h \le h_{\max}$ over $\Omega$: the ordinary min-max channel, stored rather than derived. §5 gives the measurement that forced this. The Taylor model *can* express a height range, but collapsing it to one costs the plane's excursion across the cell, which is fatal at the coarse levels a ray traversal starts from.

> [!info] What the model deliberately is not
> Not a least-squares fit, not a compression scheme, not LEADR's moments. Every inequality is **conservative** — the surface is guaranteed inside the slab — and the plane need not be optimal for the bounds to be valid, only for them to be tight.

> [!note] Two height bounds, because there are two shapes of query
> $[h_{\min}, h_{\max}]$ and the slab are not redundant and neither subsumes the other. As a **scalar interval** over the cell, min-max is always at least as tight, by construction. As a **region in space**, the slab is thinner: two planes tilted by $g$, separated by $2r = O(s^2)$, against an axis-aligned extent of $O(s)$ — measured 7.3× thinner at texel cells on real content ([[Result — Pyramid channel study]]). Consumers that need a range over a cell (coarse traversal building a prism, the LoD position budget) read min-max; consumers that can use a tilted region (fine traversal, closest point, feature size) read the slab.

---

## 3. Leaf construction — closed form, exact for bilinear

**Bilinear interpolation** over one texel is, in centred coordinates,

$$
h = c_0 + c_1 \Delta u + c_2 \Delta v + c_3\, \Delta u\, \Delta v
$$

with the $c_i$ linear combinations of the four corner texels. Then the tightest node is **exact**:

$$
(h_0, g_u, g_v) = (c_0, c_1, c_2),
\qquad
r = |c_3|\, s_u s_v,
\qquad
\rho_u = |c_3|\, s_v,
\qquad
\rho_v = |c_3|\, s_u,
$$

since $h_u = c_1 + c_3 \Delta v$ and $h_v = c_2 + c_3 \Delta u$, and all suprema are attained at cell corners. The height channel is exact for the same reason — a bilinear patch has a saddle at its interior critical point and is linear on each edge, so $h_{\min}$ and $h_{\max}$ are simply the min and max of the four corner texels. No estimation anywhere.

**Biquadratic/bicubic B-spline** $h$ is polynomial per knot cell; take $(h_0, g) = (h, \nabla h)$ at the centre and bound the residues by the second-order terms:

$$
r \le \tfrac12 M_{uu} s_u^2 + M_{uv} s_u s_v + \tfrac12 M_{vv} s_v^2,
\qquad
\rho_u \le M_{uu} s_u + M_{uv} s_v,
\qquad
\rho_v \le M_{uv} s_u + M_{vv} s_v,
$$

with $M_{ij} = \sup_\Omega |h_{ij}|$ available in closed form from the polynomial coefficients. Gradient discontinuities across texel edges (bilinear) need no special case: each leaf is exact for its own cell, and the jumps surface in the fold as inter-child plane disagreement (§4).

---

## 4. The fold — one bottom-up pass

Children $c \in \{00, 10, 01, 11\}$ have half-extents $(s_u/2, s_v/2)$ and centres at offsets $\delta_c = (\pm s_u/2, \pm s_v/2)$ from the parent centre. Any parent plane gives a valid node. The implemented choice (Phase 1; it replaced a child-mean plane and cut metric bound widths by roughly a third at mid levels) is built from interval hulls, per channel:

- **Gradient.** The parent gradient interval is the interval hull of the four child intervals $[g_{u,c} \pm \rho_{u,c}]$ — the smallest interval containing them, which is exactly the min-max range of the gradient. $g_{u,p}$ is its midpoint and $\rho_{u,p}$ its half-width; likewise for $v$.
- **Value.** With $g_p$ fixed, each child plane minus the parent slope, $h_{0,c} + g_c\cdot(\Delta - \delta_c) - g_p\cdot\Delta$, is affine over the child quadrant, so its largest and smallest values sit at quadrant corners. Evaluate the four corners per child, add $\pm r_c$, and take the overall maximum and minimum; $h_{0,p}$ is their midpoint and $r_p$ their half-width. This choice of $h_{0,p}$ minimizes $r_p$ for the chosen slope.

- **Height range.** $h_{\min,p} = \min_c h_{\min,c}$ and $h_{\max,p} = \max_c h_{\max,c}$. The range of a union is the union of the ranges, so this fold is **exact at every level**, not merely conservative.

Conservativeness is inherited by construction: the hull contains every child bound. Cost and shape are exactly a mipmap build. The behaviour is the right one: where children agree on a plane, only $r_c$ survives; where the displacement has genuine multi-scale structure, the disagreement terms grow the remainder and the node honestly degrades toward min-max behaviour.

**The three channel families fold by different laws, and that is what decides where each is tight.** Min-max (both height and gradient) folds by min and max and is exact at every level. The gradient hull is likewise exact — it is the min-max range of the gradient, written as midpoint and radius, and direct construction from the texel grid reproduces it to 3.6e-15. Only the **value** channel compounds: the parent is built from the child *models*, so slack already in a child becomes part of $r_p$ and can never be recovered. Measured at 1.0× at leaves rising to 2.4× at the root of a 64-cell tile and 4.3× at the root of a 256-cell one ([[Result — Pyramid channel study]]); how far it rises depends on both tile size and roughness. That is tolerable exactly because the slab's value is a fine-level property, where the compounding has barely started.

> [!tip] Direct construction — the second build mode
> $h$ is bilinear per texel, and $h$ minus a linear function is still bilinear per texel, so the residual $h - g_u\Delta u - g_v \Delta v$ attains its extremes over any cell at **texel grid nodes**. Enumerating the nodes of the closed cell (boundary included — $k+1$ per side, not $k$) therefore gives the tightest $r$ for a given slope *exactly*: no sampling, no Lipschitz padding, no risk to conservativeness. It removes the fold's compounding entirely, at a build cost of $N^2 \log N$ against $\tfrac43 N^2$, with every level independent and therefore parallel where the fold is sequential; measured, that is 1.6–2.1× the fold, not the 4.5× the operation count suggests, because the fold's twenty corner evaluations per parent are a large constant. It does **not** substitute for the min-max channel: the slope stays the gradient-hull midpoint, so the recovered interval still sits 6–11× wide at the root, and how much of the gap it removes is content-dependent. Implemented as the second build mode (`PyramidBuild::Direct`); the fold is the default.
>
> Only $h_0$ and $r$ differ between the two. The gradient hulls are exact either way (§4), and so is the height range, so the two constructions must agree on $g_u, g_v, \rho_u, \rho_v, h_{\min}, h_{\max}$ exactly — a checked invariant, not an expectation. Nesting also forces $|\Delta h_0| \le r_{\text{fold}} - r_{\text{direct}}$, which is why the descent sampler is nearly indifferent to which build it reads.
>
> Random sampling of $h$ is not an alternative. A finite sample can miss the extremum, so min and max over samples is an estimate, not an enclosure, and the certificate is gone. Padding by a Lipschitz term from the node's own $\rho$ restores validity, but at texel spacing the padding is zero and the method degenerates to the enumeration above.

---

## 5. What it recovers, and one semantics caveat

**Min-max recovery, and why the channel is stored instead.** The Taylor part alone recovers a height interval as $h \in h_0 \pm (|g_u| s_u + |g_v| s_v + r)$, so every consumer of the old two-channel pyramid runs unchanged. But it recovers it loosely: measured against the exact channel (Phase 1), 1.06× its width at texel cells, 1.6–1.8× at 4-texel cells, **5–29× at the root**. In world units that is a root shell about 2 mean edges thick on rock at 0.2×edge amplitude, against a true 0.18 — a bounding volume twice the size of the triangle it bounds, at the level where a traversal starts and where culling pays most. Hence $(h_{\min}, h_{\max})$ is stored (§2), which makes coarse-level traversal identical to TFDM and RMIP by construction rather than by measurement.

Decomposed ([[Result — Pyramid channel study]]), the recovery gap has two causes, and it is worth knowing which is which because only one is fixable by construction:

- **Not the representation.** On an exact plane the recovery is 1.000000 at every level, and the six Taylor numbers can encode a min-max node outright — slope 0, $r$ the true half-range. Nothing is inherently lost; the channel is stored to escape the fold's *choices*, not a limit of what the node can hold.
- **Mostly the recovery formula with a nonzero slope** (8.32× of the 10.93× at rock's root). It adds the plane's peak excursion and the residual's peak as if they coincided. When the plane is a real trend they effectively do; at coarse cells it is not one. The stored slope is the midrange of the gradient hull — 1.289 at the root against a true mean gradient of 0.008 — and the fold is right to store it that way, because $\rho_u, \rho_v$ must be the exact gradient range for the metric propagation of §6. Reading that slope as a height trend is what costs.
- **Partly the fold** (the remaining 1.6–2.4×), removable exactly by direct construction (§4) but not enough on its own.

The comparison against the *adjusted* recursion (different LoD semantics, see the warning below) is deferred to the LoD application.

> [!warning] LoD semantics differ from TFDM's adjusted min-max
> This pyramid bounds the **finest-level interpolant** over each region. TFDM's *adjusted* min-max additionally covers the surfaces obtained by **sampling $h$ at coarser mip levels**, which its fractional-LoD blending renders. Those are different surfaces. A Taylor pyramid can absorb the same adjustment — fold each level's own sampled model into the recursion — but until that is done, LoD-blended rendering must keep the adjusted channel (or conservatively widen $r$ by the per-level resampling bound). State which semantics a consumer needs; do not mix them silently.

---

## 6. Propagation, and the effect on queries

**To the metric.** Substitute the affine forms into the master formula of [[The induced metric of a displaced surface]]. Products of affine forms keep linear terms exact and push only the quadratic residue $\mathrm{rad}(x)\,\mathrm{rad}(y)$ into a fresh symbol (standard affine multiplication), so: $h^2$ stays anchored to $h_0$; $\nabla h \nabla h^\top$ has exact leading part $g\,g^\top$; the obliquity coupling uses the *same* $g$, so the §1 failure (two inconsistent worst-case values for one gradient) cannot occur. The result is an affine form per entry of $\mathbf{G}$, hence certified intervals for:

- **entries of $\mathbf{G}$** — directly;
- **$\det \mathbf{G}$ and $\sqrt{\det\mathbf{G}}$** (area density) — affine product of entries (shared symbols cancel across $\mathbf{G}_{11}\mathbf{G}_{22} - \mathbf{G}_{12}^2$), or interval-evaluate the closed forms (the quartic $\det\mathbf{Q}(h)$ over the $h$-interval plus the rank-one/Sylvester corrections);
- **stretch and anisotropy** — eigenvalues of $\mathbf{G}_0^{-1}\mathbf{G}$ from interval trace and determinant: $\lambda_\pm = \tfrac12\bigl(T \pm \sqrt{T^2 - 4D}\bigr)$ with $T, D$ interval-valued;
- **a normal cone** — the unnormalised normal $S_u \times S_v$ is an affine 3-vector; centre value gives the axis, accumulated radii give the half-angle (the tangent-frame practice of [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]], the target quantity of Munkberg et al. 2010).

All of this **composes per base triangle**: the pyramid stores only $(h, \nabla h)$ statistics; $\mathbf{G}_0, \mathbf{B}_0, \mathbf{C}_0, \mathbf{a}$ enter at query time from the triangle being asked about, TFDM-style.

Four propagation lessons from the Phase 1 implementation ([[Result — Phase 1 Taylor pyramid]]):

- **Intersect the affine route with plain interval evaluation of the same node.** Both are valid; their strengths are complementary. Affine keeps correlation across products (det); exact interval squares win once a deviation straddles zero, because $x^2 \ge 0$ is invisible to an affine form — and on rough content at coarse cells the gradient deviation always straddles zero. Inside the affine route, anchor each square at the midpoint of its quadratic term's range ($x^2$ at $x_0^2 + \mathrm{dev}^2/2$).
- **Eigenvalues need structure, not the interval discriminant.** The interval $\sqrt{T^2 - 4D}$ is orders of magnitude too wide. Implemented: Weyl perturbation of $\mathbf{G}_0^{-1/2}\mathbf{G}_c\,\mathbf{G}_0^{-1/2}$ ∩ the all-affine discriminant route with a secant-linearized affine sqrt ∩ refinement through $\lambda_-\lambda_+ = \det\mathbf{G}/\det\mathbf{G}_0$.
- **$\lambda_-$ has no meaningful per-node range certificate.** A slope-dominated metric is a rank-one-like update of $\mathbf{G}_0$, which pins the small eigenvalue of $\mathbf{G}_0^{-1}\mathbf{G}$ near 1; its true per-cell range is nearly zero and no first-order enclosure tracks that. The meaningful number is its certified width relative to its *value* (order 1 at traversed levels).
- **Cone validity guard.** The cosine bound $\cos\theta \ge (|c| - R)/(|c| + R)$ requires $R < |c|$; beyond that the cone must open to $\pi$. Use per-symbol vector norms for $R$ (the componentwise box is looser) and intersect with the perpendicular-component sine bound.

**Existing ray query — at parity, possibly improved.** Traversal reads the stored $(h_{\min}, h_{\max})$ and is therefore identical to TFDM and RMIP at every level, by construction. The node additionally yields a **slab bound**: the surface lies between two planes tilted by $g$, separated by $2r = O(s^2)$ instead of an axis-aligned extent of $O(s)$. Tilted-smooth regions — where min-max boxes are fattest — get thin slabs. Both bounds live in the same node, so a traversal can test min-max at coarse levels, the slab at fine levels, or intersect the two. **Traversal cost itself remains unmeasured**; Phase 1 measured bound widths only, and this note has said "measure it, claim nothing" since it was written. The measurement is now a Phase 2 item (master plan §7): node tests and time against the pre-tessellated mesh as ground truth, for min-max only, slab only, and both.

**New queries.** Closest-point lower bounds against slabs (tighter than against AABBs) for branch-and-bound proximity and certified walk-on-spheres steps; per-node area and product weights for hierarchical CDF sampling; certified adaptive quadrature for regional integrals; per-node anisotropy for LOD error. These are the consumers the [[Project — Conservative metric queries without tessellation|project note]] curates.

---

## 7. Cost

8 channels vs 2; same pyramid topology, same implicit traversal graph, same $\tfrac43$ mip overhead. Two of the eight *are* the min-max pyramid, so the increment over what a traversal already stores is six. Full-precision floats put this at 4× the min-max pyramid — same order as a single extra texture, and ~small against [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]]'s 108 bytes per base triangle of always-resident data. Quantisation (low-precision remainders, shared exponents) is production polish, deferred.

Two cost levers follow from the channels having different consumers and different fold laws, both measurable rather than assumed. **Precision by family:** bound channels need outward rounding under float32; anything stored as an estimate does not, because its error costs variance only. **Channels by level:** a channel need not exist at every level. The slab is useless at the root ($r$ is 6.7× the true half-range there), min-max is weakest at texel cells, and level 0 alone holds three quarters of all nodes — so storing a channel only where it pays recovers most of its cost. Unusual, legal, and the natural consequence of the fold-law split in §4.

**Tightness, measured (Phase 1, [[Result — Phase 1 Taylor pyramid]]):** on real assets at 0.05×edge amplitude, the median certified-to-true width ratio for $\sqrt{\det\mathbf{G}}$ is ~1.2 at texel cells, 1.6–1.9 at 4-texel cells, and 3.0–4.1 at 64-texel cells; obliquity plus large amplitude degrades it further (up to ~12 at the stress case). The bound is nearly exact at leaves; at coarse cells the remaining gap is inherent to the node, whose six Taylor numbers bound $h$ and the gradient by independent ranges and therefore also cover value combinations that never occur together on the surface patch. Independent min-max channels with interval propagation are 1.6–1.9× wider at texel scale, converging to parity by ~32-texel cells; the correlation advantage lives at fine levels. Certifying $\det\mathbf{G} > 0$ over large rough cells fails for every bound family tried — positivity certificates are a fine-level tool.

---

## 8. Classical vs new

**Classical — cite it.** Affine arithmetic (Comba & Stolfi; de Figueiredo & Stolfi) and its use for displacement bounds in [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]]. Taylor models / centred forms with remainder (Berz & Makino; validated-numerics literature). Interval eigenvalue enclosures for symmetric $2\times2$. Mipmapped displacement-gradient *moments* — LEADR (Dupuy et al. 2013) — the non-conservative ancestor; and mip pyramids over displacement for LoD signal-matching — [[Nießner & Loop 2013 — Analytic Displacement Mapping]].

**Believed new here.** The joint $(h, \nabla h)$ Taylor node on a displacement pyramid with the exact bilinear leaf, the conservative fold, and slab bounds, carried alongside the min-max channel so the existing ray query is at parity; its propagation to certified metric, area, anisotropy and normal-cone bounds per node; the argument that interval evaluation of independent gradient channels gives the same gradient two inconsistent worst-case values and so cannot serve the oblique metric (measured at 1.6–1.9× and twice the false alarms at fine levels; centred evaluation of stored channels closes the gap — §1).

**Check first.** "Slope intervals" and centred-form literature in validated numerics (the node may exist under another name); Heitz/Dupuy follow-ups to LEADR; subdivision-surface bounding work that stores tangent bounds.

---

Related: [[The induced metric of a displaced surface]] · [[Min-max mipmap and conservative bounds]] · [[Obliquity and the integrability defect]] · [[Project — Conservative metric queries without tessellation]] · [[Surface measure and sampling on implicit displaced surfaces]] · [[Nießner & Loop 2013 — Analytic Displacement Mapping]]
