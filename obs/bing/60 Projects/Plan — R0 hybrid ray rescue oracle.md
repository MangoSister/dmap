---
title: Plan — R0 hybrid ray rescue oracle
tags: [plan, ray-tracing, hybrid, DDA, nonlinear-ray, upper-bound, stop-rule]
status: complete-gate-failed
created: 2026-08-25
updated: 2026-08-25
parent: "[[Plan — Certified ray architecture comparison]]"
predecessor: "[[Report — A2 same-surface GPU comparison]]"
---

# Plan — R0 hybrid ray rescue oracle

> [!abstract] Decision
> Determine whether a geometry-aware dispatcher between certified tube-supercover DDA and the same-surface nonlinear quadtree has enough *optimistic* headroom to justify any further GPU implementation. R0 uses the immutable A2 raw trials and assumes perfect family recognition, homogeneous method queues, and zero dispatch cost. If even this upper bound misses the frozen gate, stop the hybrid hypothesis before constructing a classifier or new corpus.

## 1. Motivation and contribution boundary

A2 establishes exact same-surface correctness but mixed performance. `CERT-DDA-MM` is `0.383x` `NRT-QT` for decision front rays, yet loses on oblique (`1.279x`), grazing (`1.624x`), and near-miss (`1.049x`) rays. A hybrid can be coherent because the methods share coefficients, surface, hierarchy, and exact leaf; it is not automatically novel. Its paper value would have to come from a cheap geometric decision that preserves the front-facing win without paying the adverse-regime cost.

Do not tune a rule on the observed decision split and call it held out. R0 is explicitly an exploratory upper bound. A confirmatory claim would require a new immutable corpus frozen before hybrid timing.

## 2. Immutable inputs

Use only:

- `experiments/ray_architecture_a2/ray-a2-4-timing-d9125676ceb0/*.raw.json`;
- its `summary.json`, `configuration.json`, and `split_manifest.json`;
- the four ordinary decision families: `front-hit`, `oblique-hit`, `grazing-hit`, and `near-miss`.

Verify source hashes and require equal candidate/baseline checksums. Do not rerun, reorder, or discard A2 timing samples.

## 3. Zero-cost family oracle

For family $f$, let $T_{C,f}$ and $T_{N,f}$ be candidate and NRT time per ray and let $w_f$ be its true count in the decision manifest. Freeze the selected method using the raw-trial median:

$$
m_f=\arg\min_{m\in\{C,N\}}\operatorname{median}(T_{m,f}).
$$

The optimistic homogeneous-queue hybrid time is

$$
T_H^{(0)}=\frac{\sum_f w_f T_{m_f,f}}{\sum_f w_f},
$$

and its NRT ratio is

$$
R_H^{(0)}=\frac{\sum_f w_f T_{m_f,f}}
                 {\sum_f w_f T_{N,f}}.
$$

This is more favorable than a real dispatcher: it charges no feature evaluation, branch divergence, queue compaction, extra register/code footprint, or loss of coherence. It is therefore a necessary upper-bound gate, not a performance prediction.

Bootstrap paired trial indices within each family after freezing $m_f$. Persist the median and 95% interval for $R_H^{(0)}$ and absolute ns/ray savings.

## 4. Workload-mixture sensitivity

