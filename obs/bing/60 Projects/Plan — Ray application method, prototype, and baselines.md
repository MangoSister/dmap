---
title: Plan — Ray application method, prototype, and baselines
tags: [plan, ray-tracing, displacement, triangle-shell, taylor-model, certified-bounds, prototype, baselines]
status: prototype
created: 2026-08-16
updated: 2026-08-23
parent: "[[Project — Conservative first-order queries on displacement maps]]"
implementation-status: python-reference-and-figures
---

# Plan — Ray application method, prototype, and baselines

> [!abstract] Current decision
> The proposed ray application intersects the **analytic triangle-shell image of a fixed texture-space microtriangulation**. The initial Python reference now implements the exhaustive cubic-leaf oracle, an independent 80-digit root cross-check, rational ray, event partition, certified chord tubes, first-order hierarchy, and min/max/Taylor recursive traversals. It has passed the initial procedural gates, but CUDA remains blocked until broader fuzz/asset coverage and a representative tightness study establish that the method is both safe and useful.

## 1. Goals and non-goals

### 1.1 Goal

Given a world ray and a triangle proxy carrying a scalar displacement texture, find the closest intersection with the represented displaced surface while:

- storing no displacement-resolution geometry or BLAS;
- avoiding nonlinear ray–node tests at most hierarchy nodes;
- conservatively replacing the nonlinear shell-space ray by a small set of line segments with certified tubes;
- using a shared first-order displacement hierarchy for rejection;
- retaining an exact nonlinear leaf test for the defined surface; and
- falling back conservatively in singular or poorly conditioned cases.

### 1.2 Non-goals for the first implementation

- Conversion from an arbitrary dense or smooth source surface into the triangle-shell representation.
- A universally fastest method across all shell thicknesses, views, maps, and hardware.
- Immediate implementation of tube-aware GPU DDA before the reference traversal is correct.
- Proxy-mesh construction or optimization.
- Fractional displacement LoD or filtering semantics. The finest-level surface is frozen first.

## 2. Freeze the represented surface before coding

The two applications must query the same mathematical surface. The recommended contract is the surface used by Ogaki-style nonlinear microtriangle intersection, not Mode 1's current planarized world microtriangles.

### 2.1 Triangle shell

For canonical base barycentrics $(a,b)$,

$$
P(a,b)=p_A+aE_1+bE_2,
\qquad
N(a,b)=n_A+aM_1+bM_2,
$$

where

$$
E_1=p_B-p_A,\quad E_2=p_C-p_A,\quad
M_1=n_B-n_A,\quad M_2=n_C-n_A.
$$

The shell map is

$$
F(a,b,h)=P(a,b)+hN(a,b).
$$

The directions $n_i$ may already include the displacement scale, as in the current NRTDSM implementation. Texture coordinates are an affine map of barycentrics:

$$
\begin{pmatrix}u\\v\end{pmatrix}
=t_A+a(t_B-t_A)+b(t_C-t_A).
$$

The initial representation uses an invertible, unwrapped per-face texture parameterization with a fixed valid domain and no repeat boundary inside a face. The current arbitrary-atlas/repeat path is deferred. A later tiled implementation must keep the curve in unwrapped coordinates and instance hierarchy tiles, or split explicitly at every integer wrap event; clamping an out-of-range segment is incorrect.

The shell must also remain regular over the represented domain. Its Jacobian is

$$
J_F=\left[P_a+hN_a,\;P_b+hN_b,\;N\right].
$$

The converter/preprocess must conservatively establish that $\det J_F$ stays away from zero over the triangle and height range, or mark the primitive for refinement/fallback. This shell-regularity condition is distinct from the ray-dependent denominator $D(h)$ below.

### 2.2 Texture microtriangles

Each texture cell is split along one fixed diagonal into two triangles. Within texture microtriangle $\tau$, height is affine:

$$
H_\tau(u,v)=c_\tau+g_{\tau,u}u+g_{\tau,v}v.
$$

The represented world surface is

$$
S_\tau(u,v)=F\bigl(a(u,v),b(u,v),H_\tau(u,v)\bigr).
$$

Although $H_\tau$ is planar in $(u,v,h)$, $S_\tau$ is generally curved in world space because both $H_\tau$ and $N$ vary. This is precisely why the exact leaf intersection is cubic.

This contract has three advantages:

1. It matches the existing Mode 0 `testNonlinearRayVsMicroTriangle` formulation.
2. The metric formula for $S=P+hN$ is exact inside each microtriangle; $\nabla h$ is constant there.
3. Texture-microtriangle edges have measure zero, so gradient discontinuities are compatible with area integration and sampling.

### 2.3 What changes relative to current Mode 1

Current Mode 1 maps the four texel corners to world space and intersects two planar world triangles. That surface is not generally identical to $S=P+hN$. It remains useful as a fast approximate ablation, but it must not serve as the correctness oracle for the unified paper.

> [!important] Decision required before implementation
> Approve the analytic shell-microtriangle surface above as the common ray/metric contract. If planar world microtriangles are retained instead, the metric and area application must be reformulated for that piecewise-planar surface, and much of the induced-metric motivation changes.

## 3. Exact nonlinear shell ray

Let the world ray be

$$
R(t)=O+td,
\qquad t\in[t_{\min},t_{\max}].
$$

Choose an orthonormal basis $(e_0,e_1)$ perpendicular to $d$. For a fixed shell height $h$, projecting $R(t)=F(a,b,h)$ onto $d^\perp$ removes $t$ and gives

$$
M(h)
\begin{pmatrix}a\\b\end{pmatrix}
=c(h),
$$

with

$$
M(h)=
\begin{pmatrix}
\langle E_1+hM_1,e_0\rangle & \langle E_2+hM_2,e_0\rangle\\
\langle E_1+hM_1,e_1\rangle & \langle E_2+hM_2,e_1\rangle
\end{pmatrix},
$$

$$
c(h)=
\begin{pmatrix}
\langle O-p_A-hn_A,e_0\rangle\\
\langle O-p_A-hn_A,e_1\rangle
\end{pmatrix}.
$$

Every entry is affine in $h$. By Cramer's rule,

$$
a(h)=\frac{A(h)}{D(h)},
\qquad
b(h)=\frac{B(h)}{D(h)},
$$

where $A$, $B$, and $D=\det M$ are quadratic polynomials. This is already implemented by `computeCanonicalSpaceRayCoeffs` and by the Python `RayLines` scaffold.

### 3.1 Texture-space form

Because texture coordinates are affine in $(a,b)$,

$$
u(h)=\frac{U(h)}{D(h)},
\qquad
v(h)=\frac{V(h)}{D(h)},
$$

where

$$
U=t_{A,u}D+(t_{B,u}-t_{A,u})A+(t_{C,u}-t_{A,u})B,
$$

and likewise for $V$. Both numerators are quadratic.

The shell-space ray is therefore

$$
q(h)=\left(\frac{U(h)}{D(h)},\frac{V(h)}{D(h)},h\right).
$$

Height is the curve parameter and is represented exactly by a chord; only $u$ and $v$ require approximation.

### 3.2 World-ray parameter

Let $L=\langle d,d\rangle$. Substituting $a=A/D$ and $b=B/D$ into $F$ gives

$$
t(h)=\frac{T(h)}{L D(h)},
$$

where

$$
\begin{aligned}
T(h)={}&\bigl(\langle p_A-O,d\rangle+h\langle n_A,d\rangle\bigr)D(h)\\
&+\bigl(\langle E_1,d\rangle+h\langle M_1,d\rangle\bigr)A(h)\\
&+\bigl(\langle E_2,d\rangle+h\langle M_2,d\rangle\bigr)B(h).
\end{aligned}
$$

$T$ is cubic. It is used to clip valid shell intervals to the OptiX ray range and to recover exact hit distance. The prototype must not assume that endpoint $t$ values bound the interior unless monotonicity has been established.

## 4. Conservative valid-interval construction

Start from the triangle's conservative displacement range $[h_-,h_+]$. Form the event set from all real roots in that range of:

