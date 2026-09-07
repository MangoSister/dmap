---
title: Plan — T1 separator-sign shell-ray certificate
tags: [plan, ray-tracing, shell-space, DDA, certificate, NRT-QT]
status: t1-stopped-structural-gates
created: 2026-08-30
updated: 2026-08-30
parent: "[[Plan — Topology-preserving shell-ray DDA]]"
---

# Plan — T1 separator-sign shell-ray certificate

> [!abstract] Purpose
> T0 proved that practical rational shell rays admit a compact topology-preserving chord decomposition, but rejected the hypothesis that removing the UV tube wins mainly by removing candidate cells. T1 tests a distinct remaining mechanism: **can event order be certified with cheap polynomial sign predicates, allowing direct zero-radius leaf DDA to replace repeated nonlinear quadtree range work?** T1 is a CPU correctness and structural-cost replay. It does not time a GPU kernel or reopen external-baseline claims.

## 1. Comparator and contribution boundary

The authoritative same-surface comparator is the passing A2 run `ray-a2-3-nrt-qt-ef2448723ee6`. Its device implementation evaluates outward rational Bernstein ranges at visited scalar-min/max quadtree nodes; across the 864 rays it records `34,520` node visits, `34,055` min/max fetches, and `1,948` leaf candidates. This is the baseline used by the T1 structural gates.

The upstream renderer also contains the Ogaki-style shared-boundary implementation that solves three $u$ and three $v$ quadratic plane equations before testing four siblings. That is relevant prior-art context, but its six-plane operation count must not be substituted for the measured A2 comparator.

T1 holds fixed:

- the 48 A2 displacement windows and their 64×64 leaf grids;
- the 864 P-D1/A2 practical rays and three shell families;
- exact rational shell coefficients and valid proxy/world-ray intervals;
- the scalar min/max displacement data;
- closed edge/corner ownership; and
- the exact T0 event stream as a validation oracle only.

## 2. Separator-sign theorem used by the candidate

On a regular interval $I=[h_0,h_1]$, assume $u(h)=U(h)/D(h)$ and $v(h)=V(h)/D(h)$ are monotone and $D$ has fixed sign. Exact and chord curves share both endpoints, so they cross the same set of vertical and horizontal grid lines. Monotonicity fixes event order within each axis. Only the interleaving of a $u$ event and a $v$ event remains unknown.

Let chord event $A$ precede adjacent chord event $B$, and choose a separator $s$ strictly between their chord heights. For an event label $A=(x,k)$ define

$$
P_A(h)=N_x(h)-\frac{k}{R}D(h), \qquad x\in\{u,v\}.
$$

Let $\sigma_x\in\{-1,+1\}$ be the monotonic sign and $\sigma_D$ the denominator sign. The exact $A$ crossing is before $s$ precisely when

$$
\sigma_x\sigma_D P_A(s)>0,
$$

and exact $B$ is after $s$ when the corresponding expression is negative. Therefore, for every adjacent cross-axis pair, the two strict sign predicates

$$
\sigma_A\sigma_D P_A(s)>0,
\qquad
\sigma_B\sigma_D P_B(s)<0
$$

certify the chord merge order. Same-axis adjacency needs no predicate for *order*. However, T1.1 also needs the separator to lie between the two exact crossing heights. Therefore the same strict signs are evaluated for every adjacent event pair: they certify topology for cross-axis pairs and certify a conservative event-height bracket for same-axis pairs. If a sign is unresolved or fails, the interval is split at that chord-derived separator. An unproved interior multi-axis tie is split at its chord event height, converting the ambiguity into a shared closed endpoint; midpoint is used only when the suggested knot is not representably interior. No exact grid-event root is used to decide acceptance.

This proves order, not exact event height. For conservative min/max rejection, event $i$ receives the bracket bounded by the separators immediately before and after it, or by the segment endpoint. Open-cell height intervals and closed event-owner intervals are formed from those brackets. They may be widened but may never omit the exact T0 interval.

## 3. Ordered stages

### T1.0 — logical CPU certificate

For each regular monotone interval:

1. evaluate exact chord endpoints and generate the ordinary finest-grid chord DDA events;
2. accept a multi-axis tie only when its boundary polynomials are proven proportional (and therefore share the event); otherwise reject/split it;
3. test every adjacent event pair at its chord-height separator, marking cross-axis tests as topology predicates and same-axis tests as height-bracket predicates;
4. accept when all strict filtered signs resolve;
5. otherwise split at the failing chord separator or chord-corner event, using the binary64 midpoint only as a precision fallback, and repeat; and
6. compare every accepted segment against the independent T0 exact event, closed-cell, ordered-cell, and reverse-order oracle.

The T0 audited turn/pole partition is reused in this stage and counted separately. The practical corpus previously contained no interior $u$, $v$, or world-$t$ turn; analytic cases retain coverage for those paths. This stage does not claim that the current audited partition is the final device implementation.

### T1.1 — conservative leaf-height replay

For each accepted segment:

1. bracket every exact event between its adjacent certified separators;
2. assign conservative height spans to ordered open cells, closed event owners, and segment endpoints;
3. merge duplicate cells across adjacent segments without shrinking their height union;
4. fetch the corresponding 64×64 leaf min/max interval; and
5. retain the leaf exactly when the conservative height span overlaps min/max.

The T0 exact event brackets independently generate a required leaf set. Candidate rejection may add leaves but may omit none.

### T1.2 — structural comparison

Join each ray by `ray_id`/replay index to the passing A2 NRT-QT output and report by shell, ray family, asset, and pooled corpus:

- runtime interval attempts, splits, segments, and maximum split depth;
- chord DDA events and distinct leaf cells;
- cross-axis separators and boundary-sign predicates;
- unresolved signs, chord-corner ambiguities, and fallbacks;
- conservative min/max survivors and exact-required survivors;
- candidate/NRT min/max-fetch ratio;
- candidate/NRT leaf-candidate ratio; and
- p50/p95/p99/max plus pooled sums.

No weighted “instruction cost” is fabricated on the CPU. FMA, division, texture-fetch, branch, register, and occupancy costs require a device primitive microbenchmark. T1 instead requires strong structural headroom before authorizing that benchmark.

## 4. Frozen gates

All correctness gates are mandatory:

- zero accepted pole or derivative sign reversal;
- zero accepted exact/chord event-order mismatch;
- zero exact closed-cell or ordered-cell omission;
- zero required leaf-min/max candidate omission;
- reverse traversal gives the reversed event/cell sequence under the frozen boundary rule; and
- zero silent depth/cap acceptance.

Practical feasibility gates:

- p95 accepted segments per practical interval $\le 4$, maximum $\le 16$;
- no more than `5%` of practical intervals require fallback or unresolved exact-event correction;
- pooled direct-DDA leaf-cell fetches $\le 0.50$ of NRT-QT min/max fetches;
- no ordinary ray family has a median leaf-cell-fetch ratio above `0.75`;
- pooled separator boundary-sign predicates $\le 0.25$ of NRT-QT node visits;
- pooled conservative min/max survivors $\le 1.25$ of NRT-QT leaf candidates; and
- no ordinary ray family has a pooled survivor ratio above `1.50`.

The fetch and predicate gates test whether enough structural headroom exists to tolerate a more expensive candidate primitive. The survivor gates prevent a cheap traversal from merely transferring excessive work to exact cubic leaves.

Any correctness failure stops immediately. If any structural gate fails, do not build a device primitive. Passing T1 authorizes only a T1.5 isolated CUDA microbenchmark; it does not authorize a renderer mode.

An unproved chord-corner ambiguity is split at most 16 levels in the CPU diagnostic. Persistence beyond that point is an explicit counted local NRT fallback, never an accepted microsegment. A practical fallback ray is charged its complete NRT-QT min/max and leaf-candidate work in the structural totals, in addition to its already counted certificate predicates.

The general diagnostic depth limit likewise produces an explicit `certificate-fallback`; it is not a correctness failure or an accepted partial cover. Analytic fallback frequency is reported separately, while the frozen `5%` continuation gate is evaluated on practical intervals.

## 5. T1.5 device gate — deferred

If T1 passes, write a separate implementation plan for four same-buffer primitives:

1. filtered separator-sign evaluation;
2. incremental zero-radius DDA event step;
3. conservative event-height/minmax rejection; and
4. the existing NRT-QT node range step.

The device gate must include randomized trial order, checksum equality, register/local-memory reporting, and at least `20%` median non-leaf timing headroom on the held-out ordinary cohort before a complete GPU traversal is considered.

## 6. Reporting boundary

A T1 pass supports only: “the runtime certificate has enough structural headroom to justify device measurement.” A T1 failure supports: “compact exact topology exists, but this separator-sign realization does not preserve sufficient work advantage.” Neither result is an end-to-end speed comparison.

## 7. Decision log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-30 | Start T1 as a new per-cell-cost hypothesis. | T0 failed the candidate-cell-reduction gate but left ordinary DDA arithmetic versus nonlinear node work unresolved. |
| 2026-08-30 | Use A2 Bernstein-range NRT-QT as the primary cost comparator. | It is the authoritative passing same-surface device baseline and differs from the upstream six-quadratic sibling implementation. |
| 2026-08-30 | Use separator signs rather than exact event solves for acceptance. | Monotonicity fixes per-axis order; strict signs at separators certify only the remaining cross-axis merge order. |
| 2026-08-30 | Defer weighted cost to a device microbenchmark. | CPU operation weights cannot represent GPU texture, division, branch, register, and occupancy costs honestly. |
| 2026-08-30 | Require separator signs for same-axis neighbors before implementation. | Same-axis order is monotone automatically, but min/max rejection also requires proof that the chosen separator lies between the two exact event heights. |
| 2026-08-30 | Prove proportional corner events and inherit whole-interval pole fallback. | Exact grid-diagonal travel cannot be removed by midpoint splitting, while spans adjacent to an outward pole bracket are not numerically suitable for endpoint event classification. |
| 2026-08-30 | Cap only repeated unproved-corner splitting and charge full NRT fallback. | Constructed turn/corner coincidences otherwise create meaningless depth-37 fragments; explicit fallback is the conservative runtime behavior and is governed by the frozen 5% practical gate. |
| 2026-08-30 | Use the failed chord event/separator as the lazy knot. | The 18-ray smoke test showed that unrelated midpoint bisection chases one known corner for 16 levels; the chord already supplies a root-free, traversal-coupled split location. |
| 2026-08-30 | Align the independent reverse audit with the `2e-9` closed-owner tolerance. | One reparameterized endpoint event moved to normalized height `7.15e-10`; forward topology and closed ownership already matched, and this tolerance does not affect candidate acceptance. |
| 2026-08-30 | Pass T1 correctness but stop before T1.5. | Direct leaf DDA has strong fetch-count headroom, but p95 segmentation, sign-predicate work, and near-miss survivor gates fail under the frozen protocol. |

