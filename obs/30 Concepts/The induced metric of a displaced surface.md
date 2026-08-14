---
title: The induced metric of a displaced surface
tags: [concept, differential-geometry, derivation, metric, displacement, texture-space]
---

# The induced metric of a displaced surface

Every structure in this vault bounds $h$. Nobody bounds $\nabla h$. And bounding $\nabla h$ does not just give you the **area** of a displaced surface — it gives you its **metric**.

That difference is the whole point. Area is one number. A metric is a $2 \times 2$ matrix per point, and it carries the Laplace–Beltrami operator, hence geodesics, diffusion, spectra and anisotropy. You cannot assemble any of those from area estimates, no matter how many samples you take.

> [!note] Derivation, not a claim from any paper
> §1–§2 are classical surface theory — cite, don't claim. §3 onwards is derived here for the base-triangle-plus-tangent-space-displacement setting. §9 splits textbook from new.

> [!abstract] This is one of four notes
> **Here:** what the metric is, how it decomposes, what it costs.
> **[[Obliquity and the integrability defect]]:** the two things that go wrong when the displacement direction is an interpolated vertex normal — which is what every format in this vault actually uses.
> **[[Laplace–Beltrami on displaced surfaces]]:** turning the metric into an operator you can solve with.
> **[[Taylor-model bound pyramid]]:** the structure that bounds the metric conservatively — §7 here is the summary, that note is the spec.

> [!info] Notation
> $P(u,v)$ base surface · $N(u,v)$ unit displacement direction · $h(u,v)$ scalar displacement · $S = P + hN$ displaced surface. Subscripts are partials: $P_u = \partial P/\partial u$. $\nabla h = (h_u, h_v)^\top$ is the parameter-space gradient. $\mathbf{G}$ is the $2\times2$ metric (first fundamental form), $\mathbf{B}$ and $\mathbf{C}$ the second and third forms — classically $\mathrm{I}, \mathrm{II}, \mathrm{III}$, renamed so $\mathbf{I}$ can be the identity. Subscript $0$ means "of the base".
>
> $\operatorname{sym}(A) = \tfrac{1}{2}(A + A^\top)$ is the **symmetric part** of a matrix, $\operatorname{skew}(A) = \tfrac{1}{2}(A - A^\top)$ the antisymmetric part; every square matrix is uniquely their sum.

---

## 1. The three forms, briefly

For a surface $X(u,v)$ with unit normal $N$:

$$
\mathbf{G} = \begin{pmatrix} X_u \!\cdot\! X_u & X_u \!\cdot\! X_v \\ X_v \!\cdot\! X_u & X_v \!\cdot\! X_v \end{pmatrix}
\qquad
\mathbf{B}_{ij} = -\,X_i \!\cdot\! N_j
\qquad
\mathbf{C}_{ij} = N_i \!\cdot\! N_j
$$

$\mathbf{G}$ is the Gram matrix of the tangent frame — what an ant on the surface could measure. It gives the area element $dA = \sqrt{\det \mathbf{G}}\, du\, dv$ and the Laplace–Beltrami operator ([[Laplace–Beltrami on displaced surfaces|separate note]]).

$\mathbf{B}$ measures bending; the shape operator $\mathbf{W} = \mathbf{G}^{-1}\mathbf{B}$ has the principal curvatures $\kappa_1, \kappa_2$ as eigenvalues. $\mathbf{C}$ is the metric pulled back through the normal map, and for a real surface it is redundant:

$$
\mathbf{C} = \mathbf{B}\mathbf{G}^{-1}\mathbf{B}
\tag{$\star$}
$$

**Identity $(\star)$ is conditional**, and it fails for interpolated vertex normals — see [[Obliquity and the integrability defect]]. Everything in §2 rests on it, and §5 is where the failure is felt.

One fact used constantly, true whenever $N$ is a unit vector: $N \cdot N_u = N \cdot N_v = 0$.

---

## 2. Offset surfaces

Push a surface out by a **constant** distance $t$ along its normal, $X_t = X + tN$. Then $(X_t)_i = X_i + tN_i$, and

