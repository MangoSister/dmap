---
title: Plan — Topology-preserving shell-ray DDA
tags: [plan, ray-tracing, displacement-mapping, shell-space, segmentation, DDA, RMIP]
status: t0-stopped-predictive-gate
created: 2026-08-30
updated: 2026-08-30
parent: "[[Project — Conservative first-order queries on displacement maps]]"
predecessor: "[[Plan — Certified ray architecture comparison]]"
---

# Plan — Topology-preserving shell-ray DDA

> [!abstract] Purpose
> The completed tube-supercover implementation showed that a conservative UV tube can erase the arithmetic advantage of chord traversal. This plan tests a new, separately gated hypothesis: **consume the curve-error proof during segmentation, then traverse an ordinary zero-radius chord whose ordered mip-cell sequence is certified to equal that of the exact rational shell ray.** Traversal stays in shell space; it does not construct TFDM world boxes or perform RMIP-style 2D↔3D interval inversion.

## 1. Hypothesis and contribution boundary

Inside one valid triangle shell, the exact ray is

$$
q(h)=\left(\frac{U(h)}{D(h)},\frac{V(h)}{D(h)},h\right),
$$

with degree-at-most-two $U,V,D$. For an interval $I=[h_0,h_1]$, let $\ell_I$ be the UV chord through the exact endpoints. At a nested grid level $L$, define $\mathcal E_L(q,I)$ as the ordered vertical/horizontal grid-boundary events of the exact curve, with simultaneous corner events grouped, and $\mathcal E_L(\ell,I)$ as the corresponding chord-DDA events.

The target certificate is the discrete invariant

$$
\mathcal E_L(q,I)=\mathcal E_L(\ell,I),
$$

including event order and closed boundary ownership. An accepted segment stores no UV radius. Its implicit error descriptor is only the finest certified grid level. If descent requests a finer level, the interval is split or recertified lazily.

This does **not** claim that error bounds disappear. They are consumed by the segmentation decision instead of being propagated as a tube through every node. Finite-precision root/event brackets and deterministic edge/corner ownership remain mandatory.

The novelty candidate, if performance survives, is:

> Hierarchy-coupled, topology-preserving segmentation that converts an exact rational shell ray into ordinary ordered DDA segments over a standard scalar min/max mipmap, without traversal-time world-space bounds, point inversion, or UV-tube neighborhood scans.

Turning-point splitting, monotone projected-curve bounds, front-to-back traversal, and texel marching individually have prior art, notably RMIP. They are not standalone novelty claims.

## 2. Relationship to RMIP and Ogaki/NRT-QT

RMIP splits the projected ray at zero-$u/v$ derivative events, bounds monotone pieces by endpoint rectangles, ping-pongs between texture-space rectangles and world-space ray intervals, and switches to exact projected-curve texel marching. Its rectangular min/max structure answers anisotropic range queries.

The proposed path borrows the monotone-event and ordered-marching insights but differs structurally:

- after the initial prism interval, all traversal state is $(u,v,h,t)$ shell state;
- no world-space surface box is built per traversal interval;
- no point-wise displacement inversion is performed during traversal;
- the existing standard min/max mipmap is retained initially;
- the chord predicts a DDA event stream, while the shell curve certifies its topology and supplies exact/bracketed event heights; and
- exact nonlinear microtriangle intersection remains the leaf contract.

Against NRT-QT, success requires amortizing segmentation across multiple hierarchy/grid events. If the method solves essentially the same rational boundary equations per node while adding segmentation logic, it has merely reimplemented the baseline and fails the performance hypothesis.

## 3. Required segment invariants

Every accepted interval must have:

1. **regular denominator:** $D(h)$ has a fixed nonzero sign;
2. **valid support:** the curve remains inside the proxy and requested world-ray interval;
3. **UV monotonicity:** $u'(h)$ and $v'(h)$ have fixed signs, allowing zero only at closed interval endpoints;
4. **ray-order monotonicity:** $t'(h)$ has a fixed sign, so segment and node work can be ordered front-to-back;
5. **topological equivalence:** exact and chord grid events have identical labels, grouping, and order through the certified level; and
6. **height-event validity:** every visited cell/node receives an exact or outward-bracketed curve height interval, not the chord's unguarded crossing height.

UV monotonicity plus identical exact endpoints guarantees that curve and chord cross the same set of vertical and horizontal grid lines. The remaining topological uncertainty is their cross-axis interleaving and corner grouping. That is the narrow predicate the segmentation stage must certify.

## 4. T0 reference oracle — implement before GPU changes

### 4.1 Frozen inputs

Reuse the 864 practical rays and 48 native displacement windows from `p-d1-2-ec0dd6a036f5`, the three audited shell families, and the exact rational coefficient construction. Add direct analytic cases for:

