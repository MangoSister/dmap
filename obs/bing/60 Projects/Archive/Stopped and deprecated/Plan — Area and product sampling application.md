---
title: Plan — Area and product sampling application
tags: [project, plan, displacement, area, importance-sampling, taylor-model, baselines]
status: stopped-at-s3-gate
created: 2026-08-25
updated: 2026-08-25
---

# Plan — Area and product sampling application

> [!abstract] Decision to make
> Determine whether the existing joint height/gradient hierarchy supports a practically useful second application on the **same triangle-proxy displaced surface** as the completed ray study. The stage must establish pointwise and integral correctness first, then compare shared first-order composition against strong surface-specific area structures. Area-only sampling precedes emission-times-area sampling. GPU work is authorized only after the CPU representation and sampling gates pass.

> [!warning] Current contribution status
> The area application is not yet a result. The induced-metric identity is classical machinery; the candidate contributions are conservative localized composition from a reusable displacement hierarchy, exact algorithmic PDFs, and a favorable update/instancing/combined-query tradeoff. Ling et al. already provide global area estimation and uniform surface sampling from ray casting, while a surface-specific scalar Jacobian hierarchy is a strong static baseline. These comparisons can kill the shared-framework thesis.

> [!failure] Final S3 decision — 2026-08-25
> The comparison did kill the shared-framework performance thesis. Run `area-s3-full-f245aff67ea7` completes 144 surfaces. On 69 nontrivial ordinary surfaces, local on-the-fly `FO-PROP` has `16.342x` the UV-uniform variance. Subtree-summed `FO-LEAFCDF` reduces that ratio to `0.1377x`, but needs per-instance weights, is `1.4729x` `J-LEAFCDF` in geometric mean and `2.1785x` at p90, and never beats dense-cell variance. It also has no memory advantage over the specialized J hierarchy. Gate S3 fails; S4–S6 remain intentionally unimplemented.

## 1. Why this stage is next

The certified ray architecture is correct but failed its frozen same-surface GPU performance gate. It remains evidence for a tessellation-free extrinsic query, but it does not establish a speed advantage for first-order node payloads. The area application is therefore the decisive test of whether retained gradient information has practical value and whether the project remains a coherent two-query paper.

No closest-point, PDE, Catmull–Clark, or third-application work begins during this plan. If this stage fails, the response is to narrow the paper, not to add another unfinished application.

## 2. Frozen represented surface

For one proxy triangle, texture coordinates determine affine barycentric coordinates. Write the affine proxy fields directly in texture space as

$$
P(u,v)=p_0+p_u u+p_v v,
\qquad
N(u,v)=n_0+n_u u+n_v v.
$$

The direction field $N$ is the **unnormalized, scaled affine displacement direction used by the ray implementation**. It includes displacement amplitude. We do not substitute a normalized shading normal or the unit-normal offset-surface formula from the older concept note.

On each fixed-diagonal texture microtriangle $\tau$,

$$
h_\tau(u,v)=c_\tau+g_{\tau u}u+g_{\tau v}v,
$$

and the represented surface is

$$
S_\tau(u,v)=P(u,v)+h_\tau(u,v)N(u,v).
$$

Its exact first derivatives almost everywhere are

$$
S_u=p_u+g_{\tau u}N+h_\tau n_u,
\qquad
S_v=p_v+g_{\tau v}N+h_\tau n_v,
$$

and its parameter-to-area Jacobian is

$$
J_\tau(u,v)=\left\|S_u\times S_v\right\|
            =\sqrt{\det G},
\qquad
G_{ij}=S_i^\mathsf{T}S_j.
$$

$S$ is quadratic on each microtriangle, $S_u$ and $S_v$ are affine, and $J^2$ is a polynomial of degree at most four. Derivative jumps on microtriangle edges have measure zero. All integrals and PDFs are over the proxy texture triangle, so every grid cell is clipped against both the proxy footprint and the fixed microtriangle diagonal.

### 2.1 Surface-identity tests

The implementation must verify all of the following:

1. direct $S_u,S_v$ agree with derivatives of the expanded quadratic surface;
2. $\|S_u\times S_v\|$ agrees with $\sqrt{\det G}$;
3. finite differences agree away from reconstruction edges;
4. constant parallel directions reduce to the flat height-field sanity case;
5. zero height gives the proxy-triangle area;
6. the same packed positions, directions, texture coordinates, and height samples used by A2 reproduce the area input exactly.

## 3. Three sampling semantics that must not be conflated

### 3.1 Non-null hierarchical proposal with exact PDF

Choose a child with stored deterministic weight $w_c$, descend to a clipped leaf region $\Omega_L$, and draw $(u,v)$ uniformly there. The parameter- and surface-area densities are