$$
\mathbf{G}_t = \mathbf{G} - 2t\,\mathbf{B} + t^2\,\mathbf{C} \;\overset{(\star)}{=}\; \mathbf{G}\,(\mathbf{I} - t\mathbf{W})^2
\qquad\Longrightarrow\qquad
\sqrt{\det \mathbf{G}_t} = \sqrt{\det \mathbf{G}}\;\bigl\lvert (1 - t\kappa_1)(1 - t\kappa_2) \bigr\rvert
$$

which collapses at $t = 1/\kappa_i$ — the **focal surface**, where the normal rays start crossing. That is the same place a [[Shell, prism and prismoid|prism]] stops being a usable coordinate system. §5 makes the identification, with caveats.

---

## 3. The derivation

Our surface is $S = P + hN$ with $h$ **varying**. Product rule:

$$
S_u = P_u + h_u N + h N_u
\qquad\qquad
S_v = P_v + h_v N + h N_v
$$

Form $\mathbf{G}_{ij} = S_i \cdot S_j$ and expand. Every cross term with $N \cdot N_i$ dies, and $N \cdot N = 1$:

$$
\mathbf{G}_{ij} = \underbrace{P_i \!\cdot\! P_j}_{\mathbf{G}_0}
\;+\; h \underbrace{(P_i \!\cdot\! N_j + P_j \!\cdot\! N_i)}_{-2\mathbf{B}_0}
\;+\; h^2 \underbrace{N_i \!\cdot\! N_j}_{\mathbf{C}_0}
\;+\; h_i \underbrace{(P_j \!\cdot\! N)}_{a_j} + h_j \underbrace{(P_i \!\cdot\! N)}_{a_i}
\;+\; h_i h_j
$$

Collect, with $\mathbf{a} = (P_u \!\cdot\! N,\; P_v \!\cdot\! N)^\top$ the **obliquity** vector:

> [!abstract] The metric of a displaced surface
> $$
> \mathbf{G} \;=\; \underbrace{\mathbf{G}_0 - 2h\,\mathbf{B}_0 + h^2\mathbf{C}_0}_{\textstyle \mathbf{Q}(h)\;=\;\text{offset metric at height } h} \;+\; \nabla h\, \nabla h^\top \;+\; \bigl( \nabla h\, \mathbf{a}^\top + \mathbf{a}\, \nabla h^\top \bigr)
> $$
> Needs only $\lVert N \rVert = 1$. Unlike $(\star)$, this holds whether or not $N$ is perpendicular to the base and whether or not $N$ is a legitimate normal field.

**The metric of a displaced surface is the metric of the offset surface at height $h$, plus a rank-one slope term, plus a coupling between slope and obliquity.**

Two things drive everything downstream:

**The coefficients are pure base geometry.** $\mathbf{G}_0, \mathbf{B}_0, \mathbf{C}_0, \mathbf{a}$ never involve $h$. For a flat base triangle with interpolated vertex normals they are constants or cheap closed forms — see §6.

**$h$ and $\nabla h$ enter through separate channels.** $h$ appears only inside $\mathbf{Q}(h)$, quadratically. $\nabla h$ appears only in the two correction terms. They never mix. That is why the determinant factors (§5) and why bounding stays cheap (§7).

Note $\mathbf{B}_0$ is **symmetrised** by construction — the derivation produces $P_i \!\cdot\! N_j + P_j \!\cdot\! N_i$, never the two terms separately. That is not a modelling choice; only a symmetric matrix can enter a Gram matrix. It is also where information is lost, which is the subject of the [[Obliquity and the integrability defect|companion note]].

---

## 4. Three regimes

| Base and normal field | $\mathbf{B}_0, \mathbf{C}_0$ | $\mathbf{a}$ | Metric |
|---|---|---|---|
| smooth base, true normal | ≠ 0 | 0 | $\mathbf{Q}(h) + \nabla h \nabla h^\top$ |
| flat triangle, face normal | 0 | 0 | $\mathbf{G}_0 + \nabla h \nabla h^\top$ |
| flat triangle, interpolated normals | ≠ 0 | **≠ 0** | full formula |