- no turn, one $u$ turn, one $v$ turn, and a $t$ turn;
- denominator-near-zero but separated, and a true pole/refusal;
- exact grid-line and grid-corner travel;
- two crossing events whose order is arbitrarily close;
- reversed ray/height orientation; and
- a high-curvature S-shaped projection.

Practical windows use every nested level through their 16×16 leaf grid. Analytic cases additionally sweep 8, 16, 32, 64, 128, and 256 cells per axis.

### 4.2 Authoritative event stream

For every valid shell interval:

1. isolate denominator, proxy, ray-range, $u'$, $v'$, and $t'$ roots with outward brackets;
2. form regular monotone subintervals;
3. enumerate every rational crossing of $u=k/2^L$ and $v=k/2^L$;
4. sort crossings in front-to-back order and group genuine corner/tie events;
5. record exact/outward height brackets for every event; and
6. derive the closed ordered cell stream independently from midpoint samples between events plus closed event ownership.

Dense sampling is diagnostic only and is never the authoritative topology oracle.

### 4.3 Candidate segmentation

For each monotone interval, compare the exact event stream with ordinary chord-DDA events at the requested finest level. If they match, accept one macro segment. If they differ:

- first split at the earliest exact event participating in the ordering/grouping disagreement when it is strictly interior;
- otherwise split at a representable height midpoint; and
- recurse without a silent depth or segment cap.

The reference persists why each knot was introduced: `denominator`, `proxy/range`, `u-turn`, `v-turn`, `t-turn`, `event-order`, `corner-group`, or `precision-refusal`.

### 4.4 T0 metrics

Persist per interval/ray and pooled p50/p95/p99/max for:

- initial regular intervals and final macro segments;
- turn/event/order/corner split counts;
- chord events and exact rational event solves;
- cells per macro segment;
- finest certified level;
- exact/chord ordered-event mismatches before and after splitting;
- closed-cell omissions/extras;
- front-to-back orientation failures;
- maximum recursion depth; and
- tube-supercover cells versus topology-certified cells on the same intervals.

### 4.5 T0 gates

Correctness gates, all mandatory:

- zero denominator/pole interval accepted;
- zero interior sign reversal for $u'$, $v'$, or $t'$ in accepted segments;
- zero ordered event-label/group mismatch after segmentation;
- zero exact closed-cell omission;
- zero invalid or unordered height-event bracket; and
- reverse traversal produces the reversed event/cell stream modulo the frozen boundary-owner rule.

Predictive gates for continuing to a GPU primitive:

- practical p95 final macro segments per valid interval $\le 4$ and maximum $\le 16$;
- practical median cells per accepted macro segment $\ge 2$ on intervals crossing at least two cells;
- topology-certified closed cells are at least 25% fewer than tube-supercover cells on the subset where the tube adds cells; and
- no more than 5% of ordinary practical intervals require a precision refusal or exact-event-per-cell fallback.

A correctness failure blocks optimization. A predictive failure stops this architecture before production CUDA changes; it is not repaired by weakening ownership or dropping difficult rays.

## 5. T1 runtime design gate — only after T0 passes

Before editing the production kernel, specify and count two runtime variants over persisted T0 streams:

1. **Predict/certify:** the chord predicts the whole event sequence; derivative/error intervals certify ordering, with exact roots only for ambiguous neighboring events.
2. **Predict/correct:** the chord predicts the next boundary; monotonicity proves the selected label; one rational solve/refinement returns its exact height.

Compare estimated and measured primitive counts against NRT-QT's shared six-plane solve per four-child expansion. Continue only if at least one variant reduces rational solves and total modeled instruction cost on the held-out practical rays.

## 6. T2 minimal GPU integration — deferred

If T0 and T1 pass, add a new intersection mode rather than replacing Mode 0 or the tube path. The first GPU version includes only:

- regular/turn partitioning;
- topology-certified macro segments;
- ordered zero-radius DDA;
- exact/outward event-height intervals for min/max rejection;
- four-child ordering or a monotone event frontier using current closest $t$;
- exact nonlinear leaf tests; and
- local ambiguity recovery, followed by counted NRT-QT fallback only for unsupported shells.

It must remove the current tube-neighbor root scan and per-child UV-radius clipping. Primary closest, secondary closest, and visibility paths receive separate counters because visibility may terminate on the first exact hit without closest-order proof.

## 7. Reporting boundary

T0 establishes only mathematical/discrete feasibility and mechanism counts. T1 is a predictive cost gate. Only T2 renderer timing can support a speed claim. No result from this plan alone authorizes superiority over RMIP, TFDM, PDM, dense triangles, or DMM.

