---
title: Obliquity and the integrability defect
tags: [concept, differential-geometry, derivation, base-mesh, shading-normals, diagnostics]
---

# Obliquity and the integrability defect

Every displacement format in this vault displaces a **flat** base triangle along an **interpolated vertex-normal** field. That pairing is geometrically inconsistent — the normals are not the normals of the triangle they sit on — and the inconsistency has two distinct, computable measures.

Both come from vertex normals and edge vectors alone: a handful of dot products per base triangle, available **before anything is baked**. Neither appears in [[Base mesh quality objectives|the base-mesh objectives]], in Maggiordomo's four penalties, in Dou et al.'s regularisers, or in DJM.

> [!note] Derivation, not a claim from any paper
> The classical symmetry conditions are textbook. Their use as *diagnostics for this representation*, and the analysis of what breaks, is derived here. Companion to [[The induced metric of a displaced surface]], whose notation this note uses throughout.

> [!tip] The one-line summary
> **Obliquity $\mathbf{a}$** is the tilt of the displacement direction away from perpendicular. **The integrability defect $\delta$** is the failure of the normal field to be anybody's Gauss map. They are **nested**: $\mathbf{a} = 0 \Rightarrow \delta = 0$. Obliquity is the price of watertightness; $\delta$ is what breaks the classical curvature machinery.

---

## 1. Obliquity $\mathbf{a}$

$$
\mathbf{a} \;=\; \bigl( P_u \!\cdot\! N,\;\; P_v \!\cdot\! N \bigr)^\top
$$

Since $P_u, P_v$ span the base tangent plane, $\mathbf{a}$ records the **tangential part of $N$** — expressed in the dual basis rather than as a vector. If $N_{\text{tan}} = c^1 P_u + c^2 P_v$ then $\mathbf{a} = \mathbf{G}_0\,c$, so the coefficients are $c = \mathbf{G}_0^{-1}\mathbf{a}$.

Its invariant magnitude is clean. Because $\lVert N \rVert = 1$,

$$
\mathbf{a}^\top \mathbf{G}_0^{-1} \mathbf{a} \;=\; \lVert N_{\text{tan}} \rVert^2 \;=\; \sin^2\theta
$$

with $\theta$ the angle between the shading normal and the geometric face normal. So $\mathbf{a} = 0$ exactly when displacement is perpendicular to the base. Per-component magnitudes are basis-dependent; this is the statement to quote.

### It grows with base coarseness

On a flat triangle approximating a curved surface, vertex normals tilt from the face normal by roughly half the surface's turning across the triangle. Coarser base mesh → more turning per triangle → larger $\theta$.

**The defect is worst precisely where µ-meshes are most aggressive** — the 1000:1 amplification regime is the high-obliquity regime.

### It is the only term that can shrink the metric

In the master formula, obliquity contributes $\nabla h\,\mathbf{a}^\top + \mathbf{a}\,\nabla h^\top$. Along a direction $\xi$ that is

$$
\xi^\top \bigl( \nabla h\,\mathbf{a}^\top + \mathbf{a}\,\nabla h^\top \bigr) \xi \;=\; 2\,(\xi \!\cdot\! \nabla h)(\xi \!\cdot\! \mathbf{a})
$$

**sign-varying** — negative wherever slope and tilt oppose. The slope term $\nabla h \nabla h^\top$ is positive semi-definite and can only stretch. So obliquity is the only mechanism by which a displaced surface is locally *compressed* relative to what perpendicular displacement would give, and it therefore drives $\det\mathbf{G}$ toward zero **sooner** than the focal-surface prediction. The same fact in matrix form: the rank-two update matrix $\left(\begin{smallmatrix}1&1\\1&0\end{smallmatrix}\right)$ has determinant $-1$, i.e. it is indefinite, whereas a rank-one outer product is not.

### Obliquity is the price of watertightness