The **face-normal case** is the classical height field on a sheared domain:

$$
\det \mathbf{G} = \det \mathbf{G}_0 \bigl( 1 + \nabla h^\top \mathbf{G}_0^{-1} \nabla h \bigr)
$$

Note $h$ drops out completely — over a flat base with a fixed normal, **area depends on slope, not on height**. Good sanity check for an implementation.

The **interpolated-normal case** is what µ-meshes, TFDM and RMIP all use, and it is the awkward one: $\mathbf{B}_0$ and $\mathbf{C}_0$ are nonzero even though the triangle is flat — all the "curvature" comes from the shading normals, none from the geometry — and $\mathbf{a} \neq 0$ because an interpolated normal is not perpendicular to a flat triangle's edges. Two defects follow, they are nested rather than independent, and one of them breaks $(\star)$. That is [[Obliquity and the integrability defect]].

---

## 5. Area, and the relation to DJM

With $\mathbf{a} = 0$ the metric is a rank-one update, so the matrix determinant lemma applies directly:

$$
\det \mathbf{G} = \det \mathbf{Q}(h) \cdot \bigl( 1 + \nabla h^\top \mathbf{Q}(h)^{-1} \nabla h \bigr)
$$

and if $(\star)$ also holds, $\det\mathbf{Q}$ factors through curvature and the area element separates into three independent pieces:

$$
dA \;=\;
\underbrace{\sqrt{\det \mathbf{G}_0}}_{\text{base}}\;
\underbrace{\bigl\lvert (1 - h\kappa_1)(1 - h\kappa_2) \bigr\rvert}_{\text{curvature stretch}}\;
\underbrace{\sqrt{1 + \nabla h^\top \mathbf{Q}^{-1} \nabla h}}_{\text{slope stretch}}\;
du\,dv
$$

> [!warning] Not valid for interpolated vertex normals
> The middle factor needs $(\star)$, which fails when the integrability defect is nonzero — i.e. in the regime every format here uses. There, evaluate $\det\mathbf{Q}(h)$ directly as a quartic in $h$. And for $\mathbf{a} \neq 0$ the update is rank two, so use Sylvester's identity: with $U = [\,\nabla h \;\; \mathbf{a}\,]$ and $C = \left(\begin{smallmatrix}1&1\\1&0\end{smallmatrix}\right)$, $\det \mathbf{G} = \det\mathbf{Q}\,\det(\mathbf{I} + C\,U^\top \mathbf{Q}^{-1} U)$. Still closed form, still $2\times2$, just not pretty. See [[Obliquity and the integrability defect]].

**Three payoffs.**

*Only one factor needs the new pyramid.* Base area is constant; curvature stretch needs an interval on $h$ plus per-triangle curvature bounds; only slope stretch needs bounds on $\nabla h$.

*Metric degeneracy and shell invalidity coincide — when displacement is perpendicular.* With $\mathbf{a} = 0$ the slope factor is $\geq 1$, so $\det\mathbf{G} \to 0$ requires $\det\mathbf{Q}(h) \to 0$, which happens exactly at $h = 1/\kappa_i$: the focal surface, which is also where the [[Shell, prism and prismoid|prism]] stops being invertible and RMIP's Newton iteration loses its guarantee. A tensor fact and a data-structure fact are the same fact, and heuristic shell thickness becomes $h < 1/\kappa_{\max}$. **With obliquity the two separate** — see the companion note, which also shows they are governed by different determinants.

*It complements DJM — neither subsumes the other.* [[Zhang et al. 2026 — DJM|DJM]]'s $J$ is the $3\times3$ Jacobian of the **volumetric shell map**, carrying the height direction and prism shear that a surface metric does not; $\mathbf{G}$ is the $2\times2$ intrinsic metric of one displaced surface, carrying what $\det J$ discards. Where they overlap, the factorisation says *why* a determinant drifts — curvature or slope — and $\mathbf{G}_0^{-1}\mathbf{G}$ has an **eigenstructure**, a direction and a stretch ratio. Anisotropy governs geodesic routing, directional diffusion, and which direction loses detail first under level of detail. Any single determinant, DJM's included, throws that away.

