---
title: Plan — S3 area sampling quality and cost decision
tags: [project, plan, area-sampling, variance, cost-model, baselines]
status: decision-complete-gate-failed
created: 2026-08-25
updated: 2026-08-25
---

# Plan — S3 area sampling quality and cost decision

> [!abstract] Decision
> Determine whether the now-correct first-order area sampler has enough variance and reuse headroom to justify product sampling and a GPU implementation. S3 separates distribution quality from implementation cost: variance is integrated deterministically on the complete 144-surface corpus, while cost is reported as operation/byte models plus non-authoritative CPU reference measurements. No GPU-speed claim is made in this stage.

## 1. Prerequisite

S2 run `area-s2-cac179574d19` passes all analytic and statistical gates on the three development surfaces:

- maximum PDF identity error `3.55e-15`;
- maximum absolute estimator z-score `2.33` under the frozen streams;
- minimum measured `FO-NULL` acceptance `0.7672`;
- zero support, ownership, cap, normalization, or nonfinite failures.

The first serialization attempt failed after computation because a NumPy boolean was not canonical-JSON serializable. A dedicated regression test was added and the identical frozen streams were rerun. This is recorded as a persistence correction, not an extra statistical trial.

## 2. Methods

### 2.1 `UV-UNIFORM`

Uniform parameter sampling. No hierarchy, one exact pointwise $J$ evaluation for the returned area PDF.

### 2.2 `FO-PROP`

The exact-PDF hierarchical proposal from S2. At each level, compose the four children of the selected first-order height/gradient node with the current shell and choose by $|\Omega_c|J_c^+$. The paper implementation must be **on the fly**; the S1 tree of precomputed first-order-derived $J$ caps is a quality oracle and cannot be counted as shared storage.

### 2.3 `J-PROP`

The same path sampler using a surface-specific scalar-$J$ cap hierarchy. It is the strongest like-for-like static hierarchy baseline.

### 2.4 `DENSE-CELL`

Choose a cell proportional to its independently integrated true area, then sample uniformly within the clipped cell. Its algorithmic PDF is exact, while its distribution is an oracle-quality piecewise-constant approximation to uniform area. It stores one surface-specific cell weight plus a CDF/alias structure.

### 2.5 `AREA-EXACT` oracle

Exactly uniform surface-area sampling is a zero-variance oracle for total area and a lower bound for other area-domain integrands. It is not a claimed runtime method. The S2 repeat sampler realizes it when supplied with the independently computed total area.

`FO-NULL` and `J-NULL` are reported for acceptance/work, but their fixed-attempt estimators are not substituted for the non-null proposal comparison.

## 3. Deterministic variance calculation

For any proposal whose $p_{uv}$ is constant within clipped leaf $L$, the one-sample estimator for

$$
I_f=\int f(u,v)J(u,v)\,du\,dv
$$

is $X=fJ/p_{uv}$. Its exact second moment is

$$
\mathbb E[X^2]
=\sum_L\frac{1}{p_{uv,L}}
\int_{\Omega_L} f(u,v)^2J(u,v)^2\,du\,dv,
$$

so

$$
\operatorname{Var}[X]=\mathbb E[X^2]-I_f^2.
$$

On each reconstruction piece, $J^2$ is quartic. For $f\in\{1,u,v\}$, the second-moment integrand is polynomial of degree at most six and is integrated deterministically with a sufficiently high-order triangle rule. Cross-check selected cases at a higher order.

This removes Monte Carlo noise from the quality comparison and evaluates the exact algorithmic PDFs already audited in S2.

## 4. Corpus and cohorts

Use all 48 signed A2 windows crossed with all three shells (`144` surfaces).

Report:

- all 144 surfaces;
- ordinary shells (`S0-affine`, `S1-moderate`, 96 surfaces);
- stress shell (`S2-stress`, 48 surfaces);
- asset families and window IDs individually;
- a predeclared **nontrivial area-variation cohort** where `UV-UNIFORM` relative variance for the total-area integrand exceeds `1e-4`.

The nontrivial threshold is frozen before execution. All-surface results remain visible; the cohort only prevents numerically meaningless ratios when both methods have nearly zero variance.

## 5. Quality metrics

For $f=1,u,v$ and every method:

- integral oracle;
- variance and relative variance $\operatorname{Var}[X]/I_f^2$;
- variance relative to `UV-UNIFORM`, `J-PROP`, and `DENSE-CELL`;
- zero/near-zero handling with absolute variance shown;
- leaf/path probability extrema;
- `FO-NULL` and `J-NULL` acceptance.

Primary quality metric: total-area relative variance. The $u,v$ moments prevent an area-only optimizer from looking good through a single scalar coincidence.

## 6. Cost and storage accounting

### 6.1 Per sample

Record exact logical work:

- hierarchy decisions;
- child nodes inspected;
- first-order cap compositions;
- scalar cap loads;
- exact pointwise $J$ evaluations;
- random numbers and CDF decisions;
- bytes fetched under FP32 layouts.