> [!important] A trade-off no paper here states — and its scope
> You can force $\mathbf{a} = 0$ by displacing along the **geometric face normal**. Then adjacent triangles displace in disagreeing directions and the surface cracks at every base edge. $\mathbf{a} = 0$ with cracks, or $\mathbf{a} \neq 0$ with continuity. **Within flat-base-triangle formats — every format in this vault — there is no third option.**
>
> The third option exists one level up: make the **base** smooth. [[Nießner & Loop 2013 — Analytic Displacement Mapping]] displaces the Catmull–Clark limit surface along its *true analytic normal*: $\mathbf{a} = 0$ everywhere **and** $C^1$-watertight. The price moves to base-surface evaluation cost — and it is the road µ-meshes, TFDM and RMIP all declined, buying flat-triangle simplicity with obliquity. The trilemma is a consequence of that design choice, not of displacement mapping itself.

That reframes [[Watertightness and cracks|watertightness]] as something bought rather than given, and it prices it: for flat bases the currency is metric distortion and the exchange rate is $\sin\theta$; for smooth bases the currency is evaluation cost.

[[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]]'s normal factor $1/(N_i \!\cdot\! N_g)$ is the nearest thing in this vault to a construction that manages obliquity, but it does **not** remove it — rescaling normal *lengths* leaves their directions tilted, so $\mathbf{a}$ is unchanged. What it buys is a shell whose cross-sections stay parallel, making the shear uniform with height instead of varying, which is what linearises its sample space. *Inferred from this vault's summary; check against the paper.*

---

## 2. The integrability defect $\delta$

$$
\delta \;=\; P_u \!\cdot\! N_v \;-\; P_v \!\cdot\! N_u
$$

**Where the symmetry it measures comes from.** For a genuine surface, differentiate $X_i \!\cdot\! N = 0$:

$$
X_{ij} \!\cdot\! N + X_i \!\cdot\! N_j = 0
\qquad\Longrightarrow\qquad
X_i \!\cdot\! N_j = -\,X_{ij} \!\cdot\! N
$$

and mixed partials commute, $X_{ij} = X_{ji}$, so $X_u \!\cdot\! N_v = X_v \!\cdot\! N_u$. **The symmetry is a consequence of $N$ being determined by $X$.** Author $N$ independently and nothing enforces it.

When $\delta \neq 0$, $N$ is not the Gauss map of *any* surface tangent to $P$: the shape operator is not self-adjoint, and there is no second fundamental form in the proper sense — only the symmetrised $\mathbf{B}_0$.

### Explicit form, and a sanity check

With $P_u = e_1$, $P_v = e_2$ constant and $N = M/\lVert M \rVert$:

$$
\delta \;=\; \frac{\bigl(e_1 \!\cdot\! M_v - e_2 \!\cdot\! M_u\bigr) \;-\; \bigl(a_1 (N \!\cdot\! M_v) - a_2 (N \!\cdot\! M_u)\bigr)}{\lVert M \rVert}
$$

where $M_u = m_1 - m_0$ and $M_v = m_2 - m_0$. The leading term $(q_1 - q_0)\!\cdot\!(m_2 - m_0) - (q_2 - q_0)\!\cdot\!(m_1 - m_0)$ is an antisymmetric pairing of edge vectors against normal differences: six input vectors in, one number out.

*Check:* put the vertices on a sphere of radius $R$ with radial vertex normals, so $m_i = q_i/R$ and hence $M_u = e_1/R$, $M_v = e_2/R$. The leading term becomes $(e_1 \!\cdot\! e_2 - e_2 \!\cdot\! e_1)/R = 0$, and the second becomes $[(e_1\!\cdot\!N)(N\!\cdot\!e_2) - (e_2\!\cdot\!N)(N\!\cdot\!e_1)]/R = 0$. So $\delta = 0$ **exactly** for spherical geometry with radial normals — as it should be, since the normal field is then a legitimate Gauss map, of the sphere rather than of the chord triangle.

### The two defects are nested

$$
\mathbf{a} = 0 \;\;\Longrightarrow\;\; \delta = 0
$$

If $N$ is perpendicular to the base everywhere then it *is* the Gauss map up to sign, and the symmetry returns. **Obliquity is the primary defect; integrability failure is secondary and can only occur in its presence.** Treating them as two independent knobs is a mistake — there is one knob, and $\delta$ is what the second-order consequences of turning it look like.