---

## 6. How you actually compute it

Per base triangle, once:

```
e1 = q1 - q0;  e2 = q2 - q0            // edge vectors, constant
G0 = [[e1·e1, e1·e2], [e2·e1, e2·e2]]  // base metric, constant
Mu = m1 - m0;  Mv = m2 - m0            // vertex-normal differences, constant
```

Per sample at $(u,v)$:

```
M  = (1-u-v)*m0 + u*m1 + v*m2
N  = M / |M|
Nu = (Mu - N*(N·Mu)) / |M|             // projector (I - NNᵀ) applied to Mu
Nv = (Mv - N*(N·Mv)) / |M|

B0 = -sym([[e1·Nu, e1·Nv], [e2·Nu, e2·Nv]])   // sym() is mandatory, not tidiness
C0 =      [[Nu·Nu, Nu·Nv], [Nv·Nu, Nv·Nv]]    // computed directly — never from B0
a  = (e1·N, e2·N)

(h, hu, hv) = sampleDisplacement(u, v)   // three texture channels
gh = [hu, hv]

G  = G0 - 2*h*B0 + h*h*C0 + outer(gh,gh) + outer(gh,a) + outer(a,gh)
```

A normalise, six dot products for the forms, a couple of $2\times2$ assembles. **Cheaper than a ray–box test.** Whatever objections this agenda faces, cost is not one — the metric is nearly free once the displacement derivatives are in hand. There is real-time precedent for exactly these derivatives: [[Nießner & Loop 2013 — Analytic Displacement Mapping]] evaluated $S_u, S_v$ — Weingarten term included — in a 2013 pixel shader, for shading normals. The metric was one Gram matrix away; it was never assembled, and never bounded.

Storage: $\mathbf{G}_0$ (3 floats), $M_u, M_v$ (6 floats), plus curvature bounds for the conservative path. Same order as PDM's 108 bytes per base triangle.

Note the two comments in the code. Both are consequences of the companion note, and both are silent correctness traps if you assume $(\star)$.

---

## 7. Bounding it

To bound $\mathbf{G}$ over a texel region you need bounds on $h, h^2, h_u, h_v, h_u^2, h_v^2, h_u h_v$.

**A joint model, not two more channels — this is where the first draft of this note was wrong.** $h$ and $\nabla h$ are correlated — one is the derivative of the other — and the metric contains both, with $\nabla h$ appearing in two terms of opposite sign behaviour. Independent min-max channels for $h_u, h_v$ are intervals: symbol-free, uncorrelatable, and they let $\det\mathbf{G}$ bounds draw the same gradient twice at worst case — false degeneracy alarms exactly in the oblique regime. The structure that keeps the correlation is a **Taylor node**: a stored plane $(h_0, g_u, g_v)$ plus remainders, so that $h$'s affine form and $\nabla h$'s share the plane coefficients and TFDM's position symbols. Construction, fold, propagation and cost: [[Taylor-model bound pyramid]]. [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] already pushes affine forms through $S = P + hN$; this extends the same machinery through $\mathbf{G}$.

**µ-meshes don't need bounds — but for a different reason than this note first claimed.** The surface a µ-mesh renders is the **piecewise-flat** micro-triangle mesh: displaced micro-vertices joined linearly in 3D. Its metric is constant per micro-face and comes straight from the three vertex positions — a Gram matrix of two edges, no formula from §3 required ([[Laplace–Beltrami on displaced surfaces|Path A]]). Applying §3 with piecewise-linear $h$ describes a *different* surface — the smooth shell interpolant $P + hN$ — which agrees with the rendered one only in the refinement limit. Use vertex positions for µ-meshes; the formula and its bounds are for the tessellation-free family, where no vertices exist to fall back on.

**Second derivatives are a separate bill.** Curvature of the displaced surface, and Christoffel symbols, need $h_{uu}, h_{uv}, h_{vv}$ — a third pyramid. [[Laplace–Beltrami on displaced surfaces|The operator note]] explains why much of the agenda dodges this.

