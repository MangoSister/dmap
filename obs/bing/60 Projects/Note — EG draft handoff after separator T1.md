---
title: Note — EG draft handoff after separator T1
tags: [paper, eurographics, handoff, ray-tracing, shell-space, DDA, certificate]
status: current
created: 2026-08-30
updated: 2026-08-30
---

# EG draft handoff after separator T1

## Method material that is ready

For a regular monotone rational shell ray, the exact curve and its endpoint chord cross the same per-axis grid boundaries. Only the merge order of $u$ and $v$ events is unknown. For adjacent chord events $A$ and $B$, evaluate their rational boundary polynomials at a chord-height separator $s$. Fixed denominator and coordinate-derivative signs prove that exact $A$ lies before $s$ and exact $B$ lies after it. Applying these predicates to every adjacent pair also brackets event heights for conservative leaf min/max rejection. Failed predicates introduce chord-derived knots; exact grid-event roots are never used for acceptance.

The full CPU replay passes exact event, closed-cell, ordered-cell, height-bracket, reverse-order, filtered-sign, and required-min/max-candidate checks on 864 practical rays plus analytic stress cases. There are no practical fallbacks.

## Quantitative result

| metric | result |
|---|---:|
| candidate / NRT-QT structural min/max fetches | `6025 / 34055 = 0.1769×` |
| candidate / NRT-QT pooled leaf candidates | `2082 / 1948 = 1.0688×` |
| separator predicates / NRT node visits | `11561 / 34520 = 0.3349×` |
| practical segments p50 / p95 / p99 / max | `1 / 7 / 10 / 14` |
| practical fallback | `0%` |

The predeclared continuation gate fails because p95 segments exceed four, sign predicates exceed `0.25×` NRT visits, and the near-miss family retains `333/203 = 1.6404×` NRT leaf candidates. Grid-corner rays cause the segment tail; conservative separator-derived height brackets cause the near-miss leaf excess.

## Claim boundary

Safe: the separator-sign construction is root-free at grid events, exact under the tested topology contract, and exposes large structural fetch-count headroom.

Unsafe: broad faster traversal, faster rendering, superiority to NRT-QT, or comparison claims against TFDM, RMIP, PDM, dense triangles, or DMM. A subsequent optimistic GPU envelope exists, but it omits schedule construction and fails its held-out continuation gate; see [[Plan — T1.5 measured separator-DDA performance]].

Suggested result paragraph:

> We replace exact rational grid-event solves by filtered separator signs. The resulting CPU oracle preserves ordered DDA topology and all required min/max candidates across 864 practical rays, while issuing only 0.177× as many structural min/max fetches as the same-surface nonlinear quadtree. This opportunity does not yet translate into a performance claim: grid-corner cases raise p95 segmentation to seven chords, predicate work reaches 0.335× the baseline node count, and near-miss leaf candidates rise to 1.64×. These failures trigger our predeclared stop rule before GPU implementation.

## Authoritative artifacts

- [[Plan — T1 separator-sign shell-ray certificate]]
- `experiments/ray_separator_t1/ray-separator-t1-c8ca43b64c6c/result.json`
- `experiments/ray_separator_t1/ray-separator-t1-c8ca43b64c6c/summary.json`
- `experiments/ray_separator_t1/ray-separator-t1-c8ca43b64c6c/traces.jsonl.gz`
- `scripts/ray_separator_t1.py`
- `scripts/test_ray_separator_t1.py`

## Subsequent T1.5 update

Run `ray-t15-envelope-f619b4ad5265` measures the candidate after making T1 segmentation and certification free. It passes all checksum and stability checks but reaches only `0.9557×` NRT-QT on held-out ordinary rays, loses on S1-moderate (`1.0762×`) and oblique hits (`1.7789×`), and therefore stops before the full CUDA segmenter. S2-stress (`0.7845×`) and front hits (`0.3118×`) are favorable regimes only.

Revised paper-safe sentence:

> A GPU replay of the certified leaf schedule confirms that direct traversal is advantageous for stress-shell and front-facing regimes, but its optimistic held-out ordinary ratio is only `0.956×` NRT-QT before charging separator construction. We therefore retain the topology certificate as a correctness contribution and do not claim broad acceleration.