---

## 3. Why $\delta$ breaks $\mathbf{C}_0 = \mathbf{B}_0\mathbf{G}_0^{-1}\mathbf{B}_0$

The identity claims the shape operator generates everything about $dN$ — that $\mathbf{C}_0$, the Gram matrix of the normal derivatives, is recoverable from $\mathbf{B}_0$. It isn't, because **$\mathbf{B}_0$ is a lossy summary of $dN$ while $\mathbf{C}_0$ is the complete one.**

Decompose in the frame $\{P_u, P_v, N\}$ — a basis, but when $\mathbf{a} \neq 0$ not an orthogonal one:

$$
N_i = -W^k_{\;i} P_k + \mu_i N,
\qquad
N \!\cdot\! N_i = 0 \;\text{ forces }\; \mu_i = \textstyle\sum_k a_k W^k_{\;i}
$$

so the out-of-plane component $\mu$ exists exactly when $\mathbf{a} \neq 0$. Substituting,

$$
\mathbf{C}_0 = W^\top \mathbf{G}_0 W \;-\; \mu\mu^\top ,
\qquad\qquad
\widetilde{\mathbf{B}}_{0,ij} = -P_i \!\cdot\! N_j = (\mathbf{G}_0 W)_{ij} - a_i \mu_j
$$

and the metric can only ever use $\mathbf{B}_0 = \operatorname{sym}(\widetilde{\mathbf{B}}_0)$, since a Gram matrix is symmetric. **Two things are discarded:**

| Discarded by $\mathbf{B}_0$ | Retained by $\mathbf{C}_0$ | Nonzero when |
|---|---|---|
| the antisymmetric part, $= \delta$ | — (it is quadratic, so it feeds back) | $\delta \neq 0$ |
| the out-of-plane component $\mu$ | as $-\mu\mu^\top$ | $\mathbf{a} \neq 0$ |

with $\delta = -\widetilde{\mathbf{B}}_{0,12} + \widetilde{\mathbf{B}}_{0,21}$.

**The identity would be reconstructing four numbers from three.** It cannot be done, and $\delta \neq 0$ is the signal that the discarded part is nonzero.

### What this costs, and what it doesn't

**Nothing at evaluation time.** Compute $\mathbf{C}_0 = [\,N_i \!\cdot\! N_j\,]$ directly — three dot products, which is exactly what the pseudocode in [[The induced metric of a displaced surface|the metric note]] already does. The implementation was never relying on the identity.

**What is lost is the theoretical shortcut**, and it propagates:

$$
\mathbf{C}_0 = \mathbf{B}_0\mathbf{G}_0^{-1}\mathbf{B}_0
\;\Longrightarrow\;
\mathbf{Q}(h) = \mathbf{G}_0(\mathbf{I} - h\mathbf{W})^2
\;\Longrightarrow\;
\det\mathbf{Q} = \det\mathbf{G}_0\,(1-h\kappa_1)^2(1-h\kappa_2)^2
$$

Every link fails. So the curvature-stretch factor in $dA$ is unavailable, **and so is $h < 1/\kappa_{\max}$ as the shell-validity criterion.** Evaluate $\det\mathbf{Q}(h)$ directly as a quartic in $h$ and take its roots.

### Three determinants, not one

Worth separating carefully, because they coincide only in the perpendicular case:

| Quantity | Condition it governs | Formula |
|---|---|---|
| $\det\mathbf{G}$ | the displaced surface is an immersion | master formula, $h$ varying |
| $\det\mathbf{Q}(t)$ | the constant-height offset surface is an immersion | quartic in $t$ |
| $\det[\,\Phi_u\;\Phi_v\;N\,]$ | the **prism** is an invertible coordinate system | $\sqrt{\det\mathbf{Q}(t)}\,\cos\psi$ |

with $\Phi(u,v,t) = P + tN$ the shell map and $\psi$ the angle between $N$ and the constant-$t$ offset surface's own normal.

