---
title: Plan — S1 conservative area bounds
tags: [project, plan, area, conservative-bounds, taylor-model, ablation]
status: planned-before-implementation
created: 2026-08-25
updated: 2026-08-25
---

# Plan — S1 conservative area bounds

> [!abstract] Isolated decision
> At equal adaptive region count, does the reusable first-order height/gradient node produce area-density caps close enough to a strong surface-specific scalar-$J$ hierarchy to support efficient localized area bounds and null-event sampling? This stage measures representation value only. It does not yet generate random samples or time GPU kernels.

## 1. Prerequisite and input contract

S0 development run `area-s0-a89dd07aafec` passes its pointwise, integration, partition, and analytic sanity checks. S1 continues to use the signed A2 inputs from `ray-a2-0-replay-144f75125df7`:

- 48 packed 65-by-65 height windows;
- `S0-affine`, `S1-moderate`, and `S2-stress`;
- standard proxy texture triangle;
- the fixed two-microtriangle reconstruction and unnormalized affine displacement directions.

S1 must materialize a fixed-order oracle area for every surface it evaluates. The full 48-by-3 decision corpus is run only after development tests and a named three-case smoke pass.

## 2. Proposed shared first-order cap: `FO-COMPOSED`

For a quadtree node with rectangle $[u^-,u^+]\times[v^-,v^+]$, the existing certificate gives

$$
h=h_0+g_u(u-u_0)+g_v(v-v_0)+\delta_h,
\qquad |\delta_h|\le r,
$$

$$
h_u\in[g_u-\rho_u,g_u+\rho_u],
\qquad
h_v\in[g_v-\rho_v,g_v+\rho_v].
$$

For each world component, interval-evaluate

$$
N=n_0+n_u u+n_v v,
$$

then

$$
S_u=p_u+h_uN+hn_u,
\qquad
S_v=p_v+h_vN+hn_v,
$$

and finally

$$
C=S_u\times S_v,
\qquad
J=\|C\|.
$$

For each component interval $C_i=[c_i^-,c_i^+]$, define conservative squared bounds

$$
q_i^-=
\begin{cases}
0,&0\in C_i,\\
\min((c_i^-)^2,(c_i^+)^2),&\text{otherwise},
\end{cases}
\qquad
q_i^+=\max((c_i^-)^2,(c_i^+)^2).
$$

Then

$$
J_n^- = \sqrt{\sum_i q_i^-},
\qquad
J_n^+ = \sqrt{\sum_i q_i^+}.
$$

All primitive operations are outward-rounded in binary64. The first version is deliberately simple and auditable. A tighter affine-arithmetic composer is a later ablation only if this interval composer is correct but misses the predeclared efficiency gate by an amount with plausible recoverable headroom.

The cap is evaluated on the complete node rectangle. The cap weight uses the exact area of that rectangle clipped to the proxy texture triangle. This is conservative at boundary nodes and exposes any boundary looseness honestly.

## 3. Strong static comparator: `J-SCALAR`

`J-SCALAR` represents the best practical surface-specific scalar cap pyramid available after the final shell and displacement are known.

On each clipped integration triangle, $S_u$ and $S_v$ are affine, each component of $C=S_u\times S_v$ is quadratic, and

$$
Q(u,v)=J^2(u,v)=C(u,v)^\mathsf{T}C(u,v)
$$

is a total-degree-four polynomial. Compose $Q$ with the affine map from the reference triangle to the clipped triangle, convert its power coefficients to degree-four triangular Bernstein coefficients, and use the convex-hull property:

$$
\min b_{ijk}\le Q\le\max b_{ijk},
\qquad i+j+k=4.
$$

After outward padding and clamping the lower squared bound at zero,

$$
J^-_\tau=\sqrt{\max(0,\min b)},
\qquad
J^+_\tau=\sqrt{\max(0,\max b)}.
$$

A cell leaf takes the minimum lower bound and maximum upper bound of its clipped reconstruction pieces. A parent stores the minimum child lower bound and maximum child upper bound. This costs two scalars per node, is tied to one final displaced surface, and must be rebuilt after any change that affects $J$. It is intentionally stronger than a sampled-max mipmap.

## 4. Required certificate tests

1. Interval primitives enclose exhaustive random real-arithmetic samples, including sign-changing products and squares.
2. Polynomial coefficients reconstructed from the analytic tangents agree with direct $J^2$ evaluation.
3. Triangular Bernstein coefficients reproduce values through Bernstein evaluation.
4. Every dense point lies inside its `J-SCALAR` leaf interval.
5. Every `J-SCALAR` descendant interval lies inside every stored parent interval.
6. For `FO-COMPOSED`, every descendant microtriangle's affine height plane and constant gradient satisfy every ancestor's $(h_0,g,r,\rho)$ certificates at the exact extremal locations: height residuals at microtriangle vertices and componentwise gradient differences. Conservative interval composition then completes the analytic proof chain. Dense pointwise $J$ checks are an independent implementation audit.
7. Do **not** require one conservative cap to contain the other: two valid upper bounds may overestimate by different amounts. Compare their values only as efficiency metrics after each method's independent certificate is established.
8. Clipped node areas sum to the proxy parameter area for every frontier.
9. Exact-ramp, zero-height, affine-direction, direction-varying, near-degenerate, checker, and high-frequency cases remain finite and conservative.