$$
p_{uv}(u,v)=
\left(\prod_{\ell=1}^{L}P(c_\ell\mid c_{\ell-1})\right)
\frac{1}{|\Omega_L|},
\qquad
p_A(S(u,v))=\frac{p_{uv}(u,v)}{J(u,v)}.
$$

This is an ordinary non-null importance sampler. Its PDF is exact for the algorithm even when node weights only approximate area or emitted power. It is **not** exactly area-proportional unless the path probabilities encode exact regional integrals.

### 3.2 Conservative null-event sampler

Let an adaptive partition contain regions $\Omega_i$ with conservative caps

$$
M_i\ge t(u,v),
$$

where $t=J$ for area-only sampling and $t=EJ$ for product sampling. Define

$$
Z=\sum_i |\Omega_i|M_i.
$$

Select region $i$ with probability $|\Omega_i|M_i/Z$, sample uniformly inside it, and accept with probability $t(u,v)/M_i$. Do not repeat silently. For one attempt, accepted area-domain density is

$$
p_A(S(u,v))=
\begin{cases}
1/Z, & t=J,\\
E(u,v)/Z, & t=EJ,
\end{cases}
$$

and the remaining probability is an explicit null event. This gives a known normalized distribution over **surface points plus null**, without requiring the unknown total area or power. The acceptance rate $A/Z$ or $\int EJ/Z$ is the central efficiency metric.

### 3.3 Repeat-until-accepted sampling

Repeating the previous construction until success produces samples exactly proportional to $J$ or $EJ$, but the conditional PDF contains the true total area or emitted power. This mode is useful for distribution validation and applications that only need points. It is not the primary renderer PDF claim unless a separately certified integral supplies the normalizer.

## 4. Conservative area composition

The shared node stores

$$
(h_0,g_u,g_v,r,\rho_u,\rho_v),
$$

with

$$
|h-(h_0+g_u\Delta u+g_v\Delta v)|\le r,
\quad
|h_u-g_u|\le\rho_u,
\quad
|h_v-g_v|\le\rho_v.
$$

For a given triangle or instance, compose these certificates through the exact derivative equations in Section 2. The first correct implementation is an outward-rounded interval/affine composer that returns

$$
J(u,v)\in[J_n^-,J_n^+]
$$

for every represented microtriangle point in the clipped node. A tighter Taylor/affine-arithmetic composer may follow only as a preplanned ablation; sampled extrema are validation data, never the certificate.

Regional area is then bounded by

$$
|\Omega_n|J_n^-\le A(\Omega_n)\le |\Omega_n|J_n^+.
$$

Adaptive subdivision sums child intervals. This yields deterministic localized bounds even if the sampler itself uses proposal weights.

## 5. Baselines

### 5.1 Area-only

1. **UV-UNIFORM:** uniform parameter-space sampling over the proxy texture triangle.
2. **DENSE-ALIAS:** high-accuracy per-region numerical area integrals plus an alias/CDF table; quality oracle and strong memory-heavy static baseline.
3. **J-SCALAR:** a surface-specific scalar $J$ or regional-area hierarchy built after composing the final shell and displacement; strong static baseline with rebuilds after relevant geometry/instance edits.
4. **TESSELLATED:** explicitly tessellated displaced surface with triangle-area sampling, swept over subdivision until it matches the represented-surface oracle.
5. **RAY-CAST:** Ling et al.'s all-intersections uniform-surface method and area estimator, implemented only after its primary paper and protocol are re-audited. It is the representation-agnostic baseline for global uniform sampling and area estimation.
6. **FO-COMPOSED:** the proposed shared height/gradient hierarchy composed with the shell at query/build time.

### 5.2 Emission-times-area

1. uniform parameter and area-only proposals;
2. emission-only scalar hierarchy;
3. surface-specific scalar $EJ$ hierarchy or dense product alias table;
4. shared first-order geometry bounds combined with an emission hierarchy;
5. a pre-tessellated emissive-mesh sampler or light-BVH baseline when the GPU stage begins.

No baseline timing is described as fair until surface identity, filtering, target integrand, returned PDF, null handling, and sample budget match.

## 6. Dataset and experiment axes

### 6.1 Geometry corpus

Reuse the frozen practical displacement data and A2 packed surface inputs:

- 48 audited 65-by-65 practical windows, including coherent, discontinuous, and high-frequency crops;
- `S0-affine`, `S1-moderate`, and `S2-stress` affine direction fields from the signed A2 replay;
- the standard proxy texture triangle stored in those same packed A2 records;
- displacement-amplitude sweeps and instance transforms after the base contract passes.

The decision corpus is the Cartesian product of the 48 windows and three shells. A smaller named development subset may be used for debugging, but gates are evaluated only on frozen held-out records.

### 6.2 Product fields

After area-only passes, add deterministic nonnegative emission fields with known provenance:

- constant emission;
- smooth spot and sparse hot region;
- emission positively correlated with $J$;
- emission anti-correlated with $J$;
- independent natural/image texture;
- high-frequency checker/noise stress.

Correlation-controlled cases are essential: they distinguish product sampling from merely choosing the better of area-only or emission-only.

### 6.3 Update and instancing axes

- static surface, many samples;
- local and global displacement edits;
- emission-only edits;
- 1, 4, 16, and 64 instances sharing one displacement map but changing shell directions, amplitude, or nonuniform transform;
- combined ray-plus-sampling resident memory and rebuild latency.

## 7. Metrics

### 7.1 Correctness

- pointwise derivative, Gram/cross-product, and finite-difference error;
- independent total and regional area-integral agreement;
- conservative-bound violations and bound width by level;
- numerical normalization of $p_{uv}$ and $p_A$;
- empirical region probabilities with confidence intervals;
- unbiased integral-estimator z-scores over repeated trials;
- edge ownership, clipped-domain, degeneracy, and null-event accounting failures.

### 7.2 Quality and efficiency

- $Z/A$ and acceptance $A/Z$ for area null sampling;
- product-cap ratio and product acceptance;
- variance, time, and variance-times-time for fixed integrands;
- samples, node visits, bound compositions, and exact $J$ evaluations per output attempt;
- adaptive partition size and bytes;
- build, full-update, local-update, and instance-composition time;
- resident and scratch memory;
- GPU bandwidth, cache, registers, and occupancy only after the CPU gates.

Report distributions per surface and cohort, including p50/p95/p99/max where tails matter. Do not replace paired per-surface results with one pooled percentage.

## 8. Ordered implementation stages and gates

### S0 — exact metric and independent area oracle

Implement two pointwise $J$ paths: direct cross product and Gram determinant. Integrate every clipped reconstruction piece with deterministic high-order/adaptive quadrature, and cross-check selected cases with an independent higher-precision path.

**Gate S0:** zero domain/ownership failures; direct and Gram $J$ agree to `2e-12` relative/scale-aware error in binary64; finite differences agree to `2e-6` away from edges; independent ordinary/stress area totals agree to `1e-9`/`1e-7` relative error; all analytic sanity cases pass.

### S1 — conservative bounds and representation-value decision

Implement `FO-COMPOSED` first and `J-SCALAR` as the strong surface-specific comparator. Validate certificates analytically where possible and with exhaustive leaf/selected high-precision checks. Construct equal-budget adaptive partitions.

**Gate S1:** zero certified-bound violations; on the ordinary held-out cohort, `FO-COMPOSED` area-null acceptance has median at least `0.60` and p10 at least `0.25`; at equal adaptive-region count, its median/p90 cap overhead relative to `J-SCALAR` is at most `1.35x`/`2.0x`. Failure stops attempts to sell first-order area efficiency, but the localized-bound result may still survive as a correctness capability.

### S2 — area-only samplers and PDF audit

Implement the non-null proposal, the one-attempt null sampler, and repeat-until-success only for validation. Add UV-uniform, dense-alias, and scalar-J baselines.

**Gate S2:** numerical PDF mass error at most `1e-8`; no positive-target zero-PDF events; all predefined empirical region tests pass after multiple-test correction; repeated integral estimates remain within four standard errors of the oracle; null frequency agrees with `1-A/Z` within its confidence interval.

### S3 — area-only practical decision

Run the full CPU comparison before any GPU port.

**Gate S3:** across nontrivial ordinary maps, `FO-COMPOSED` must improve the geometric-mean variance-time product over UV-uniform by at least `10%` and win on at least two thirds of surfaces. Relative to `J-SCALAR`, it must remain within `1.5x` variance-time and demonstrate at least one material reuse advantage: `2x` lower relevant rebuild/update time or a crossover under shared instancing/combined-query memory. Otherwise area remains a demonstration rather than the first-order performance result.

### S4 — emission-times-area product sampler

Add emission bounds and all controlled correlation cases. Keep the exact path-PDF and null-event semantics from Section 3.

**Gate S4:** zero correctness regressions; in the positive-, negative-, and independent-correlation cohorts, the shared product proposal must reduce geometric-mean variance-time by at least `20%` relative to the better of area-only and emission-only, while remaining within `1.5x` of the surface-specific `EJ` structure or showing the same update/instancing crossover required by S3.

### S5 — GPU and external-baseline comparison

Port only methods that pass S0–S4. Use randomized method order, warmup, repeated GPU-event timing, locked-clock reporting when available, identical sample streams, and separate kernel and end-to-end results. Implement the tessellated and ray-cast baselines at this stage; do not use the failed A2 candidate timing as a substitute for an all-intersections Ling baseline.