## 8. T1 result — 2026-08-30

The authoritative run is `experiments/ray_separator_t1/ray-separator-t1-c8ca43b64c6c`. It evaluates all 864 practical rays and the 54 analytic case/resolution pairs. The implementation and combined regression suite are:

- `scripts/ray_separator_t1.py`;
- `scripts/run_ray_separator_t1.py`;
- `scripts/test_ray_separator_t1.py`; and
- `13/13` passing T0+T1 unit tests.

### 8.1 Correctness

Every accepted segment passes:

- exact/chord event-label and grouping equality;
- closed-cell and ordered open-cell equality;
- conservative containment of every exact event-height bracket;
- reverse traversal under closed endpoint ownership; and
- filtered binary64 sign agreement with the independent Decimal sign.

There are zero required min/max survivor omissions, zero practical fallback intervals, and zero unresolved practical signs. The six denominator-pole refusals, eight corner fallbacks, and three general certificate fallbacks are analytic stress outcomes; none is silently accepted.

### 8.2 Structural work

| quantity | candidate | A2 NRT-QT | ratio |
|---|---:|---:|---:|
| min/max fetches | `6025` direct leaf cells | `34055` hierarchy nodes | `0.1769×` |
| min/max survivors / leaf candidates | `2082` | `1948` | `1.0688×` |
| separator sign predicates / NRT node visits | `11561` | `34520` | `0.3349×` |

The fetch count is the main positive evidence: direct ordered DDA exposes 82.3% fewer min/max fetch operations structurally. These are not equivalent-cost operations—leaf access patterns, predicate arithmetic, exact-leaf work, registers, and divergence are not timed here—so `0.1769×` is not a runtime prediction.

Practical segments per monotone interval are p50/p95/p99/max `1/7/10/14`. The tail is concentrated in the constructed grid-corner family, whose p50/p95/max are `6/10/14`; front rays remain exactly one segment, grazing rays have p95 `4`, and the other ordinary families have p95 at most `3` except grid-corner. Moderate and stress shells have p95 `7` and `9`, respectively.

### 8.3 Frozen gates

Passing structural gates:

- no correctness or required-candidate omission;
- zero practical fallback, below `5%`;
- maximum segments `14`, below `16`;
- pooled fetch ratio `0.1769×`, below `0.50×`;
- every family median fetch ratio below `0.75×`;
- pooled survivor ratio `1.0688×`, below `1.25×`.

Failing structural gates:

| gate | observed | required |
|---|---:|---:|
| practical p95 segments | `7` | `≤4` |
| sign predicates / NRT node visits | `0.3349×` | `≤0.25×` |
| worst family survivor ratio | near-miss `1.6404×` | `≤1.50×` |

Near-miss retains `333` leaves versus NRT-QT's `203`; the wide separator-derived event-height brackets are safe but transfer too much work to the leaf stage. Oblique is close to the family limit at `1.4566×`. Grid-corner segmentation and grazing's long event sequences dominate certificate work.

### 8.4 Decision and claim boundary

T1 is a mixed mechanism result and a negative continuation decision. It proves that separator signs can replace exact event roots while preserving topology and conservative min/max candidates on the full practical corpus. It also reveals that using failed separators as segmentation knots couples height-bracket tightness to segment proliferation, especially at grid corners.

Under the frozen all-gates rule, T1.5 CUDA primitive timing and a complete GPU mode are not authorized. The run does not show that the candidate is slower; it shows that this realization lacks enough pre-device headroom on all three protected dimensions to justify another GPU branch.

Possible future hypotheses must be planned separately. The most concrete are an exact-affine zero-predicate fast path and a predict/correct event-height bracket that keeps topology segments intact instead of splitting at every failed separator. They may not be presented as repairs to the frozen T1 result.

Safe paper language:

> A separator-sign oracle certifies ordered shell-ray DDA without exact grid-event roots and reduces structural min/max fetches to `0.177×` those of the same-surface nonlinear quadtree. However, conservative event-height bracketing raises p95 segmentation to seven chords, evaluates `0.335×` as many sign predicates as NRT visits, and retains `1.64×` near-miss leaf candidates. We therefore report feasibility and mechanism evidence, not a GPU speed claim.

Draft handoff: [[Note — EG draft handoff after separator T1]].