## 8. Decision log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-30 | Start a separate topology-preserving segmentation plan. | The conservative tube path remained several-fold slower even with zero-radius diagnostics; per-node tube/control cost dominates. |
| 2026-08-30 | Keep standard scalar min/max and shell-space traversal initially. | This isolates the ray segmentation/traversal hypothesis from RMIP's rectangular hierarchy and 2D↔3D inversion. |
| 2026-08-30 | Require monotone $u,v,t$ and event-order equivalence. | A straight chord alone does not prove the exact curve has the same cell sequence or front-to-back order. |
| 2026-08-30 | Consume error bounds during segmentation rather than traversal. | The target fast path carries a discrete certified level/event stream, not a UV tube radius. |
| 2026-08-30 | Pass T0 correctness but stop before T1/T2. | The full corpus needs few topology-preserving segments and has exact ordered ownership, but median candidate-cell reduction on tube-expanded segments is `14.29%`, below the frozen `25%` continuation gate. |

## 9. T0 result — 2026-08-30

The authoritative run is `experiments/ray_topology_t0/ray-topology-t0-d891da21b944`. It evaluates all 864 practical P-D1 rays plus 54 analytic case/resolution pairs. The implementation and its seven regression tests are:

- `scripts/ray_topology_t0.py`;
- `scripts/run_ray_topology_t0.py`; and
- `scripts/test_ray_topology_t0.py`.

### 9.1 Correctness result

All mandatory topology checks pass:

- zero accepted non-pole refusal and zero practical refusal;
- zero accepted event-label/group mismatch;
- zero closed-cell or ordered open-cell mismatch between the rational curve and chord;
- zero sampled derivative-sign or event-bracket failure; and
- reverse traversal reproduces the reversed event and cell stream under the frozen closed-boundary rule.

The six recorded refusals are the intentionally constructed analytic denominator-pole case at six resolutions. Independent reverse reparameterization initially exposed five events displaced roughly `3e-12` from an owned endpoint by coefficient composition. They are counted as `reverse_endpoint_duplicate_count = 5` and removed only when both the event is within the frozen reverse-endpoint tolerance and its label is owned by that exact endpoint. UV monotonicity makes a genuine same-boundary recrossing impossible inside such an interval.

### 9.2 Practical mechanism result

| quantity | result |
|---|---:|
| practical intervals | `864` |
| accepted practical macro segments | `1012` |
| segments/interval p50 / p95 / p99 / max | `1 / 2 / 3 / 3` |
| crossing cells/segment p50 / p95 / max | `5 / 22 / 39` |
| practical pre-split topology mismatches | `148` |
| practical topology splits | `148`, all `event-order` |
| practical oracle rational root solves | `6587` |
| practical turn partitions | `0` on this corpus |
| tube-expanded practical segments | `149` |
| candidate-cell reduction on that subset, p50 / mean / p95 | `14.29% / 14.65% / 28.05%` |

The segment-count hypothesis is supported. Every affine-shell interval accepts one chord. Moderate shells have p95 `2` and maximum `2`; stress shells have p95 `2` and maximum `3`. Accepted segments cross enough cells to offer amortization: the practical median is five cells. The practical corpus does not exercise an interior $u$, $v$, or $t$ turn, so those paths remain analytic correctness coverage rather than a performance distribution.

The cell-reduction hypothesis is not supported strongly enough. The topology path removes some tube neighbors, especially for stress, oblique, near-miss, and proxy-edge cases, but the tube is already cell-tight on most segments. Where it adds cells, the frozen median reduction target is missed by more than ten percentage points.

### 9.3 Frozen decision

T0 is a positive feasibility/correctness result and a negative continuation result. It establishes that a rational shell ray can be represented by a very small number of ordinary chords whose ordered finest-grid topology is exactly preserved on this corpus. It does **not** establish a cheaper runtime certificate, a faster hierarchy traversal, or superiority to NRT-QT, TFDM, RMIP, PDM, dense triangles, or DMM.

Because the `25%` candidate-cell gate was frozen before the full run and failed, T1 cost-model design and T2 CUDA integration are not authorized under this plan. Removing tube intersection arithmetic could still reduce constants, but that is a different predictive hypothesis; prior zero-radius diagnostics already showed that zero radius alone did not repair the existing DDA control cost. Reopening GPU work therefore requires a newly written mechanism and upper-bound gate, not a reinterpretation of this run.

Safe paper language:

> We show that monotone rational triangle-shell rays admit a compact topology-preserving chord decomposition: on 864 practical intervals, the decomposition uses at most three chords and exactly preserves ordered grid events and closed cell ownership. However, relative to a certified chord tube, it removes only `14.3%` of candidate cells at the median on intervals where the tube expands coverage; we therefore do not claim a ray-tracing speed advantage from this representation.

Draft handoff: [[Note — EG draft handoff after topology T0]].
