---
title: Note — EG draft handoff after topology T0
tags: [paper, eurographics, handoff, ray-tracing, shell-space, DDA]
status: current
created: 2026-08-30
updated: 2026-08-30
---

# EG draft handoff after topology T0

## What can be written now

We formulate a topology-preserving shell-space segmentation problem. For a monotone rational shell ray $q(h)=(U(h)/D(h),V(h)/D(h),h)$ and a nested grid level, an accepted endpoint chord must reproduce the exact curve's ordered vertical/horizontal boundary-event labels, corner grouping, persistent grid-line ownership, closed cell set, and open-cell order. The segmenter first partitions at denominator poles and $u$, $v$, and world-ray-$t$ derivative roots, then splits remaining intervals at exact event-order disagreements. Error certification is consumed while choosing knots; an accepted traversal segment carries no UV tube radius.

The CPU T0 reference validates this discrete construction on 864 practical shell rays and 54 analytic case/resolution pairs. Every accepted segment passes exact-event, closed-cell, open-cell-order, derivative-sign, event-height-bracket, and reverse-traversal checks. Practical segment counts are p50/p95/p99/max `1/2/3/3`, and accepted crossing segments visit p50/p95/max `5/22/39` cells. This is evidence that the rational curve can often be replaced by a compact ordered chord sequence for grid traversal.

## What must not be written

Do not state that the method is faster than nonlinear ray tracing, TFDM, RMIP, PDM, dense triangles, or DMM. T0 is a CPU correctness/mechanism oracle, not a runtime implementation. It deliberately solves exact rational grid events and records `6587` such solves over the practical corpus; a cheap runtime certificate has not been established.

Do not describe removal of the UV tube as a large candidate-work win. Only 149 practical accepted segments have tube-expanded coverage. On that subset, topology-only traversal removes p50/mean/p95 `14.29%/14.65%/28.05%` of cells, failing the predeclared `25%` median continuation gate. Production CUDA integration is stopped under the current plan.

## Suggested concise result paragraph

> We evaluated whether the nonlinear shell ray can be converted into ordinary DDA segments without propagating a conservative tube through the hierarchy. Our reference partitions the rational curve into monotone intervals and introduces a knot only when the endpoint chord changes the ordered grid-boundary event stream. Across 864 practical intervals, the resulting decomposition uses at most three chords and exactly preserves event order and closed cell ownership. The representation is compact, but its candidate-set advantage is limited: on segments for which a certified tube expands cell coverage, topology-only traversal removes 14.3% of cells at the median, below our predeclared continuation threshold. We therefore treat this as a feasibility and correctness result rather than a performance claim.

## Authoritative artifacts

- [[Plan — Topology-preserving shell-ray DDA]]
- `experiments/ray_topology_t0/ray-topology-t0-d891da21b944/result.json`
- `experiments/ray_topology_t0/ray-topology-t0-d891da21b944/summary.json`
- `experiments/ray_topology_t0/ray-topology-t0-d891da21b944/traces.jsonl.gz`
- `scripts/ray_topology_t0.py`
- `scripts/test_ray_topology_t0.py`