1. $D(h)=0$ — singular constant-height inversion;
2. $A(h)=0$ — $a=0$ boundary;
3. $B(h)=0$ — $b=0$ boundary;
4. $D(h)-A(h)-B(h)=0$ — $a+b=1$ boundary;
5. $T(h)-t_{\min}LD(h)=0$;
6. $T(h)-t_{\max}LD(h)=0$;
7. $U'D-UD'=0$ — texture-$u$ turning points; and
8. $V'D-VD'=0$ — texture-$v$ turning points.

Include $h_-$ and $h_+$, sort, and isolate coincident roots robustly. Between adjacent events, every sign used for domain classification is constant, $D$ is nonzero, and $u,v$ are monotone. Classify an open interval from a certified midpoint enclosure:

$$
a\ge0,\qquad b\ge0,\qquad a+b\le1,
\qquad t\in[t_{\min},t_{\max}].
$$

Event points themselves must also be tested or assigned to closed adjacent intervals. Otherwise a ray that only touches a triangle or ray-range boundary could be missed.

The hardware BVH and six-corner AABB only decide that this triangle shell is a candidate. The valid curve range is derived from the complete conservative $[h_-,h_+]$ plus the polynomial ray-range events above. Do not infer the full interior $h$ range only from the heights at a prism's entry and exit faces.

### 4.1 Singular and ill-conditioned intervals

A denominator root is not handled by inserting it as a segment endpoint and then skipping failed endpoint evaluation. Initial policy:

- if $D$ has a root in a potentially valid shell interval;
- or if the certified lower bound on $|D|$ is below a conditioning threshold;
- or if event-root isolation cannot certify the signs;

then fall back for that primitive/ray to the exact existing nonlinear traversal. Correctness takes priority; the paper reports fallback frequency and cost.

## 5. Certified piecewise-linear curve tubes

Consider one valid event interval $I=[h_0,h_1]$ on which $D$ has constant sign. For a scalar rational coordinate

$$
x(h)=\frac{N(h)}{D(h)},
$$

with quadratic $N,D$,

$$
x''(h)=\frac{Q_x(h)}{D(h)^3},
$$

where

$$
Q_x=igl(N''D-ND''\bigr)D-2\bigl(N'D-ND'\bigr)D'.
$$

$Q_x$ is at most cubic after cancellation. Let

$$
m_D=\min_{h\in I}|D(h)|>0
$$

and let $M_x$ conservatively bound $|Q_x|$ over $I$. Then

$$
\sup_I |x''|\le \frac{M_x}{m_D^3}.
$$

For the linear interpolant $\ell_x(h)$ through the interval endpoints, the standard interpolation remainder gives

$$
|x(h)-\ell_x(h)|
\le
\epsilon_x
=\frac{(h_1-h_0)^2}{8}\frac{M_x}{m_D^3}.
$$

Apply this separately to $u$ and $v$. The exact curve is contained in the axis-aligned tube

$$
|u-\ell_u(h)|\le\epsilon_u,
\qquad
|v-\ell_v(h)|\le\epsilon_v.
$$

### 5.1 Computing the polynomial bounds

- Since $D$ is quadratic and sign-stable, $m_D$ is obtained from the endpoints and the stationary point $D'=0$ if it lies in $I$, with outward rounding.
- Transform $Q_u,Q_v$ from the power basis on $I$ to the Bernstein basis on $[0,1]$. The polynomial range lies in the convex hull of its Bernstein coefficients, so

$$
M_x\le\max_i |\beta_i|
$$

is conservative.
- The Python reference uses high precision and/or outward-rounded interval arithmetic. The GPU port uses a fixed-degree, outward-widened implementation.

The current sampled finite-difference estimate in `scripts/visualize_rays.py` is useful for plots but is not a certificate.

### 5.2 Adaptive segmentation

Recursively bisect $I$ until

$$
\max(N_u\epsilon_u,N_v\epsilon_v)\le\eta,
$$

where $N_u,N_v$ are leaf grid resolutions and $\eta$ is a performance parameter measured in texels. Correctness does not depend on $\eta$ because traversal covers the entire tube; smaller $\eta$ trades more segments for fewer visited cells.

There is no silent segment cap. If a GPU resource limit is reached, the remaining interval falls back to exact nonlinear traversal. The prototype records the segment-count distribution and the fallback rate.

## 6. Exact first-order node for the chosen leaf surface

The existing Taylor-node note gives an exact bilinear leaf. The chosen surface instead has two affine height triangles per cell. Its exact conservative leaf is still simple.

Let the two microtriangles have constant gradients $g^{(0)},g^{(1)}$. Choose a stored gradient, initially

$$
g=\tfrac12\left(g^{(0)}+g^{(1)}\right).
$$

For cell center $x_c$ and each of the four cell corners $x_j$ with height $h_j$, define

$$
q_j=h_j-g^\mathsf{T}(x_j-x_c).
$$

Set

$$
h_0=\tfrac12\left(\min_j q_j+\max_j q_j\right),
\qquad
r=\tfrac12\left(\max_j q_j-\min_j q_j\right),
$$

and

$$
\rho_u=\max_{k\in\{0,1\}}|g_u^{(k)}-g_u|,
\qquad
\rho_v=\max_{k\in\{0,1\}}|g_v^{(k)}-g_v|.
$$

On each microtriangle, the residual from the stored plane is affine, so its extrema occur at that triangle's vertices. The four-corner construction therefore proves

$$
|h-(h_0+g^\mathsf{T}(x-x_c))|\le r
$$

over the complete cell, and the gradient bounds are exact for the chosen $g$. For parents, use the quadrant-aware fold from [[Taylor-model bound pyramid]]: fix $g_p$, evaluate each child model relative to the parent-gradient plane at that child's four corners, include $\pm r_c$, and take the midpoint/radius of the global residual range. This is conservative because the relative model is affine inside each child and is tighter than applying the slope mismatch over the full parent box.

Possible later optimization: choose $g$ to minimize a weighted objective in $(r,\rho_u,\rho_v)$ instead of simply averaging the two gradients. Do not optimize this before measuring the simple construction.

## 7. Conservative segment–node rejection

Let a segment be parameterized by $s\in[0,1]$:

$$
\ell(s)=\bigl(\ell_u(s),\ell_v(s),\ell_h(s)\bigr),
$$

where $\ell_h$ is exact and the true curve satisfies the tube bounds $(\epsilon_u,\epsilon_v)$.

For a hierarchy node with UV rectangle

$$
\Omega=[u_-,u_+]\times[v_-,v_+],
$$

first compute the parameter interval $J\subseteq[0,1]$ for which the chord lies in the inflated rectangle

$$
[u_- -\epsilon_u,u_+ +\epsilon_u]
\times
[v_- -\epsilon_v,v_+ +\epsilon_v].
$$

Because $\ell_u$ and $\ell_v$ are affine, $J$ is found by two one-dimensional slab intersections. If $J$ is empty, the true curve cannot enter the node.

The node height model is

$$
\widehat H(u,v)=h_n+g_u(u-u_c)+g_v(v-v_c),
$$

with remainder $r_n$. Along the chord define

$$
f(s)=\ell_h(s)-\widehat H(\ell_u(s),\ell_v(s)).
$$

$f$ is affine. At a true surface intersection inside the node,

$$
|f(s)|
\le
w_n
=r_n+|g_u|\epsilon_u+|g_v|\epsilon_v.
$$

Therefore the node is safely rejected when

$$
f(J)\cap[-w_n,w_n]=\varnothing.
$$

The range $f(J)$ is obtained from its two endpoint values. This replaces the current heuristic `hBow` inflation with a derived bound.

### 7.1 What first order contributes to ray tracing

The first-order hierarchy does **not** make the nonlinear shell ray straighter, reduce the degree of the exact leaf equation, or replace the certified tube. It improves a different part of the algorithm: surface rejection during hierarchy traversal.

A scalar min/max node asks only whether the ray-height range overlaps the complete height range stored over a UV region. That zero-order interval includes predictable height variation caused by local slope. Over a smooth region of linear size $s$, its width is generally

$$
O(s\lVert\nabla h\rVert).
$$

The first-order node subtracts the stored plane before bounding uncertainty. Its residual is zero for an exact ramp and, on smooth data, is expected to scale as

$$
r=O(s^2\lVert\nabla^2h\rVert).
$$

For example, let

$$
h(u,v)=0.7u,
\qquad
h_{\mathrm{ray}}(u)=0.7u+0.1.
$$