---

## 8. What the metric is good for

- **Area and sampling** — total, regional, and area-proportional importance sampling by hierarchical CDF. The original framing, now a corollary. See [[Surface measure and sampling on implicit displaced surfaces]] for the baseline it has to beat.
- **Geodesics, diffusion, spectra** — everything downstream of Laplace–Beltrami. See [[Laplace–Beltrami on displaced surfaces]].
- **LoD error, anisotropic and spectral** — the stretch tensor between two levels' metrics gives a certified, directional error whose anisotropy channel alone controls the Laplace–Beltrami spectrum; the same machinery against $\mathbf{G}_0$ measures what displacement *adds* to the base. Derived in [[Anisotropic and spectral LoD error]].
- **Shell validity and reach** — for perpendicular displacement, $h < 1/\kappa_{\max}$ is simultaneously metric non-degeneracy and prism invertibility. It is *related to, but not identical with*, the local feature size a [[Sugimoto et al. 2024 — Projected Walk on Spheres|Projected-Walk-on-Spheres]] tube needs: the reach of the **displaced** surface additionally depends on that surface's own curvature (second derivatives of $h$) and on self-proximity between distant sheets. The shell criterion is one ingredient of a certified tube radius, not the whole of it.
- **Base-mesh quality** — via [[Obliquity and the integrability defect]], two pre-bake diagnostics DJM has no counterpart for.
- **Curvature** — with a second-derivative pyramid, for curvature bakes and feature detection.
- **Physical quantities** — the Wenzel roughness ratio (true area / projected area) drives wetting; total area drives heat transfer, drag, coating, material cost. All functionals of $\sqrt{\det\mathbf{G}}$, now differentiable and localisable.
- **Contact** — tangent plane and area element for integrating over a contact patch, the missing piece for micro-to-macro friction homogenisation. See [[Proximity and contact queries against micro-geometry]].
- **Prefiltered coverage** — regional area is the primitive behind [[Prefiltered coverage from opacity hierarchies]].

---

## 9. Classical vs new

**Classical — cite it.** The three fundamental forms, the shape operator, the Weingarten relation $(\star)$. The offset metric $\mathbf{G} - 2t\mathbf{B} + t^2\mathbf{C}$, its factorisation $\mathbf{G}(\mathbf{I}-t\mathbf{W})^2$, the determinant $(1-t\kappa_1)^2(1-t\kappa_2)^2$, and the focal surface. Any surface theory text — and shell mechanics, where $\mathbf{Q}$ is the *shifter* and non-normal director fields are Cosserat territory. The height-field metric $\mathbf{G}_0 + \nabla h\nabla h^\top$ likewise. Analytic evaluation of displaced-surface derivatives, Weingarten term included, in real time: [[Nießner & Loop 2013 — Analytic Displacement Mapping]].

**Believed new here.**

1. The master formula of §3 as an **unconditional identity** — offset metric plus rank-one slope plus obliquity coupling — and the three-way factorisation of $dA$ in §5.
2. **Conservative metric bounds** via the joint [[Taylor-model bound pyramid]] — the pointwise formula is classical machinery; the certified per-region bounds are the claim.

Claims about the two defects live in [[Obliquity and the integrability defect]]; claims about the operator in [[Laplace–Beltrami on displaced surfaces]].

**Check first.** Geometry images (Gu, Gortler & Hoppe 2002) and descendants — closest existing "geometry as a texture" line. Offset and parallel surfaces in CAD, where item 1 is most likely to have a precedent.

---

Related: [[Obliquity and the integrability defect]] · [[Laplace–Beltrami on displaced surfaces]] · [[Taylor-model bound pyramid]] · [[Anisotropic and spectral LoD error]] · [[Min-max mipmap and conservative bounds]] · [[Shell, prism and prismoid]] · [[Surface measure and sampling on implicit displaced surfaces]] · [[Base mesh quality objectives]] · [[Micro-triangle and subdivision level]]