Perpendicular displacement sets $\psi = 0$ and makes the slope factor $\geq 1$, so all three degenerate together at $h = 1/\kappa_i$. Obliquity separates them in two ways at once: $\cos\psi$ can vanish independently, and — because the obliquity coupling is indefinite (§1) — $\det\mathbf{G}$ can vanish while $\det\mathbf{Q}$ does not. **Evaluate all three, or state which one you mean.**

---

## 4. As base-mesh diagnostics

Both quantities cost a few dot products per base triangle and need no displacement data, so they can be reported by an authoring tool at decimation time:

```
// per base triangle, from vertices q0,q1,q2 and unit vertex normals m0,m1,m2
e1 = q1 - q0;  e2 = q2 - q0
Mu = m1 - m0;  Mv = m2 - m0
G0 = [[e1·e1, e1·e2], [e2·e1, e2·e2]]

// evaluate at a few sample points, or at the barycentre for a scalar summary
M  = (1-u-v)*m0 + u*m1 + v*m2;   N = M / |M|
Nu = (Mu - N*(N·Mu)) / |M|
Nv = (Mv - N*(N·Mv)) / |M|

a         = (e1·N, e2·N)
sin2theta = a ᵀ * inverse(G0) * a          // obliquity, = sin²θ  ∈ [0,1]
delta     = e1·Nv - e2·Nu                  // integrability defect
```

`sin2theta` is dimensionless and directly interpretable — a value of $0.25$ means the displacement direction is tilted $30°$ off perpendicular. `delta` carries units and should be normalised against $\sqrt{\det\mathbf{G}_0}$ before being compared across triangles of different sizes.

**What to do with them.** Both are candidate terms in a decimation cost, alongside Maggiordomo's visibility value $V(v) = \max_d \min_{n \in N} (d \!\cdot\! n)$ — which is a related but distinct quantity, testing *admissibility* (do displacement directions vanish inside a face?) rather than *distortion*. A base mesh can pass the visibility test comfortably and still carry large obliquity.

This is the concrete sense in which [[Base mesh quality objectives|the base-mesh objectives]] are less settled than [[MOC — Open Questions|the open-questions map]] currently claims: DJM replaced two heuristics with an exact scalar, but the scalar is a determinant, and neither it nor its predecessors measure normal-field coherence at all.

---

## 5. Classical vs new

**Classical.** The self-adjointness of the shape operator, its equivalence to the symmetry $X_u \!\cdot\! N_v = X_v \!\cdot\! N_u$, and the Weingarten relation. The decomposition of a vector in a non-orthogonal frame. All textbook.

**Believed new here.**

1. $\mathbf{a}$ and $\delta$ as **named, computable, pre-bake diagnostics for this representation**, with $\mathbf{a}^\top\mathbf{G}_0^{-1}\mathbf{a} = \sin^2\theta$ as the invariant magnitude and the nesting $\mathbf{a} = 0 \Rightarrow \delta = 0$.
2. **Obliquity as the price of watertightness in flat-base formats** — face normals give $\mathbf{a} = 0$ and cracks, interpolated normals give continuity and $\mathbf{a} \neq 0$; smooth bases escape both at evaluation cost ([[Nießner & Loop 2013 — Analytic Displacement Mapping]]) — and obliquity as the only term that can shrink the metric.
3. The **lossy-summary explanation** of why $(\star)$ fails, with the two discarded components identified separately.
4. The **separation of the three determinants**, and the resulting correction to $h < 1/\kappa_{\max}$ as a shell criterion.
5. $\delta$ as a quantitative form of the shading-normal artefact [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] observed qualitatively.

**Check first.** PDM itself, for claim 2 and for the reading of its normal factor above. Offset and parallel surface theory in CAD, where oblique offsets are studied under other names. Anything on non-integrable frame fields in geometry processing — the same defect appears there as a curl or holonomy term, and the vocabulary may already exist.

---

Related: [[The induced metric of a displaced surface]] · [[Laplace–Beltrami on displaced surfaces]] · [[Base mesh quality objectives]] · [[Shell, prism and prismoid]] · [[Watertightness and cracks]] · [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]]