The two scalar height ranges may overlap over a coarse node even though the ray is everywhere above the surface. The first-order node stores $g=(0.7,0)$ and $r=0$, so after subtracting the common slope, $f(s)=0.1$ and the complete node is rejected immediately.

The two main components are therefore orthogonal and must be ablated separately:

- **piecewise shell-ray tubes** control the error from replacing the nonlinear ray by chords;
- **first-order Taylor slabs** remove predictable surface slope from the node uncertainty;
- the tube widens the slab by $|g_u|\epsilon_u+|g_v|\epsilon_v$ so the combined test remains conservative; and
- the exact cubic leaf solve preserves the declared surface once traversal reaches a candidate microtriangle.

Expected ray benefits are fewer descended nodes, fewer candidate leaves and cubic solves, and larger high-level skips in smooth tilted regions. The costs are a four-component ray node $(h_0,g_u,g_v,r)$ rather than a two-component min/max node, additional arithmetic, and tube inflation proportional to $|g|$. Constant-height regions already have tight min/max bounds; checkerboard/noise maps can make $r$ large; grazing or poorly conditioned rays can make the tube wide. The first-order ray claim survives only if reduced traversal and leaf work outweigh the larger fetch and arithmetic cost on the GPU.

### 7.2 Lazy node-local linearization and the direct directional certificate

The initial prototype globally subdivides every valid rational-ray interval to a fixed UV-tube tolerance before traversal. This is the cleanest correctness reference, but it makes segmentation look like an independent preprocessing step. The stronger coupled formulation constructs chords lazily during hierarchy traversal:

1. intersect the incoming certified UV tube with the current node rectangle and obtain a conservative height interval $I_n$;
2. build a chord and certified UV tube only over $I_n$;
3. apply the node height test;
4. if height rejection is blocked only by curve bow, bisect $I_n$ and test the two shorter chords at the same node; otherwise reject or descend; and
5. repeat after child clipping, so hierarchy descent determines where new knots are useful.

This still requires the componentwise UV tube for node ownership. First order does not replace that spatial certificate. It adds a separate, node-dependent scalar certificate for the height test. Write the texture-space rational ray as

$$
x(h)=\begin{pmatrix}u(h)\\v(h)\end{pmatrix},
$$

and let $\ell_x(h)$ be its chord on $I_n$. For the node plane

$$
\widehat H_n(x)=h_n+g_n^\mathsf{T}(x-c_n),
$$

define the chord residual

$$
f_{n,\ell}(h)=h-\widehat H_n(\ell_x(h)).
$$

At any true hit inside the node,

$$
|f_{n,\ell}(h)|
\le
r_n+E_n(I_n),
\qquad
E_n(I_n)=\sup_{h\in I_n}
\left|g_n^\mathsf{T}\bigl(x(h)-\ell_x(h)\bigr)\right|.
$$

The componentwise tube gives the safe bound

$$
E_{n,\mathrm{cw}}=|g_{n,u}|\epsilon_u+|g_{n,v}|\epsilon_v,
$$

but it discards cancellation between the two coordinates. Because projection is linear,

$$
z_n(h)=g_{n,u}u(h)+g_{n,v}v(h)
=
\frac{g_{n,u}U(h)+g_{n,v}V(h)}{D(h)}
$$

is another scalar rational function whose chord is exactly $g_n^\mathsf{T}\ell_x(h)$. Therefore the same second-derivative/Bernstein construction used for $u$ and $v$ directly certifies

$$
E_{n,\mathrm{dir}}
\le
\frac{|I_n|^2}{8}
\frac{M_{z_n}}{m_D^3}.
$$

This direct directional bound is first-order-specific because its direction is the node gradient $g_n$. It can admit longer node-local chords or reject a parent interval without refinement when $u$ and $v$ bow errors cancel after projection. The contribution is not that first order makes the rational curve geometrically straighter; it makes only the error component relevant to the tilted surface model smaller.

The acceptance logic must remain dimensionally explicit:

- the UV tube decides which hierarchy rectangles the true curve may enter;
- $r_n+E_n$ is a height-space width used only for surface rejection; and
- the rule “raw arc deviation is below slab thickness” is invalid because it compares different units and can miss neighboring UV cells.

This coupling does not imply a universal advantage over min/max. A min/max node has $g=0$ and pays no projected bow term, while first order pays a larger node representation and directional-bound arithmetic. The intended ablation is therefore: global/minmax, global/first-order, lazy/first-order with componentwise projection, and lazy/first-order with direct projection, all with identical exact leaves and oracle checks.

## 8. Traversal strategy

### 8.1 Reference traversal: recursive quadtree

The Python prototype should use recursive or explicit-stack quadtree traversal, not DDA. For each node it performs:

1. tube-versus-UV-rectangle interval clipping;
2. triangle-domain rejection if useful;
3. the Taylor-slab test;
4. child descent or exact leaf tests.

This is the shortest route to validating the mathematics and measuring candidate-node reduction. It is not presented as the final performance algorithm.

### 8.2 GPU target: tube-supercover hierarchical DDA

After the reference passes, adapt the current hierarchical DDA so it enumerates every cell whose rectangle intersects the chord tube. The intended strategy is:

- segment until the tube radius is below a chosen fraction of a leaf cell;
- traverse the center chord at each active level;
- enumerate the small neighbor stencil induced by $(\epsilon_u,\epsilon_v)$;
- retain a neighbor only if the chord intersects that cell's inflated rectangle;
- establish a deterministic ownership rule so a cell emitted from adjacent centerline steps is processed once; and
- use the Taylor-slab test over the exact chord-parameter interval for that cell.

If the ownership proof or duplicate suppression becomes expensive, a stack-based tube/quadtree traversal is the first correct GPU fallback. DDA is a performance choice, not part of the correctness foundation.

### 8.3 Ordering and early termination

The prototype tests all candidate leaves and chooses the minimum exact world-ray $t$. It does not infer closest-hit order from segment endpoints. The GPU may sort segments heuristically, but it may skip work behind a known hit only with a conservative lower bound on $t(h)$ over the remaining interval. Any-hit rays may terminate after any exact occluding hit.

## 9. Exact leaf intersection

A texture microtriangle lies in a plane

$$
\alpha u+\beta v+\gamma h+\kappa=0.
$$

Substituting the rational ray yields the cubic numerator

$$
G_\tau(h)
=\alpha U(h)+\beta V(h)+\gamma hD(h)+\kappa D(h).
$$

The exact leaf procedure is:

1. isolate every real root of $G_\tau$ in the segment's valid $h$ interval;
2. evaluate $(u,v)$ and verify membership in the texture microtriangle;
3. recover $t(h)$ and verify the ray interval;
4. compute $S_u,S_v$ and the geometric normal from the analytic surface; and
5. keep the smallest valid $t$.

The existing `testNonlinearRayVsMicroTriangle` implements this surface equation and is the first CUDA leaf path to reuse. A later optimization may use the chord-plane intersection as a root predictor followed by safeguarded Newton, while retaining the cubic solver as a certified fallback.

## 10. Conservative outer shell bound

For a triangle-wide height interval $[h_-,h_+]$, each coordinate of $F(a,b,h)$ is affine in $(a,b)$ for fixed $h$, so its extrema over the barycentric triangle occur at a vertex. At a fixed vertex it is affine in $h$, so its extrema occur at $h_-$ or $h_+$. Therefore the AABB of the six positions

$$
p_i+h_-n_i,\qquad p_i+h_+n_i,\qquad i\in\{A,B,C\},
$$

is conservative for the complete triangle shell. This part of the current preprocessing is reusable once the height interval itself is conservative over the triangle's texture footprint.

## 11. Correctness invariant

For every non-fallback ray/primitive pair:

1. Event partitioning covers every part of the exact shell curve inside the proxy triangle and ray interval.
2. Every accepted chord tube contains its corresponding exact rational curve interval.
3. Inflated UV clipping visits every hierarchy node that the exact curve could enter.
4. The widened Taylor-slab test cannot reject a node containing an intersection.
5. Exact leaf root isolation finds every surface intersection in every visited leaf.
6. Comparing exact world $t$ values returns the closest hit.

Ill-conditioned cases use the exact nonlinear fallback. Consequently, approximation affects work, not hit correctness.

## 12. Algorithm sketch