Any violation blocks efficiency measurements.

## 5. Equal-budget frontier construction

For a frontier $\mathcal F$ of nonoverlapping nodes covering the proxy texture triangle, method $m$ has cap mass

$$
Z_m(\mathcal F)=\sum_{n\in\mathcal F}|\Omega_n|J^+_{m,n}.
$$

Given oracle area $A$, the one-attempt area-null acceptance is

$$
\alpha_m(\mathcal F)=A/Z_m(\mathcal F).
$$

Two frontier families are required.

### 5.1 Fixed-level frontier

Descend all intersecting nodes to the same level. This is deterministic and exposes convergence by resolution.

### 5.2 Greedy equal-count frontier

For a splittable frontier node, the known cap-mass reduction is

$$
\Delta Z_n=|\Omega_n|J_n^+-
\sum_{c\in\operatorname{children}(n)}|\Omega_c|J_c^+.
$$

Repeatedly split the node with largest nonnegative $\Delta Z_n$. Record both methods at common attainable frontier counts. Each method may choose a different frontier, but receives the same number of regions. No oracle area participates in the split decision.

Target counts are `1, 4, 16, 64, 256, 1024, 4096` after accounting for proxy clipping. The exact recorded counts may differ when a split adds fewer than four nonempty children.

## 6. Metrics and paired plots

For every surface, method, and frontier:

- region count;
- $Z$, $Z/A$, and acceptance $A/Z$;
- fixed-level or greedy construction;
- p50/p95/p99/max node cap ratio against dense/leaf maxima;
- clipped boundary versus interior cap mass;
- bytes per logical node and surface-dependent bytes;
- build/composition operation counts and CPU reference time, labeled non-GPU.

Required paired plots:

1. acceptance versus equal region count;
2. `FO-COMPOSED / J-SCALAR` cap mass versus count;
3. per-surface decision heatmap at the largest predeclared budget;
4. level-wise cap ratios split by shell and map class;
5. boundary-node contribution to excess cap mass;
6. static storage and rebuild dependency diagram.

## 7. Frozen S1 gate

Correctness requires zero certificate violations.

At the largest frontier budget not exceeding the reconstruction-cell count, on the ordinary held-out cohort:

- `FO-COMPOSED` median acceptance must be at least `0.60`;
- its p10 acceptance must be at least `0.25`;
- at equal region count, median `Z_FO/Z_JSCALAR` must be at most `1.35`;
- p90 `Z_FO/Z_JSCALAR` must be at most `2.0`.

Report the full budget curves even if the final gate fails. A pass authorizes S2 sampling. A failure stops the claim that the present first-order composer is an efficient area sampler. One tighter composer may be considered only if a recorded exact/affine upper-bound oracle predicts that it can cross the gate; otherwise preserve the negative result and reconsider the shared-framework thesis.

## 8. Ordered implementation

1. Add interval and polynomial utilities with synthetic unit tests.
2. Implement exact per-piece $J^2$ coefficients and triangular Bernstein conversion.
3. Build and validate `J-SCALAR` bottom-up.
4. Build packed-height Taylor nodes and implement `FO-COMPOSED`.
5. Run hierarchy-wide certificate audits before frontier code.
6. Implement fixed-level and greedy frontiers with coverage tests.
7. Run the three-case development smoke and inspect plots.
8. Freeze the full 144-surface decision configuration and execute it.
9. Write the signed S1 report and update the main project decision.

## 9. Immediate implementation boundary

Items 1–7 now pass 16 combined S0/S1 tests. Development run `area-s1-dev-95330a0df2b9` has zero correctness failures. At its largest fixed frontier, `FO-COMPOSED` has median/p10 acceptance `0.9811`/`0.8100` and median/p90 cap-mass overhead over `J-SCALAR` `1.0063x`/`1.1319x`; the three-case preview crosses the numerical thresholds. The curves also expose serious coarse-level looseness on the stress surface, so this is not the S1 decision.

Freeze and run item 8 next as follows:

- all 48 signed A2 windows crossed with all three signed A2 shells (`144` surfaces);
- ordinary decision cohort: `S0-affine` and `S1-moderate` (`96` surfaces);
- stress cohort: `S2-stress` (`48` surfaces), reported but not substituted into the ordinary gate;
- the exact same fixed sizes, frontier counts, cap formulas, and oracle order as the development run;
- CPU process parallelism changes only wall-clock execution and is not timing evidence;
- append-only per-case checkpoints permit exact resume after interruption;
- the signed summary reports both correctness and the frozen scientific gate without changing thresholds.

Do not implement S2 sampling until this complete run passes.

Related: [[Plan — Area and product sampling application]] · [[Project — Conservative first-order queries on displacement maps]] · [[Taylor-model bound pyramid]]
