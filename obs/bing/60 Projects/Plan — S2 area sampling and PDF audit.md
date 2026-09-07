---
title: Plan — S2 area sampling and PDF audit
tags: [project, plan, area-sampling, pdf, rejection-sampling, correctness]
status: planned-before-implementation
created: 2026-08-25
updated: 2026-08-25
---

# Plan — S2 area sampling and PDF audit

> [!abstract] Isolated decision
> Turn the S1-certified area caps into executable samplers and prove their probability semantics. S2 is a correctness and distribution stage on three development surfaces. It does not claim GPU speed or full-corpus variance superiority. A pass authorizes the S3 practical comparison.

## 1. Prerequisite

Full S1 run `area-s1-full-8d3e3c63583a` passes all six correctness checks and all four frozen scientific gates over 144 signed-A2 surfaces. On the ordinary cohort, leaf `FO-COMPOSED` acceptance is median `0.9945`, p10 `0.9658`, and minimum `0.7867`; cap overhead over `J-SCALAR` is median `1.0016x`, p90 `1.0149x`, and maximum `1.1373x`. Stress minimum acceptance is `0.7506`.

S1 also shows that coarse first-order caps can be loose: ordinary median acceptance rises from roughly `0.47` at the root to `0.9945` near cell resolution. S2 therefore records the complete number of level decisions and bound evaluations. A high final acceptance does not imply a cheap sampler.

## 2. Exact leaf-domain construction

For each grid cell, clip its axis-aligned square against the proxy texture triangle. Triangulate the resulting convex polygon into a fan. To draw uniformly in the clipped cell:

1. choose a fan triangle proportional to its parameter-space area;
2. draw a uniform barycentric point with the square-root transform;
3. classify the fixed-diagonal height microtriangle with one shared boundary convention;
4. evaluate the exact affine height plane and exact pointwise $J=\|S_u\times S_v\|$.

The returned parameter density is exactly $1/|\Omega_L|$ within the selected clipped leaf. Cell, reconstruction-diagonal, and proxy-boundary points use deterministic closed ownership for reproducibility; measure-zero ownership does not change the PDF.

## 3. Samplers

### 3.1 `UV-UNIFORM`

Draw uniformly in the proxy texture triangle:

$$
p_{uv}=1/|\Omega|,
\qquad
p_A=p_{uv}/J.
$$

This is the cheapest baseline and a necessary PDF sanity check.

### 3.2 `FO-PROP` and `J-PROP`

At an internal node, assign each nonempty child

$$
w_c=|\Omega_c|J_c^+,
\qquad
P(c\mid n)=w_c/\sum_k w_k.
$$

Descend to a leaf and draw uniformly there. `FO-PROP` obtains $J_c^+$ from `FO-COMPOSED`; `J-PROP` reads `J-SCALAR`. For the selected path $c_1,\dots,c_L$,

$$
p_{uv}=\left(\prod_{\ell=1}^{L}P(c_\ell\mid c_{\ell-1})\right)
\frac1{|\Omega_L|},
\qquad
p_A=p_{uv}/J.
$$

These samplers always return a point and have an exact algorithmic PDF. They are not called exactly uniform-area samplers.

### 3.3 `FO-NULL` and `J-NULL`

Use the complete nonempty leaf frontier. Let

$$
Z=\sum_L |\Omega_L|J_L^+.
$$

Choose a leaf proportional to $|\Omega_L|J_L^+$, draw uniformly, and accept with probability $J/J_L^+$. On one attempt,

$$
p_A(S(u,v))=1/Z
$$

for accepted points, with an explicit null event of probability $1-A/Z$. No hidden repeat is allowed in this mode.

### 3.4 `FO-REPEAT` and `J-REPEAT` — validation only

Repeat the null attempt until success. Accepted points are uniform with respect to surface area, and the validation PDF is $1/A$ using the independent S0 oracle area. This mode proves distribution semantics but is not the renderer-facing exact-PDF claim because the true normalizer must be supplied separately.

## 4. Independent cell-area oracle