```text
IntersectTriangleShell(ray, triangle, hierarchy):
    build A, B, D, U, V, T
    events <- roots of shell-domain, ray-range, singularity, and UV-turning polynomials

    best <- no hit
    for each closed candidate h interval induced by events:
        if interval validity or denominator conditioning is uncertified:
            return ExactNonlinearFallback(ray, triangle)

        segments <- recursively construct chord tubes using second-derivative bounds
        if segment resource budget is exceeded:
            return ExactNonlinearFallback(ray, triangle)

        for segment in segments:
            TraverseNode(root, segment, best)
    return best

TraverseNode(node, segment, best):
    J <- chord parameter range inside node UV rectangle inflated by tube radius
    if J is empty: return

    w <- node.r + abs(node.gu)*segment.eps_u + abs(node.gv)*segment.eps_v
    f_range <- range of chord height minus node plane over J
    if f_range does not overlap [-w, w]: return

    if node is internal:
        traverse children, using only conservative ordering for pruning
    else:
        test both analytic shell microtriangles with exact cubic root isolation
        update best by exact world-ray t
```

## 13. Prototype decision: Python first

Use one pure Python reference program initially. Its purpose is to falsify the formulation quickly, not predict final GPU milliseconds.

### 13.1 Why Python

- Existing `scripts/visualize_rays.py` already contains the rational-ray coefficient construction, polynomial utilities, event generation, UV mapping, and plots.
- Exact/exhaustive leaf enumeration is straightforward for small procedural maps.
- High-precision and outward-rounded checks are much easier to inspect than CUDA behavior.
- Fuzz cases can preserve the full failing input and generate a diagnostic plot.
- A failed bound costs days rather than weeks of GPU debugging.

### 13.2 Planned reference layout

Start as one script, tentatively `scripts/prototype_first_order_ray.py`, with these logical sections:

1. polynomial and interval utilities;
2. triangle-shell and rational-ray construction;
3. piecewise-affine height microtriangles;
4. Taylor leaf and parent fold;
5. event isolation and validity classification;
6. certified second-derivative tube construction;
7. recursive Taylor-slab traversal;
8. exhaustive exact leaf oracle;
9. randomized/adversarial tests; and
10. plots and CSV statistics.

Split it into modules only if the single file becomes difficult to test.

### 13.3 Prototype inputs

Begin with $8^2$–$64^2$ NumPy height grids:

- constant and pure ramps;
- sinusoid plus Gaussian bumps from `scripts/make_test_disp.py`;
- checkerboards and impulses;
- deterministic random smooth fields; and
- high-frequency random fields.

Use procedural shells matching `quad`, `twisted_quad`, and selected triangles from `curved_surface` and `sphere`. Real 1K–4K assets are unnecessary until the GPU port.

### 13.4 Independent oracle

For every test ray, exhaustively enumerate every texture microtriangle intersecting the proxy triangle, isolate its cubic roots at high precision, verify membership, and choose the closest exact $t$. This oracle shares the mathematical surface but not the hierarchy, segmentation, DDA, or pruning logic.

The current dense planarized triangle oracle is retained only for the planar-microtriangle ablation; it is not independent truth for the analytic shell surface.

### 13.5 Adversarial ray families

- Rays exactly on or within ulps of texture grid lines and corners.
- Tangent and nearly tangent leaf intersections.
- Front-facing through grazing incidence.
- Ray origins inside the outer shell.
- UV turning points inside the height range.
- Denominator roots just outside and inside the range.
- Large normal variation and nearly singular shell Jacobians.
- Multiple intersections within one leaf and across multiple leaves.
- Hits exactly on the proxy-triangle or texture-microtriangle boundary.

### 13.6 Reference outputs

- Miss/false-hit count against the exhaustive oracle.
- Candidate-leaf superset failures, independently of final roots.
- Maximum sampled curve deviation divided by the certified tube radius.
- Segment count distribution.
- Denominator-conditioning and fallback distribution.
- Node/cell/leaf counts for min/max versus Taylor slabs.
- Bound width by hierarchy level.
- Diagnostic SVG/PNG for every minimized failure.

### 13.7 Prototype gates

The Python reference must pass before CUDA changes:

1. No candidate-set omission in deterministic and randomized tests.
2. No hit mismatch against the exhaustive analytic oracle.
3. Every densely sampled curve deviation lies inside the certified tube, with the proof implemented independently of sampling.
4. All injected singular/ill-conditioned cases take the fallback.
5. Smooth/ramp maps show materially tighter Taylor rejection than recovered min/max.
6. Segment and candidate-leaf distributions suggest a plausible GPU regime.

Estimated scope: roughly four to six focused working days for a useful go/no-go reference, provided the surface contract is frozen first.

### 13.8 Initial implementation checkpoint — 2026-08-16

`scripts/prototype_first_order_ray.py` now contains the single-file CPU reference described above. Current implemented checks include:

- exact cubic leaf equations over exhaustive and hierarchy-selected microtriangles;
- roots of proxy-domain, ray-range, denominator, and texture-turning events;
- adaptive rational-curve chord tubes from Bernstein-bounded second derivatives;
- explicit floating-point widening for Bernstein conversion, chord evaluation, and event ownership;
- exact two-microtriangle leaf nodes and the tighter quadrant-aware parent fold;
- recursive min/max and Taylor-slab traversals over the same curve tubes;
- candidate-leaf superset checks independently of final closest-hit comparison;
- exact grid-line, grid-corner, microtriangle-diagonal, proxy-edge, on-surface, inside-shell, grazing, randomized, and singular-denominator cases;
- constructed UV-turning, isolated proxy tangency, tangent-leaf, two-root leaf, and denominator-root-near-range checks;
- an independent 80-digit `Decimal` derivative-partition root solver, used only to cross-check exhaustive cubic leaves; and
- JSON records containing a complete failing ray and segment list when a mismatch occurs.

The current default gate covers 384 ray/case pairs on ramp, smooth, and checker maps with flat and twisted shells. It reports zero candidate omissions, zero hit mismatches, sampled tube ratios at or below one, and one deliberately singular fallback per case. The default also cross-checks 6,144 exhaustive leaf polynomials through the independent 80-digit solver with zero root-set mismatch. It includes multi-hit rays: the latest run found 46 unique intersections on 40 hitting smooth/flat rays and 89 on 40 hitting checker/flat rays. A stricter 0.01-texel run exercises adaptive subdivision (up to 2.44 mean segments per active noisy/twisted ray in the measured case) without a mismatch. A seven-map corpus adds constant, smoothed noise, impulse, and unsmoothed noise; its latest 448-ray post-fix run also passed, including 7,168 independently checked leaf polynomials. A larger 2,048-ray stress run passed before the final tighter parent-fold change; rerun it after every subsequent certificate or fold modification.

Initial work-count evidence is encouraging but not yet a GPU go decision. On the current 64-ray default, Taylor slabs reduce ramp node visits by roughly 17–18% and smooth-map leaf visits by roughly 20–22% relative to min/max. Checkerboards tie, as expected. Coarse smooth/noise parent remainders can still be wider than scalar min/max, and Taylor node visits can be slightly worse on some noisy cases. These are CPU traversal counts, not timings, and do not yet price the larger node fetch.

Remaining Python gates before CUDA:

1. expand the 80-digit cross-check from selected rays to randomized saved seeds and every constructed adversarial case;
2. run per-level tightness/work sweeps over selected existing procedural shells and real displacement crops;
3. persist CSV/JSON summaries and automatically minimize/plot any failure;
4. audit the event-root enclosure policy with explicit interval objects rather than the reference solver's fixed ownership pad; and
5. decide H1/H3 from distributions rather than the initial aggregate means.

### 13.9 TFDM-box relationship ablation — 2026-08-16

The reference now includes a three-way mechanism test motivated by TFDM's conservative box estimation:

1. **Independent-height box:** the exact coordinate-wise AABB of $P(u,v)+hN(u,v)$ when $h$ may take any value in the node's scalar min/max interval independently of $(u,v)$. This gives the TFDM-style zero-order height relaxation its tightest AABB for the chosen world frame; it is stronger than a deliberately loose interval implementation.
2. **First-order correlated box:** a conservative AABB retaining $h=h_0+g^\mathsf{T}(x-x_c)+e$, $|e|\le r$. The resulting quadratic coordinate polynomials are enclosed with tensor-product Bernstein coefficients.
3. **Direct tube–slab traversal:** the proposed shell-space test, which does not collapse the tilted model into an axis-aligned world box.

