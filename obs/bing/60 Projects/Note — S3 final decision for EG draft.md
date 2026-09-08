---
title: Note — S3 final decision for EG draft
tags: [paper-handoff, area-sampling, negative-result, evidence]
created: 2026-08-25
updated: 2026-08-25
---

# S3 final decision for the EG draft

## Bottom line

The current **shared first-order performance thesis does not pass**. This is now supported by full-corpus evidence, not a prediction. Keep the exact/correctness material, but do not claim that the first-order hierarchy accelerates ray traversal or is a practically superior area sampler.

## Signed evidence

Run: `experiments/area_sampling_s3/area-s3-full-f245aff67ea7`

Figure: `figures/area_sampling_s3/area_s3_full_decision.png`

Corpus: all 48 practical windows crossed with three shells, 144 surfaces total. The frozen nontrivial ordinary cohort has 69 surfaces.

### Original shared proposal

`FO-PROP` composes first-order caps on the fly and chooses locally among children. Its total-area variance relative to UV-uniform has geometric mean `16.342x`; it wins on `5.8%` of the nontrivial ordinary cohort. Relative to the analogous scalar `J-PROP`, it is `2.872x` in geometric mean and `8.869x` at p90. The original S3 gate fails.

The mechanism is important: a local child weight $|\Omega_c|J_c^+$ is not the total proposal mass of its descendant leaves. Repeated local normalizations therefore distort the final leaf distribution even when each leaf cap is tight.

### One authorized repair

`FO-LEAFCDF` defines leaf masses $m_L=|\Omega_L|J_L^+$ and stores subtree sums. It is a good proposal relative to UV: geometric-mean variance ratio `0.1377x`, with a win on all 69 surfaces. But it is no longer an on-the-fly shared sampler. It stores one scalar subtree mass per nonempty node and surface instance.

Against the proper specialized comparators, it has `1.4729x` the `J-LEAFCDF` variance in geometric mean and `2.1785x` at p90, failing the frozen p90 threshold. It has `2.4497x` the dense-cell variance and wins on zero surfaces. At one instance, combined-query logical storage is `185928` B for FO leaf-CDF, `54864` B for J leaf-CDF, and `60328` B for dense-cell; FO stays larger over 1–16 instances.

## What remains publishable material

- exact analytic surface differential and clipped-domain area oracle;
- conservative first-order area-cap derivation and certification;
- correct non-null/null/repeat sampling semantics and PDF proofs;
- the distinction between conservative bound tightness and proposal quality;
- the negative local-normalization ablation and the storage-charged repair;
- certified tessellation-free nonlinear ray intersection correctness and its mixed A2 regime results.

These are real technical results, but together they do not currently establish the positive performance contribution expected of the proposed EG full paper.

## Work intentionally not started

Product sampling, closest point, the GPU area sampler, Ling ray-cast area sampling, and tessellated area baselines were conditional on S3. They are stopped by the predeclared gate. Starting them now as rescue applications would weaken the experimental story and violate the plan.

## Recommended draft posture

Treat the first-order ray and area performance results as honest negative ablations. If continuing toward EG, narrow the active research question to a materially improved certified ray architecture or develop a new representation with an advantage that survives its specialized comparator. Do not finalize the abstract or contribution bullets around the shared hierarchy in its current form.