**Gate S5:** no PDF/correctness regression and a defensible quality-time-memory-update Pareto advantage in at least one predeclared practical regime. A static raw-speed loss is acceptable only if the measured update, instancing, or combined-query crossover is substantial and clearly stated.

### S6 — paper decision

Freeze figures, signed run IDs, exact claims, negative results, and a draft handoff. Only if the first two applications and the shared-system comparison are coherent may a closest-point third application be considered.

## 9. Required figures

1. Same-surface diagram: proxy texture triangle, clipped fixed-diagonal microtriangles, and mapped quadratic surface.
2. Metric diagram: $S_u,S_v$, their cross product, and local area stretching.
3. Localized bound figure: shared first-order versus scalar-J interval tightening across levels.
4. Sampling semantics figure: exact-PDF proposal versus null-event rejection; explicitly show the null mass.
5. Area-bound convergence by adaptive node budget.
6. Acceptance/cap-overhead distributions by map and shell.
7. Variance-time and memory-update Pareto plots.
8. Product-correlation ablation.
9. Static-versus-edited and instance-count crossover.
10. Final combined ray-plus-sampling resource table.

## 10. Draft material that is safe now

Safe to derive and write:

- the exact unnormalized affine-direction surface derivatives in Section 2;
- the distinction among proposal, null-event, and conditional rejection semantics;
- the exact algorithmic PDF in Section 3.1;
- the null-event density derivation in Section 3.2;
- baseline definitions, surface contract, and proof obligations.

Not yet safe to write as results:

- that first-order area bounds are tight;
- that the sampler is faster or lower variance than specialized structures;
- that the shared hierarchy saves memory;
- that samples are exactly area/product proportional **and** have a known normalized PDF without explaining the integral normalizer or null event;
- that the two-query project is sufficient for Eurographics.

## 11. Immediate next action

S0 development run `area-s0-a89dd07aafec` passes on the signed A2 packed inputs: eight unit/differential tests pass; the largest direct-versus-Gram relative discrepancy is `4.13e-16`; the largest tangent finite-difference discrepancy is `4.26e-10`; and fixed-order versus adaptive total area differs by at most `3.53e-16` across the three development surfaces. The analytic parallel-ramp, zero-height, and parameter-partition checks pass. This authorizes the detailed S1 plan, not a first-order contribution claim.

Execute only items 1–3 of [[Plan — S1 conservative area bounds]] next: interval/polynomial utilities, exact per-piece $J^2$ coefficients, triangular Bernstein certification, and the surface-specific `J-SCALAR` comparator. Do not implement `FO-COMPOSED` until those comparator tests pass.

Items 1–3 now pass 13 combined S0/S1 tests. A complete 64-by-64 practical `J-SCALAR` build contains 5,461 logical nodes, covers parameter area `0.5`, and has zero parent-containment violations; the unoptimized binary64 interval-polynomial reference takes about 33.3 seconds and is correctness scaffolding, not timing evidence. This authorizes items 4–5 of the S1 plan: implement and audit `FO-COMPOSED` before adding frontier experiments.

Items 4–7 also pass, for 16 combined tests total. Development run `area-s1-dev-95330a0df2b9` is correctness-clean and previews a positive leaf-budget result, but shows large first-order overhead at coarse budgets on the stress case. Execute the frozen, resumable 144-surface S1 decision run next. Sampling remains blocked.

Full S1 run `area-s1-full-8d3e3c63583a` passes over all 144 surfaces. Near cell resolution the shared first-order caps are very close to the surface-specific scalar-$J$ caps, including the stress cohort; coarse caps remain the main weakness. Execute only items 1–3 of [[Plan — S2 area sampling and PDF audit]] next.

S2 run `area-s2-cac179574d19` passes the proposal, null-event, repeat-distribution, PDF, and estimator audits. Correctness is no longer the blocking question. Execute only items 1–3 of [[Plan — S3 area sampling quality and cost decision]] next.

S3 development run `area-s3-dev-f94aa877d085` shows that correct local hierarchical proposals can still have worse variance than UV-uniform because child-cap normalization does not telescope to leaf masses. Run the single authorized subtree-sum repair, with its per-instance storage counted, before the complete S3 decision.

Full S3 run `area-s3-full-f245aff67ea7` signs the negative practical decision. The first-order cap representation is conservative and leaf-tight, and the repaired distribution is much better than UV, but the repair specializes the hierarchy per surface and loses its p90, dense-quality, and storage comparisons. Product sampling and GPU/external sampling baselines are stopped by the predeclared gate rather than executed selectively after failure.

Related: [[Project — Conservative first-order queries on displacement maps]] · [[Plan — Certified ray architecture comparison]] · [[Taylor-model bound pyramid]] · [[The induced metric of a displaced surface]] · [[Surface measure and sampling on implicit displaced surfaces]]