Both box traversals use the same hierarchy, proxy-triangle footprint rejection, world rays, and oracle leaf ownership. Dense surface samples validate both box families at every hierarchy level. This remains a mechanism ablation rather than a complete TFDM reproduction: TFDM also chooses a local tangent frame, computes affine boxes on demand, implements its own D-BVH traversal and LoD semantics, and must still be run as the external baseline.

The post-footprint test covers 640 rays over ramp, smooth, smoothed-noise, checker, and noise maps on flat and twisted shells. It reports zero box-enclosure violations and zero candidate-leaf omissions. On the twisted shell, mean first-order-to-independent AABB volume ratios were:

| Map | Leaf level | Root level |
|---|---:|---:|
| Ramp | 0.70 | 0.67 |
| Smooth | 1.03 | 1.50 |
| Smoothed noise | 0.95 | 1.41 |
| Checker | 1.00 | 1.00 |
| Noise | 0.95 | 2.94 |

Thus preserving height–UV correlation can materially shrink a box for a coherent ramp, but conservative parent folding plus Bernstein wrapping can make coarse first-order AABBs worse on curved/noisy content. Ray work shows the same limited box benefit: on the twisted ramp, first-order versus independent boxes visit 20.12 versus 20.75 nodes/ray and 1.23 versus 1.34 leaves/ray. On several smooth/noisy cases the first-order box visits more internal nodes.

The direct slab is more promising because it keeps the bound aligned with the surface instead of axis-aligning it. In the same twisted-ramp run, direct Taylor versus shell-space min/max traversal visits 12.35 versus 14.63 nodes/ray. On smooth/twisted data, node counts are close but Taylor reaches 1.00 versus 1.25 leaves/ray. These counts are not a comparison against the complete TFDM implementation; they isolate why the proposed bound shape may help.

**Current interpretation:** TFDM's affine-box philosophy is direct prior art, and the first-order height form can be viewed as adding shared height–UV symbols to its zero-order min/max height form. However, “first order makes TFDM boxes tighter” is not a general contribution or performance claim. The practical ray hypothesis should remain the direct certified tube–slab test. Continue toward the bandwidth-aware GPU slab ablation only after the remaining Python/literature gates; do not prioritize the first-order AABB variant unless a substantially tighter inexpensive box construction is found.

### 13.10 Reproducible notebook and initial paper figures — 2026-08-17

The correctness harness remains a command-line program; the notebook is a thin explanatory and figure-generation layer. This separation avoids duplicating mathematical predicates or letting hidden notebook state become part of the certification story.

Files:

| File | Role |
|---|---|
| `scripts/prototype_first_order_ray.py` | Authoritative CPU correctness/tightness harness and exhaustive oracle. |
| `scripts/first_order_ray_core.py` | Stable analysis API: deterministic demo construction, trace records, sampled surfaces/nodes, and lightweight same-surface sweeps. It delegates all predicates and exact leaves to the reference harness. |
| `scripts/first_order_ray_figures.py` | Color-blind-safe paper style and deterministic PDF/SVG/PNG exporters. |
| `notebooks/first_order_ray_visualization.ipynb` | Executed narrative notebook containing the method, first-order node, lazy first-order-coupled linearization, TFDM-box relationship, and work-count sections. |
| `environment-figures.yml` | Conda environment. NumPy is pinned to 2.3.4 with OpenBLAS because the initially resolved NumPy/MKL 2026 combination hung on a 3-by-3 linear solve in this Windows environment. |
| `scripts/run_first_order_notebook.ps1` | Project-local Jupyter/IPython cache setup and one-command headless execution or JupyterLab launch. |

Create and run the environment from the repository root:

```powershell
conda env create --prefix .conda-figures-clean --file environment-figures.yml
powershell -ExecutionPolicy Bypass -File scripts\run_first_order_notebook.ps1
```

Use `-Lab` on the second command for interactive iteration. The current clean headless run executes ten code cells in order in about 32 seconds and writes each figure as PDF, SVG, and PNG under `figures/first_order_ray/`.

The initial figure set is:

1. **Method overview:** three explicitly separated views of the same ray: (a) the straight world ray, proxy triangle, displaced surface, and mapped shell volume/prismoid; (b) the canonical triangular prism, rational shell-space ray $q(h)=F^{-1}(R(t))$, analytic events, and certified piecewise chords; and (c) the affine texture-domain curve, chord tubes, candidate leaves, and exact cubic hit. The old residual/slab panel was removed from this overview and remains in the dedicated first-order-node figure.
2. **First-order node:** the sampled piecewise-affine height surface, local plane, residual field, gradient direction, and certified remainder.
3. **Lazy segmentation:** node-local chords, componentwise versus direct gradient-projected bow certificates, and a four-way operation-count ablation with the evidence boundary shown in the figure.
4. **TFDM-box relationship:** a coherent ramp where height–UV correlation shrinks the AABB, paired with leaf/root volume ratios showing that AABB collapse becomes worse on coarse smooth/noisy nodes.
5. **Regime sweep:** same-surface CPU node and exact-leaf counts for ramp, smooth, smoothed noise, checker, and noise maps.

The notebook's 24-ray-per-map sweep reports zero candidate omissions and hit mismatches. The unchanged default command-line gate was rerun after introducing the analysis layer: 384 ray/case pairs and 6,144 independent high-precision leaf-polynomial checks again report `FAILURES=0`.

**Evidence boundary:** these are explanatory figures and CPU mechanism counts. They are candidates for a method/ablation figure, not evidence of GPU speed or superiority to complete TFDM, RMIP, PDM, triangles, or DMM. Publication plots must later consume persisted experiment data rather than hand-entered values.

### 13.11 Method-overview clarification — 2026-08-17

The canonical domain $\Delta\times[h_{\min},h_{\max}]$ is a triangular prism in $(a,b,h)$. Its image under $F(a,b,h)=P(a,b)+hN(a,b)$ is an ordinary world-space prism only for parallel displacement directions; interpolated directions generally produce a warped ruled shell volume (a prismoid). The displaced height graph lies inside this volume. A straight world ray maps through $F^{-1}$ to the rational curve $q(h)=(a(h),b(h),h)$, which is the nonlinear ray processed by Ogaki-style traversal.

The original mild-twist demo produced a genuinely almost-straight shell-space locus, so the rational curve and its chords overlapped visually. The revised explanatory overview uses a declared $6\times$ direction-variation case with a sampled positive Jacobian determinant range, solely to make this correspondence and curvature legible. It is not benchmark evidence and is kept separate from the ordinary smooth/twisted case used by the first-order-node and work-count figures.

For visual separation, the one global endpoint secant is a strong purple dotted line with diamond endpoints, while the local piecewise chords are strong vermilion long-dashed lines. Panel (c) now annotates the traversal sequence directly: affine map into texture coordinates, certified tube construction, then overlap-driven hierarchy descent and exact testing at admitted leaves. The global secant remains a visual guide and is not an algorithmic traversal primitive.

Panel (c) was subsequently changed to a magnified local leaf view because the full $[0,1]^2$ texture made the $16\times16$ cells, tubes, and nearly coincident chords too small. Certified tubes now use cyan; first-order surviving leaves use navy cross-hatching; extra min/max-only survivors use yellow hatching; and local chords are drawn as a thick vermilion underlay beneath the thinner green rational curve. “Surviving” explicitly means that both the UV tube test and node height-bound test could not reject the node. It does not imply an intersection: only the exact cubic test in a surviving leaf can produce the black root marker.

The final panel-(c) layout retains both scales side by side: a full $[0,1]^2$ texture view with the proxy boundary rendered as a faint dashed outline and the crop marked in purple, followed by the magnified traversal view without the proxy outline. This preserves global context while keeping tubes, piecewise chords, surviving leaves, and the exact root legible.

The overall method overview uses two rows: world space and shell space are panels (a) and (b) on the first row, while panel (c) spans the complete second row. This prevents the context-plus-detail texture views from being compressed into a narrow third column.

### 13.12 Lazy first-order-coupled linearization checkpoint — 2026-08-17