Let $p$ be the front-ray fraction and distribute $1-p$ equally across the other three ordinary families. Plot the zero-cost ratio for $p\in[0,1]`, plus `2` and `5` ns/ray hypothetical dispatch overhead curves. Report the smallest $p$ that reaches `0.90x` for each overhead.

This plot answers whether a result would depend on an unusually front-heavy workload. It must not be used to replace the balanced frozen gate after observing the curve.

## 5. Frozen R0 go/no-go rule

Further hybrid work is authorized only if all conditions hold on the manifest-weighted decision ordinary families:

1. zero-cost oracle median $R_H^{(0)}\le0.90$;
2. bootstrap p95 $R_H^{(0)}\le0.95$;
3. median absolute saving is at least `8 ns/ray`;
4. the candidate is selected for at least two of the four ordinary families, so the hypothesis is broader than a single named fast path;
5. all source checksums and raw sample counts validate.

Failure of any condition stops the hybrid implementation, new confirmation-corpus generation, OptiX integration, and external baseline timing. Do not weaken this gate after seeing R0.

## 6. Conditional stages if R0 passes

### R1 — feature-realizable rule

Use only pre-traversal quantities shared by both methods: supported shell-height length, denominator conditioning, chord UV length in texels, certified tube radius in texels, proxy footprint, and a shell-incidence surrogate. Fit a depth-two decision tree or at most three scalar thresholds on the old A2 development records. Charge feature cost. No per-ray measured method time may be read at runtime.

### R2 — new immutable confirmation corpus

Before hybrid timing, freeze new texture windows, shells, continuous incidence angles, primary and secondary ray packets, ownership stress rays, and a content hash. The old A2 decision records remain training/exploratory data and cannot be reused as confirmation.

### R3 — confirmatory GPU comparison

Compare `ARCH-HYB`, `NRT-QT`, and `CERT-DDA-MM` with dispatch overhead, correctness, warmups, randomized trial order, and identical buffers. Require hybrid/NRT `<=0.90x` pooled ordinary, no ordinary family above `1.05x`, no asset group above `1.10x`, and exact oracle equality.

### R4 — external baselines

Only an R3 pass authorizes OptiX integration and quality-matched TFDM/RMIP/PDM/triangles/DMM work. A hybrid that merely selects NRT for nearly everything is an engineering fallback, not the paper's technical contribution.

## 7. R0 outputs

- content-addressed configuration and result JSON;
- selected method and timing distribution per family;
- bootstrap oracle ratio and absolute savings;
- front-fraction/dispatch-overhead sensitivity plot in PDF and PNG;
- explicit automatic pass/fail and stopped/authorized next action;
- project and draft-guide checkpoint.

## 8. Ordered implementation

1. Implement raw-artifact validation and manifest family counts.
2. Implement deterministic median selection and paired bootstrap.
3. Test synthetic exact mixtures, checksum failures, and front-fraction thresholds.
4. Run once on the immutable A2 artifact.
5. Sign the R0 decision before touching CUDA or generating a confirmation corpus.

Related: [[Report — A2 same-surface GPU comparison]] · [[Plan — Certified ray architecture comparison]] · [[Project — Conservative first-order queries on displacement maps]]

## 9. Signed R0 result — 2026-08-25

Run `ray-r0-hybrid-oracle-3dd5b9a1a015` validates all immutable inputs and executes 100,000 paired bootstrap mixtures. The zero-cost family oracle selects `CERT-DDA-MM` only for `front-hit` and selects `NRT-QT` for oblique, grazing, and near-miss rays.

The manifest-balanced result is:

- point estimate `0.9165x` NRT and `4.825 ns/ray` saved;
- bootstrap median `0.9182x`, p95 `0.9251x`;
- bootstrap median absolute saving `4.738 ns/ray`;
- only one of four families selects the candidate.

Thus the ratio, absolute-saving, and family-breadth checks fail. This already assumes perfect classification, homogeneous queues, and zero overhead. The workload sensitivity also shows that a `0.90x` result requires at least `29.2%` front rays with zero dispatch cost, `37.9%` with `2 ns/ray`, and `51.0%` with `5 ns/ray`.

Stop the hybrid without implementing it. Do not generate a confirmation corpus, modify CUDA, integrate OptiX, or run external TFDM/RMIP/PDM/DMM timing under this hypothesis. The front-ray special case remains a documented regime result, not a broad paper contribution.

Evidence: `experiments/ray_architecture_r0/ray-r0-hybrid-oracle-3dd5b9a1a015/result.json` and `figures/ray_architecture_r0/ray_r0_hybrid_upper_bound.png`.