Integrate every clipped reconstruction piece with the S0 order-10 rule and sum by cell. The resulting nonnegative cell areas must sum to the signed S0 total to `1e-10` relative error. Aggregate them into an 8-by-8 coarse binning for empirical uniform-area tests, omitting zero-area bins.

This oracle does not steer `FO-PROP`, `J-PROP`, or null proposal construction.

## 5. PDF and distribution audits

For each development surface and sampler:

1. **Analytic path mass:** recursively sum all leaf path probabilities; error at most `1e-12`.
2. **PDF identity:** recompute $p_{uv}$ independently from the recorded path and verify the returned $p_AJ=p_{uv}$ at every sample.
3. **Positive support:** no positive-area point receives zero or nonfinite PDF.
4. **Proposal frequencies:** compare empirical leaf or coarse-bin counts against exact proposal probabilities.
5. **Null frequency:** compare observed null count against $1-A/Z$ with a binomial z-score.
6. **Repeat distribution:** compare accepted coarse-bin counts against oracle regional area fractions.
7. **Integral estimates:** estimate at least $\int 1\,dA$, $\int u\,dA$, and $\int v\,dA$ using returned PDFs and compare replicated means to independent quadrature.

Use deterministic, disjoint RNG streams and record the exact seed. Statistical checks use predeclared four-standard-error limits; do not rerun with a new seed after seeing a failure. Analytic failures are never excused statistically.

## 6. Development corpus and sample counts

Use the same three named S0/S1 development surfaces:

- `PH-rock05-4k-W0 / S0-affine`;
- `PH-brick001-1k-W0 / S1-moderate`;
- `PH-leather02-1k-W0 / S2-stress`.

For distribution audits, use `131072` attempts per sampler and surface. For estimator audits, use `32` independent trials of `4096` attempts. A null is a zero contribution and remains part of the fixed attempt budget.

If this reference count is too slow, optimize the Python implementation without changing streams or counts; do not lower the statistical gate after inspecting results.

## 7. Metrics

- path probability and PDF-mass error;
- null rate and acceptance;
- chi-square standardized residual for proposal and repeat distributions;
- maximum bin standardized residual where expected count is at least 20;
- estimator bias z-score and empirical variance;
- hierarchy levels, child-cap evaluations, exact-$J$ evaluations, and attempts per returned non-null point;
- cap and path arithmetic counts;
- reference CPU time, explicitly not GPU evidence.

The S3 efficiency comparison will use variance-times-time; S2 reports work counts so a correctness pass cannot hide a pathological algorithm.

## 8. Frozen S2 gate

S2 passes only if all development surfaces satisfy:

- analytic path mass error at most `1e-12`;
- numerical/analytic PDF mass error at most `1e-8`;
- zero support, ownership, or nonfinite failures;
- proposal-frequency, null-frequency, and repeat-distribution tests within four standard errors after the predefined bin filtering;
- all replicated integral means within four standard errors of their quadrature oracles;
- measured null acceptance agrees with S1's $A/Z$ prediction within four standard errors.

Efficiency thresholds are deliberately absent from S2. A correctness pass authorizes S3; it does not establish a practical contribution.

## 9. Ordered implementation

1. Implement clipped-cell uniform sampling and pointwise owner/J evaluation with tests.
2. Implement generic hierarchy path sampling and independent path-PDF reconstruction.
3. Add `UV-UNIFORM`, `FO-PROP`, and `J-PROP`; pass analytic mass tests.
4. Add leaf-frontier CDF and one-attempt null samplers; test null accounting.
5. Add repeat-until-success for distribution validation only.
6. Build per-cell area/moment oracles and statistical audit helpers.
7. Run a small deterministic smoke without changing the frozen final seed/counts.
8. Execute and persist the complete three-surface S2 gate.
9. Update the paper guide and plan S3 before any practical timing implementation.

## 10. Immediate implementation boundary

Implement items 1–3 only and run unit tests. Do not add rejection/repeat modes until proposal PDF reconstruction passes.

Related: [[Plan — Area and product sampling application]] · [[Plan — S1 conservative area bounds]] · [[Project — Conservative first-order queries on displacement maps]]