`FO-PROP` at a 64-by-64 grid makes six decisions and composes up to four child caps per level. `J-PROP` makes the same decisions but loads scalar caps. `UV-UNIFORM` makes none. `DENSE-CELL` performs an alias/CDF choice.

### 6.2 Storage

Report at least these logical layouts per texture node:

- shared Taylor metric view: 6 FP32 values;
- ray min/max view if retained for A2: 2 FP32 values;
- specialized scalar cap: 1 FP32 upper cap, or 2 FP32 values if localized lower area bounds are required;
- per-instance/path CDF or alias weights separately.

For $K$ shell instances sharing one displacement map, plot total bytes for:

1. ray min/max plus shared Taylor, with on-the-fly area composition;
2. ray min/max plus one scalar-$J$ hierarchy per instance;
3. dense per-cell area CDF per instance.

Do not count a cached per-instance first-order-derived $J$ tree as shared.

### 6.3 Updates

Classify invalidation, then measure reference build work separately:

- displacement edit: Taylor and all surface-specific distributions affected;
- shell/direction/amplitude/instance-transform edit: Taylor texture statistics reusable, scalar-$J$ and dense area distributions rebuilt;
- emission edit: deferred to S4;
- local edits: count affected leaves and ancestors.

Python interval-polynomial build time is correctness scaffolding and excluded from the speed gate. It may illustrate invalidation only.

## 7. Predictive cost scenarios

Because Python cannot predict GPU interval-composition cost, report a break-even family. Let one scalar child-cap load cost one unit and one first-order child composition cost $c\in\{1,2,4,8\}$ units. Combine exact work counts with deterministic variance:

$$
\text{quality-cost}_m(c)=\operatorname{Var}_m[X]\,C_m(c).
$$

Plot the FO/J and FO/UV ratios across $c$. This identifies the maximum affordable composition cost. It is a GPU-port decision aid, not measured performance.

## 8. Frozen S3 quality/reuse gate

S3 authorizes S4 product sampling and the later GPU port only if:

1. on the nontrivial ordinary cohort, `FO-PROP` reduces geometric-mean total-area variance by at least `10%` relative to `UV-UNIFORM` and wins on at least two thirds of surfaces;
2. `FO-PROP` total-area variance is within `1.5x` of `J-PROP` in geometric mean and p90;
3. the same conclusions are not reversed catastrophically for the $u,v$ moment integrands;
4. S1 correctness and S2 PDF checks remain unchanged;
5. at least one reuse crossover is plausible before 16 shared instances: shared on-the-fly storage beats per-instance scalar-$J$ storage, or shell-edit rebuild work falls by at least `2x` under the measured logical model;
6. the predictive quality-cost curve has FO break-even at composition cost $c\ge2$ against UV on the nontrivial ordinary cohort.

Failure of the variance gate stops the area-performance claim even if bounds are conservative. Passing it authorizes product sampling because $EJ$ can create stronger variation and a more renderer-relevant target; it still does not authorize a GPU speed claim until measured S5 results.

## 9. External baselines and when they enter

- Ling et al. ray-cast uniform sampling: primary paper/protocol re-audit and implementation in S5, because its cost depends on an all-intersections GPU ray path.
- Pre-tessellated triangle-area sampling: S5 with subdivision/error sweep against the S0 surface.
- Light-BVH/many-light structures: S4/S5 after the emission product is fixed.

They remain required paper baselines. S3 does not replace them with a CPU estimate.

## 10. Ordered implementation

1. Implement per-cell integrals of $J^2$, $u^2J^2$, and $v^2J^2$ with dual-order tests.
2. Reconstruct per-cell $p_{uv}$ for all four non-null proposals and verify mass.
3. Compute deterministic variances on the three development surfaces and compare with S2 empirical variances.
4. Freeze the full-run schema, cohort classification, cost constants, and memory formulas.
5. Execute the resumable 144-surface quality run.
6. Produce variance, quality-cost, and instance-memory crossover plots.
7. Sign the S3 decision and plan S4 before implementing emission fields.

## 11. Immediate implementation boundary

Items 1–3 pass 28 combined tests and development run `area-s3-dev-f94aa877d085` agrees with S2 empirical single-sample variances within the predeclared factor-two audit. The result rejects the current local-child proposal on all three development surfaces: `FO-PROP/UV-UNIFORM` total-area variance is `30.30x` on rock, `2.15x` on brick, and `3.82x` on leather. `FO-PROP/J-PROP` is `4.11x`, `0.887x`, and `6.33x`.

The cause is structural: local child weight $|\Omega_c|J_c^+$ is not the sum of descendant leaf masses. Its normalization error compounds down the path. Tight leaf caps therefore do not imply a good local hierarchical proposal. `J-PROP` also loses to UV on two development cases, confirming that this failure is not solely first-order looseness.

Before the full decision, authorize exactly one repair ablation:

- define leaf mass $m_L=|\Omega_L|J_L^+$;
- store/fold subtree mass $M_n=\sum_{L\subset n}m_L$;
- descend with $P(c\mid n)=M_c/M_n$;
- call the methods `FO-LEAFCDF` and `J-LEAFCDF`;
- charge one surface/instance-specific scalar subtree weight per logical node to both methods;
- do not count `FO-LEAFCDF` as an on-the-fly shared sampler.

This repair telescopes to leaf probability $m_L/\sum_Km_K$ and should isolate the best quality available from S1 leaf caps. Implement and test it, then include both the failed local proposal and repaired leaf-CDF proposal in the frozen 144-surface run. The original on-the-fly FO gate remains failed if the repair alone succeeds. Product sampling stays blocked until this distinction is signed.

### 11.1 Repair checkpoint and frozen full-run interpretation

Repair run `area-s3-dev-38862f7c52a3` passes the deterministic checks. On the same three development surfaces, `FO-LEAFCDF/UV-UNIFORM` total-area variance is `0.095x`, `0.016x`, and `0.569x`; therefore the repair is worth evaluating on all 144 surfaces. It is nevertheless `1.23x`, `1.63x`, and `1.79x` the variance of `J-LEAFCDF`, and stores `2794` nonempty subtree weights (`11176` FP32 bytes) per surface instance at the current 64-by-64 triangular footprint.

Freeze these additional reporting rules before the full run:

1. keep the original six-part `FO-PROP` gate unchanged and report its failure independently;
2. on the predeclared nontrivial ordinary cohort, report `FO-LEAFCDF` geometric-mean and p90 variance ratios versus `UV-UNIFORM`, `J-LEAFCDF`, and `DENSE-CELL`, plus win fractions;
3. call repair quality competitive only if `FO-LEAFCDF/J-LEAFCDF` is at most `1.5x` in geometric mean and `2x` at p90;
4. multiply deterministic variance by the frozen logical per-sample work model, so low variance alone cannot imply practical advantage;
5. count one FP32 subtree mass per nonempty node and instance for both leaf-CDF methods; the shared Taylor data are additional for FO, not a substitute for those weights;
6. plot 1--16-instance storage for on-the-fly FO, `FO-LEAFCDF`, `J-LEAFCDF`, and dense-cell sampling;
7. do not authorize S4 solely because the repair beats UV. It must leave a coherent reuse, update, memory, or multi-query reason to prefer the first-order representation over the specialized scalar hierarchy.

## 12. Signed full-corpus decision — 2026-08-25

Run `area-s3-full-f245aff67ea7` completes all 144 signed-A2 surfaces with valid probability mass for every method. The predeclared nontrivial ordinary cohort contains 69 surfaces.

The original shared/on-the-fly proposal fails decisively:

- `FO-PROP/UV-UNIFORM` total-area variance has geometric mean `16.342x` and wins on only `5.8%` of the cohort;
- `FO-PROP/J-PROP` has geometric mean `2.872x` and p90 `8.869x`;
- after charging the frozen logical work at first-order composition cost `c=2`, `FO-PROP/UV-UNIFORM` variance-work is `153.614x`.

The single authorized repair improves distribution quality but does not rescue the representation claim:

- `FO-LEAFCDF/UV-UNIFORM` variance has geometric mean `0.1377x` and wins on all 69 nontrivial ordinary surfaces;
- it is still `1.4729x` `J-LEAFCDF` in geometric mean and `2.1785x` at p90, so it fails the frozen specialized-comparator p90 gate;
- it is `2.4497x` `DENSE-CELL` variance, with zero wins and a best case of `1.1984x`;
- its variance-work ratio is `0.9636x` versus UV at the reference cost, but `1.4729x` versus `J-LEAFCDF` and `13.191x` versus dense-cell;
- it requires one surface-specific subtree weight per nonempty node. At one instance the combined-query logical model charges `185928` bytes to `FO-LEAFCDF`, versus `54864` for `J-LEAFCDF` and `60328` for dense-cell. The FO repair remains larger than J at every 1–16 instance count because the shared Taylor payload is additional.

Therefore both the original S3 gate and the repair comparator gate fail. The first-order hierarchy remains a correct way to derive tight leaf area caps, but it is not the practical area-sampling result required by the shared two-query paper. Do not proceed to S4 product sampling, S5 GPU timing, Ling ray-cast sampling, or tessellated baseline timing under this hypothesis. Those stages were conditional on S3 and would now be post-hoc scope expansion.

Preserve as valid results: the exact surface differential/oracle, conservative first-order area-cap construction, probability semantics, and the ablation showing why local cap decisions do not form a good global proposal. Do not claim a first-order sampling performance, storage, or update advantage.

Evidence: `experiments/area_sampling_s3/area-s3-full-f245aff67ea7/summary.json` and `figures/area_sampling_s3/area_s3_full_decision.png`.

Related: [[Plan — S2 area sampling and PDF audit]] · [[Plan — S1 conservative area bounds]] · [[Plan — Area and product sampling application]]