The CPU reference now implements the formulation in §7.2 without replacing the existing global-segmentation path:

- `directional_chord_error` constructs the rational numerator $g_uU+g_vV$ and applies the same denominator lower bound, second-derivative numerator, Bernstein range, and explicit floating-point padding as the componentwise tube certificate;
- `lazy_candidate_hits` clips intervals and constructs chord tubes on demand, keeps componentwise UV ownership separate from the height test, and records UV-driven versus bow-driven splits;
- the exact cubic microtriangle solver remains the only operation allowed to report a hit;
- structural tests densely sample synthetic rational curves and verify both the componentwise UV certificate and the direct projected certificate; and
- `first_order_ray_core.py` exposes a deterministic four-way case and a small multi-map correctness/work sweep for the notebook.

The fixed explanatory case now uses a declared anisotropic triangle shell and a smoothed-noise $16\times16$ map. It is selected for legibility, not as benchmark evidence: the rational shell-space ray has a perpendicular curve–chord bow of $0.00436$ ($6.6\%$ of its parent-chord length). The sampled shell Jacobian stays positive over the represented height range, $[0.136,0.309]$, and the ray has one exhaustive-oracle hit. All four variants recover that hit.

At the selected hierarchy node, the direct projected certificate is $3.46\times10^{-4}$ versus $3.13\times10^{-3}$ for componentwise projection, a $9.03\times$ reduction. Define the rejection margin as $\min_h|f_{n,\ell}(h)|-(r_n+E)$; positive certifies rejection. The componentwise parent has margin $-2.15\times10^{-3}$ and must split. The same parent with the directional certificate has margin $+0.63\times10^{-3}$ and rejects immediately. After splitting, the first componentwise child reaches $+0.48\times10^{-3}$ and rejects. End-to-end reference work for the ray is:

| Variant | Node tests | Curve linearizations | Bow splits | Leaves | Hits |
|---|---:|---:|---:|---:|---:|
| Global + min/max | 34 | 2 | 0 | 3 | 1 |
| Global + first order | 34 | 2 | 0 | 2 | 1 |
| Lazy first order, componentwise projection | 32 | 8 | 1 | 2 | 1 |
| Lazy first order, direct projection | 30 | 5 | 0 | 2 | 1 |

The notebook also runs 24 deterministic rays on each of five maps. Componentwise/direct node counts per active ray are 17.96/17.96 (ramp), 21.57/21.57 (smooth), 19.91/19.83 (smoothed noise), 25.39/25.39 (checker), and 26.65/26.65 (noise). Linearization counts tie except on smoothed noise, where they are 5.35/5.17; all exact-oracle hit mismatches are zero. This confirms that cancellation exists but is sparse in the initial small corpus.

An expanded 64-ray-per-map run (320 rays total) also reports zero hit mismatch for both lazy variants. Each map contains one deliberately singular ray and both variants take the same fallback. The only work difference again appears on smoothed noise: 14.413/14.381 nodes and 3.984/3.921 linearizations per active ray for componentwise/direct projection. This reinforces the current conclusion that the directional idea is correct and measurable but not yet broadly consequential.

This is mechanism evidence, not a speed claim. The directional variant removes one knot, three local linearizations, and two node tests on the selected ray, but node tests and linearizations do not have equal cost. A publishable claim still requires distributions over rays/scenes plus GPU time, register pressure, occupancy, and memory traffic.

The previous three-panel `lazy_segmentation` composite has been split into three independent figures so that method geometry, decision logic, and evidence no longer compete for space:

- `ray_through_prism` follows one ray across its complete valid proxy-prism interval. Panel (a) shows the straight world ray crossing the ruled triangle shell, panel (b) shows the complete nonlinear $q(h)$ inside the straight shell-space prism, and panel (c) enlarges the full UV projection. The ray is continuously valid for $h\in[0.2,0.8]$ and has one exact oracle hit. Lazy traversal requests three distinct internal split locations—two from UV-tube refinement and one from the bow certificate—which form four colored chords in the explanatory reconstruction.
- `lazy_certificate` isolates the signed certificate margin and the $9.03\times$ directional-error reduction that changes the parent from split to reject.
- `lazy_work_counts` isolates the four-way CPU mechanism counts on the certificate example ray and explicitly keeps them separate from the method diagram.

The full-prism chord sequence is reconstructed from the union of split locations requested at different hierarchy nodes. It is a visualization of the lazy traversal, not a proposed runtime buffer: the GPU keeps only temporary node-local chord state and stores no per-ray global polyline. Each figure is exported independently as PDF, SVG, and PNG under `figures/first_order_ray/`. The notebook renders neither the old composite nor the two-short-ray replacement.

### 13.13 Step 1 experiment specification — 2026-08-23

The next action is now frozen as [[Plan — Step 1 ray reference experiment]]. It specifies the dataset manifest, procedural and real displacement crops, triangle-shell regimes, 256-ray family, saved-seed and adversarial suites, persisted per-node/per-ray schema, required plots, and explicit G0–G4 go/no-go rules before further prototype changes.

Step 1 remains a CPU recursive-quadtree experiment with exact analytic leaves. It contains no DDA, CUDA work, or external-baseline timing. Its two main paired comparisons are:

1. global certified segmentation with min/max versus first-order slab rejection, which isolates the hierarchy; and
2. lazy first-order traversal with componentwise versus direct directional projection, which isolates whether the node slope reduces ray-side splitting.

Passing correctness, hierarchy-benefit, and certification gates authorizes only the later fixed-segmentation GPU slab ablation. The directional lazy variant has a separate gate and can be dropped without invalidating a fixed-segmentation first-order result.

### 13.14 Step 1 persistence checkpoints — 2026-08-23

Persistence P1/P2 are complete. The schema and aggregate adapter pass 26 tests, and run `p2-40a9c6e5e680` saved 14 procedural map/shell summaries covering 896 rays with zero recorded correctness failures. Pooled aggregate-only ratios were 0.9709 for `G-FO/G-MM` node tests and 0.8610 for accepted leaf-interval tests. These values are not paper results because they lack per-ray distributions, lazy variants, real maps, and frozen family stratification.

The detailed P3 observer and record contract is frozen in [[Plan — Step 1 persistence and instrumentation#16. P3 detailed observer and record plan — 2026-08-23]]. It defines immutable post-predicate events, exact counter semantics, static-node measurements, per-ray incidence/ownership/fallback fields, lazy-method execution, equivalence tests, and the gate that must pass before broader corpus work.

### 13.15 Detailed P3 result — 2026-08-23

P3 passes: 32 combined tests, the unchanged 384-ray/6,144-high-precision-polynomial legacy gate, and detailed run `p3-f29f4158c87f` (896 rays, all four methods) all report zero correctness failures. Global methods reproduce P2 exactly. Certified global segmentation is p50/p95=1, p99/max=2 segments per active ray. Fixed `G-FO/G-MM` ratios remain 0.9709 nodes and 0.8610 exact leaf-interval tests.

The directional lazy result is adverse and must remain visible. `L-DIR` evaluates 5,300 directional certificates but saves only four local linearizations and two node tests relative to `L-CW`; exact leaves are identical. Only one active ray improves. This does not formally decide G4 because the legacy generator lacks the frozen incidence families and real maps, but it invalidates any current wording that directional certification broadly reduces knots. The fixed first-order slab and shared-hierarchy story do not depend on that optional feature.

## 14. Reusable code and what must change

| Existing component | Reuse | Required change |
|---|---|---|
| `scripts/visualize_rays.py` | `RayLines`, polynomial helpers, UV transform, plotting, current curvature experiments | Replace sampled finite-difference “bounds” and `numpy.roots` correctness assumptions with certified interval/Bernstein logic; use actual $u,v$ events. |
| `nrtdsm_intersection_kernels.h::computeCanonicalSpaceRayCoeffs` | Exact $A,B,D$ coefficients | Preserve; add conditioning and outward-rounding policy. |
| `computeTextureSpaceRayCoeffs` | Exact $U,V$ coefficients | Preserve; use $U,V$ derivative events rather than only $a,b$. |
| `testNonlinearRayVsMicroTriangle` | Correct analytic-shell leaf equation and cubic solver | Restrict to certified candidate intervals; audit numerical root coverage and hit attributes. |
| Mode 0 nonlinear traversal | Correctness fallback and same-surface baseline | Validate against the new independent analytic oracle; do not assume it is authoritative. |
| Mode 1 segment/DDA path | GPU scaffolding, statistics, texture fetches, per-segment setup | Replace midpoint subdivision, zero-width DDA, `hBow`, fixed caps, ordering assumptions, and planar world-triangle leaf. |
| `nrtdsm_preprocess_kernels.cu` | Min/max construction, shell AABB setup, height texture access | Add the first-order node build and a packed ray view; audit per-triangle height coverage. |
| `scripts/verify_vs_oracle.py` | AOV comparison and reporting pattern | Point it to the new analytic oracle/reference dataset; retain the old oracle as an approximation comparison. |
| `baseline_compare.py`, `profile_system.py`, `measure_segments.py` | Performance harness and counters | Add Taylor/minmax ablations, fallback counts, tube widths, and corrected warmup/repetition protocol. |
| `docs/testsets.md` and procedural meshes | Regime matrix and assets | Add explicit singular, grazing, boundary, and correlated smooth-ramp tests. |
| `tfdm/` | Existing TFDM baseline implementation | Normalize surface/filtering semantics and any-hit behavior before timing. |

## 15. CUDA port order after the Python gate

1. Add the exact piecewise-affine Taylor pyramid on CPU/GPU preprocessing and validate dumps against Python.
2. Add Taylor-slab node tests to an existing exact traversal without changing segmentation; isolate the hierarchy benefit.
3. Port certified event intervals and tube bounds, retaining stack/quadtree traversal first.
4. Enable lazy node-local chord construction and the direct gradient-projected bow certificate; keep global/componentwise switches for the four-way ablation.
5. Replace traversal with tube-supercover hierarchical DDA only after candidate sets match the reference.
6. Reuse the exact cubic leaf solver.
7. Add packed formats and occupancy work last.

At every step, preserve a runtime switch for the preceding method. This creates the ablation table automatically.

### 15.1 GPU state ownership and cost-control rule

“Node-local ray linearization” must not be implemented as a persistent ray stored by every prism or as a complete polynomial reconstruction at every visited hierarchy node. The intended ownership is:

- each proxy-triangle prism permanently stores only its shell-map coefficients and displacement hierarchy;
- each ray–prism intersection invocation constructs one temporary rational-curve certificate;
- traversal reuses that certificate across all visited nodes; and
- a node may request a temporary child certificate only when the existing UV tube is too wide for efficient ownership or when curve bow is the sole reason an otherwise rejectable height interval survives.

The temporary state should contain the active $h$ interval, chord endpoints, componentwise UV radii, a denominator lower bound, and the fixed-degree Bernstein data needed for projected bow bounds. It belongs in registers or a deliberately small traversal stack and is discarded when that ray–prism query ends. Do not allocate one ray record per prism, and do not use a large per-thread array of prebuilt segments.

The directional second-derivative numerator is linear in the numerator of the rational coordinate:

$$
Q_{g_uU+g_vV}=g_uQ_U+g_vQ_V.
$$

Therefore compute the Bernstein coefficients for $Q_U$ and $Q_V$ once per active temporary segment. A node can then evaluate its gradient-specific bound from a fixed number of fused multiply-add and max/absolute-value operations,

$$
M_g=\max_i|g_u\beta_{U,i}+g_v\beta_{V,i}|,
$$

rather than rebuilding polynomial products. When refinement is needed, obtain child Bernstein coefficients by fixed-degree de Casteljau subdivision and reuse all unchanged rational-ray data.

The current Python lazy traversal is deliberately more aggressive: it relinearizes after many node clips to expose and validate the mechanism. It is a correctness/reference implementation, not the CUDA scheduling blueprint. The CUDA version must measure and limit the extra certificate work. Its break-even condition is conceptually

$$
(L_{\mathrm{lazy}}-L_{\mathrm{global}})C_{\mathrm{lin}}
<
(N_{\mathrm{global}}-N_{\mathrm{lazy}})C_{\mathrm{node}}
+
(K_{\mathrm{global}}-K_{\mathrm{lazy}})C_{\mathrm{leaf}},
$$

where $L,N,K$ denote linearizations, visited nodes, and exact leaf tests. Report all three distributions and GPU time. If repeated refinement, register pressure, or divergence erases the saving, retain global certified segmentation or exact nonlinear traversal as the runtime fallback. Expected favorable cases are coarse proxies with high-resolution displacement and expensive leaf work; finely tessellated, nearly affine, flat, or grazing cases are likely unfavorable.

## 16. Ray-tracing baselines

Use two evaluation tiers because not every published method represents exactly the same filtered surface.

### 16.1 Tier I — same-surface algorithm comparison

All methods use the same triangle proxy, texture microtriangulation, displacement values, shell directions, ray stream, and analytic oracle.

| Method | Purpose | Priority / status |
|---|---|---|
| Exhaustive analytic microtriangle roots | CPU correctness oracle, not performance | Mandatory; planned Python reference. |
| Ogaki nonlinear shell-BVH traversal | Closest direct algorithmic baseline | Mandatory; Mode 0 exists, must be audited. |
| Fixed-$N$ uniform chords | Segmentation strawman | Sweep $N$ at equal oracle correctness/error. |
| Global adaptive curve flattening to fixed geometric/UV tolerance | Ogaki-adjacent curve-side baseline | Mandatory; use the same certified rational bound where possible so only coupling changes. |
| Global certified tubes + min/max traversal | Isolate zero-order hierarchy | Mandatory ablation. |
| Global certified tubes + Taylor-slab traversal | Isolate first-order hierarchy with segmentation fixed | Mandatory ablation and first GPU gate. |
| Lazy Taylor traversal + componentwise tube projection | Isolate lazy node-local knot generation | Mandatory ablation. |
| Lazy Taylor traversal + direct gradient projection | Isolate the first-order-specific ray-side certificate | Mandatory ablation; full stack/quadtree proposal before DDA. |
| Certified tube + Taylor-slab hierarchical DDA | Full proposed method | Target. |
| Current heuristic Mode 1 | Historical engineering checkpoint | Report separately; not a correctness baseline. |

### 16.2 Tier II — matched-quality system comparison

Each method is swept to comparable error against its declared surface/oracle. Report quality–time–memory Pareto curves rather than one nominal setting.

| Baseline | Why it matters | Plan |
|---|---|---|
| TFDM (Thonat et al. 2021) | Established tessellation-free affine-bound hierarchy | Local implementation exists; mandatory. |
| RMIP (Thonat et al. 2023) | Strong ray-adaptive oblong-bounding method; reported large gains over TFDM | Mandatory; obtain/port a faithful implementation or document any reproduction gap. |
| PDM (Hoetzlein 2025) | Recent direct-sampling method designed for editable surfaces | Mandatory for the editability claim; sweep step/thin-feature settings at matched error. |
| Dense displaced triangle BLAS | Hardware triangle speed and memory reference | Mandatory across tessellation levels. |
| NVIDIA DMM | Hardware-assisted compressed microgeometry reference | Mandatory when available on the test GPU/API; otherwise state the hardware limitation and include memory/build estimates separately. |

For TFDM, RMIP, and PDM, compare both the authors' intended reconstruction and the closest common-surface configuration that can be implemented honestly. Do not hide filtering or LoD differences inside timing tables.

### 16.3 Ray workloads and measurements

- Isolated primary-ray G-buffer.
- Closest-hit secondary rays.
- Any-hit shadow rays with normalized termination behavior.
- Fixed path-tracing workload and ray counts.
- Static displacement and full/local displacement edits.

Report:

- ns/ray and complete-frame time;
- hit/miss, $t$, position, normal, and silhouette error;
- p50/p95/p99 segments, nodes, cells, leaves, and fallbacks;
- memory including textures, hierarchy, proxy, BLAS, build scratch, and temporary buffers;
- preprocessing, BLAS build/refit, and edit-to-render latency; and
- registers, spills, occupancy, L2/texture hit rate, and DRAM traffic.

### 16.4 Contribution boundary and expected advantage over each baseline

The ray-only idea must not be described as “replace Ogaki's curved ray with line segments.” Piecewise approximation of curved canonical rays predates this project. The candidate contribution is the complete conservative composition:

> Event-partition the rational shell ray, enclose each interval by a certified UV tube, traverse that tube against a conservative first-order displacement slab, and retain exact cubic microtriangle intersections at the leaves.

The role and expected advantage relative to the mandatory baselines are:

| Baseline | Baseline strength | Intended advantage of this method | Expected loss regime |
|---|---|---|---|
| Ogaki nonlinear ray tracing | Retains and processes the exact nonlinear shell ray; closest same-surface algorithmic baseline. | Pay nonlinear curve analysis at interval/segment construction, then use cheap affine tube–slab tests at most nodes and exact cubic work only at leaves. | Thick, grazing, distorted, or nearly singular shells can require many segments or fallback. |
| TFDM | Mature tessellation-free displacement hierarchy with conservative affine bounds and low memory. | Factor local slope out of ray rejection; reuse the stored height/gradient model for metric and area queries. | The ray view has four values instead of two; high-frequency maps may not repay the bandwidth. |
| RMIP | Strong ray-adaptive inverse mapping and oblong bounds; currently the most demanding software traversal baseline. | Smooth slope-dominated regions may benefit from very thin surface-aligned residual slabs; the hierarchy is query-independent and shared with sampling. | RMIP's ray-adaptive regions may be tighter for grazing or irregular traversal and may win ray-only memory/performance. |
| PDM | Simple direct sampling without a displacement BLAS, designed for interactive editing. | Certified hit coverage for the declared analytic microtriangle surface and hierarchical empty-space skipping rather than finite-step sampling alone. | PDM can be cheaper and faster at accepted approximation error, especially under frequent edits. |
| Dense triangle BLAS | Native hardware traversal and straightforward fixed-geometry correctness. | Avoid displacement-resolution vertex/index data and a dense triangle BLAS; potentially cheaper edits and instancing. | Static scenes that fit in memory will often favor hardware triangles. |
| NVIDIA DMM | Compact hardware-assisted structured microgeometry and a high raw-performance ceiling. | Software-defined analytic surface, no DMM encoding path, and a possible combined visibility/measure structure. | Supported hardware will likely win static raw ray speed; DMM and animated-DMM results prevent a simplistic “DMM is static” claim. |

The main novelty candidates, subject to a focused prior-art search, are:

1. the exact piecewise-affine displacement leaf and quadrant-aware conservative fold over $(h,\nabla h)$;
2. the certified rational-ray tube and its singular/event policy;
3. the derived tube-aware Taylor-slab rejection width $r+|g_u|\epsilon_u+|g_v|\epsilon_v$; and
4. reuse of the same hierarchy for visibility and surface-area/product sampling.

Not novel in isolation: triangle proxy shells, tessellation-free displacement mapping, nonlinear canonical rays, cubic leaf roots, piecewise ray segments, DDA, min/max pyramids, Taylor models, affine arithmetic, or hardware-BVH integration.

The paper-level claim is therefore conditional rather than universal: faster or competitive ray traversal in smooth, thin, well-conditioned, non-grazing regimes, together with a favorable **combined** visibility–sampling memory/build/update Pareto point. “Faster than every baseline” is not a credible target. If the first-order hierarchy does not improve GPU ray time in any intended regime, or if area sampling needs an unrelated structure, the unified contribution is not strong enough in its current form.

## 17. Baselines for Application B — area/product sampling

The sampling evaluation must distinguish a within-surface distribution from an outer many-light tree. Give every within-surface method the same outer face/light selection when testing many emitters.

### 17.1 Correctness and static-quality baselines

| Baseline | Distribution / role |
|---|---|
| Uniform parameter sampling | Cheapest valid proposal; $p_A=p_{uv}/\sqrt{\det G}$. |
| Emission-only texture hierarchy | Samples $E$ but ignores displaced area variation. |
| Area-only scalar hierarchy | Stores or quadratures $J=\sqrt{\det G}$ directly; strongest specialized area-only competitor. |
| Fused scalar product hierarchy | Stores/quadratures $EJ$ directly; strongest static within-face competitor. |
| Dense analytic-microtriangle alias/CDF | High-memory static reference for area or emission-weighted area. |
| Tessellated mesh-light BVH / Conty–Kulla-style light tree | Production-style spatial/orientation hierarchy and quality reference. |
| Ling et al. 2025 ray-cast uniform surface sampling | Representation-agnostic tessellation-free alternative; run early to test whether a metric pyramid is needed for uniform area. Account explicitly for its all-intersections ray-query requirement rather than pricing it as one closest-hit call. |
| Joint Taylor hierarchy with composed weights | Proposed shared, base/instance-aware method. |
| Joint Taylor hierarchy with fused weights | Upper-quality variant that gives up some sharing/update advantages. |

Wavelet or quadtree product-sampling methods are related work; include a direct implementation only if the final claim is general dynamic product sampling rather than the narrower $EJ$ surface product.

### 17.2 Sampling comparisons

Run both:

1. **area-only sampling**, to validate measure and compare against Ling et al. and scalar $J$ structures; and
2. **emission-times-area sampling**, using maps where emission is positively correlated, negatively correlated, and uncorrelated with displacement slope.

Metrics:

- PDF normalization and sample-histogram agreement with dense integration;
- equal-sample and equal-time variance;
- rejection/acceptance rate if rejection is used;
- path-depth and node-fetch distributions;
- total memory and preprocessing;
- full and localized updates after changing $h$ or $E$;
- reuse under a changed base triangle, instance displacement amplitude, and nonuniform transform; and
- final direct-lighting error with consistent MIS.

The critical comparison is not “ours versus uniform.” It is the shared Taylor structure versus specialized scalar $J$/$EJ$ structures, including their rebake and instancing costs.

## 18. Paper baseline priority under time pressure

### Must have

- Ogaki / Mode 0, all same-surface ablations, TFDM, RMIP, PDM, and dense triangles for ray tracing.
- Uniform parameter, scalar $J$, scalar $EJ$, dense alias/CDF, and Ling et al. for sampling.
- Independent correctness oracles for both applications.

### Hardware-dependent but highly desirable

- NVIDIA DMM.
- Production-style mesh-light tree over dense tessellation.

### Related-work-only unless the claim expands

- General wavelet product sampling.
- ReSTIR/light-resampling systems, which consume initial light proposals rather than replace the surface sampler.
- Heat-method, PWoS, and LOD baselines.

## 19. Decisions now in force

The five initial planning decisions have been reviewed through the prototype work and are now active:

1. The common surface is the analytic triangle-shell image of fixed piecewise-affine texture microtriangles.
2. Python plus recursive quadtree traversal is the correctness/tightness reference before CUDA.
3. The first GPU version retains exact nonlinear leaf roots; leaf prediction/iteration is optimized only after traversal wins are measured.
4. Denominator and shell degeneracies take a conservative exact fallback rather than an incomplete special-case solver.
5. Evaluation uses both same-surface ablations and matched-quality external baselines.

The current implementation gate is equally explicit: no CUDA port begins until saved-seed/adversarial validation, explicit event-root intervals, representative asset tightness sweeps, and the focused novelty audit are complete. The first authorized GPU experiment, if those gates pass, changes only min/max versus four-component Taylor-slab rejection while holding segmentation fixed.

## 20. Sources to verify and track

- [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]]
- [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]]
- [[Thonat et al. 2023 — RMIP]]
- [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]]
- [NVIDIA Micro-Mesh / Displaced Micro-Mesh](https://developer.nvidia.com/rtx/ray-tracing/micro-mesh)
- [Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes](https://doi.org/10.1111/cgf.15225)
- [Ling et al. 2025 — Uniform Sampling of Surfaces by Casting Rays](https://diglib.eg.org/items/dc456507-a16d-420a-9ae1-e4c9b993cf41)
- [Conty Estevez and Kulla 2018 — Importance Sampling of Many Lights with Adaptive Tree Splitting](https://fpsunflower.github.io/ckulla/data/many-lights-hpg2018.pdf)
- [Clarberg et al. 2005 — Wavelet Importance Sampling](https://cs.dartmouth.edu/~wjarosz/publications/clarberg05wavelet.html)

---

Related: [[Project — Conservative first-order queries on displacement maps]] · [[Taylor-model bound pyramid]] · [[The induced metric of a displaced surface]] · [[Surface measure and sampling on implicit displaced surfaces]]
