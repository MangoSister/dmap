---
title: Plan — O2 on-device ordered shell DDA
tags: [plan, ray-tracing, CUDA, DDA, interval-newton, closest-hit]
status: isolated-query-performance-negative-renderer-validation-open
created: 2026-08-30
updated: 2026-08-30
parent: "[[Plan — T1.6 ordered closest-hit shell DDA]]"
---

# Plan — O2 on-device ordered shell DDA

> [!abstract] Purpose
> O1 run `ray-o1-closest-b35fd77510db` proves that an ordered, tightly contracted leaf schedule has enough GPU headroom: held-out ordinary traversal is `0.5360×` fair ordered NRT-QT. O2 charges the omitted construction work. It ports one mathematical checkpoint at a time and culminates in a fused kernel that reads only the ray/shell, height pyramid, and fixed method constants. CPU schedules remain differential truth only and may not influence the fused runtime path.

## 1. Frozen query and comparison contract

The query is the closest represented-surface hit along increasing world-ray parameter $t$. Candidate, ablations, and NRT-QT use the same packed 65×65 displacement window, exact two-microtriangle cubic leaf solver, tolerance-scaled canonical owner rule, and fallback result. A candidate fallback invokes the fair ordered NRT-QT path for that ray and is included in candidate timing.

O2 retains the current fixed scope:

- one triangle-proxy shell with rational $u(h),v(h),t(h)$;
- a 64×64 microgrid and its scalar min/max mip hierarchy;
- topology-preserving zero-radius chord DDA with closed edge/corner ownership;
- event-local one-step interval Newton (`IN1`); and
- explicit fallback for a denominator pole, unresolved derivative sign, separator sign, non-progressing split, capacity, or device interval failure.

No first-order displacement slab participates in this ray path. The performance hypothesis is caused by shell-space topology, local event contraction, DDA order, and closest-hit termination.

## 2. O2.1 — outward device `IN1` primitive

Export every O0.5 event label as an independent record containing:

- binary64 quadratic boundary polynomial $P(h)=c_0+c_1h+c_2h^2$;
- original separator bracket $I=[a,b]$;
- event/group identity and CPU `IN1` reference bracket; and
- independently computed Decimal root enclosure.

Implement the device operation with directed binary64 interval arithmetic:

1. $m=a+(b-a)/2$;
2. outward interval evaluation of $P(m)$;
3. outward derivative range $P'(I)=c_1+2c_2I$;
4. reject to the original bracket if zero lies in the derivative range;
5. compute $N=m-P(m)/P'(I)$ outward;
6. intersect $N$ with $I$; and
7. reject to $I$ if any value is nonfinite or the intersection is empty.

For a multi-axis corner group, contract each label from the same original bracket and intersect all successful results; any empty combined intersection restores the original bracket.

O2.1 gates:

- zero independently audited Decimal-root omission;
- zero device/CPU bracket disjointness;
- device width no more than `1.10×` CPU `IN1` width at p95, excluding unchanged zero-width cases;
- no more than `2%` uncontracted event groups; and
- exact agreement of derivative-zero/empty fallback semantics on the frozen differential vectors.

No timing claim is made at O2.1; its purpose is to establish the arithmetic primitive used by the fused kernel.

## 3. O2.2 — device chord events and separator certificate

Start with CPU-provided monotone valid intervals but no event labels, brackets, cells, or order. For each interval on device:

1. evaluate outward endpoint $u,v$;
2. enumerate crossed dyadic grid boundaries directly from the endpoint range;
3. form chord crossing heights and group simultaneous crossings with the frozen tolerance;
4. apply the T1 filtered separator-sign test between adjacent groups;
5. split at the certified hint or midpoint on refusal, preserving front-to-back work order;
6. run device `IN1` on accepted event groups; and
7. emit open-region and closed-event owner batches with conservative height spans.

The checkpoint output is a fixed-capacity diagnostic event/cell stream, not yet an intersection kernel. Compare it record-for-record against CPU T1/O0.5:

- accepted segment count and interval endpoints;
- event signatures, group order, and persistent boundaries;
- contracted brackets and ordered closed owners;
- merged cell set and first-occurrence order; and
- suffix conservative world-$t$ lower bounds.

Implement O2.2 in two frozen subcheckpoints so event arithmetic is debugged separately from recursive control flow:

- **O2.2a accepted-segment differential:** input the 1,632 CPU-accepted segment endpoints and derivative signs, but no event labels, representatives, brackets, owners, cells, or order. The device must independently regenerate and certify each accepted segment, apply O2.1, and reproduce its event/cell stream. This is not a performance variant.
- **O2.2b monotone-interval differential:** replace those inputs by the 864 parent monotone intervals and let the device split every refused certificate. It must reproduce the accepted segment partition and the merged front-to-back schedule. Only O2.2b removes CPU segmentation decisions.

O2.2 gates are zero missing event/owner/cell, zero unsafe suffix bound, zero order disagreement after the documented closed-group convention, and no ordinary fallback. A fresh T1 inventory supersedes the earlier T0 chord count for capacity planning: the current corpus reaches 14 accepted separator-certified segments, 38 events in one segment/ray, 27 segmentation attempts, depth 13, and 39 unique cells. Fixed diagnostic capacities are therefore `16` accepted segments, `48` events, `32` work tasks/attempts in depth-first storage, and `64` unique cells. These are measured implementation capacities, not universal mathematical bounds; overflow must invoke and report NRT fallback.

## 4. O2.3 — on-device valid/monotone interval construction

Remove the last CPU structural input. Reuse the validated rational coefficient construction and derivative numerator arithmetic. Isolate denominator roots and $u'(h),v'(h),t'(h)$ turns over the shell-height/ray-$t$ supported domain, sort the finite roots, and attach fixed derivative signs to each open interval. Endpoint ownership is closed under the same tolerance as T0.

Differential outputs are supported intervals, pole/turn labels, signs, and fallback reasons. Gates are zero omitted valid interval, zero wrong derivative sign, zero practical pole/turn miss, and no ordinary fallback on the frozen 864-ray corpus. This stage may use the already validated low-degree root isolation code; it must not use the min/max hierarchy to infer topology.

## 5. O2.4 — fused ordered closest-hit kernel

Fuse O2.3, O2.2, O2.1, leaf min/max, exact cubic testing, and suffix-$t$ termination in one ray kernel. Generate segments lazily in increasing world $t$. Within an accepted segment, construct one contracted event at a time and emit DDA batches immediately; do not materialize the 1,288-byte O1 schedule in global memory.

The first implementation may retain a small per-thread event array to compute the remaining suffix lower bound. A streaming replacement is an optimization only after the fixed implementation passes. Duplicate cells across adjacent segments use a 64-bit leaf mask when possible; shared event owners must all be tested before advancing the termination frontier.

Required ablations on identical inputs:

- `PRE-IN1`: O1 CPU-precomputed contracted schedule (optimistic envelope);
- `DEV-IN1`: device contraction with precomputed event signatures/order;
- `DEV-EVENT`: device chord events, separator certificate, contraction, and DDA with precomputed monotone intervals;
- `FULL`: all construction and traversal on device;
- `FULL-NO-STOP`: full construction with closest-hit termination disabled; and
- fair ordered `NRT-QT`.

Report median/p50/p95/p99 construction counts per ray, segments, events, sign predicates, contracted/uncontracted events, emitted unique cells, min/max fetches, exact leaf tests, early-stop fraction, fallback reasons, registers, local memory, occupancy, and paired timing distributions.

## 6. Frozen end-to-end gates

Correctness and robustness:

- all candidate/NRT/Mode 0 closest-hit checksums match on every cohort;
- zero unsafe early stop, closed-owner omission, overflow, or silent unresolved output;
- every fallback returns the NRT result and is included in timing; and
- ordinary fallback rate is at most `1%`, with every reason reported.

Performance on the RTX 5090 frozen protocol (32,768 records/cohort, 50 paired trials, CV below `5%`):

- `FULL/NRT ≤ 0.85×` on held-out ordinary rays;
- `FULL/NRT ≤ 0.95×` on held-out S1 and S2;
- no held-out ordinary family above `1.15×`;
- `FULL` retains at least `35%` of O1's absolute decision-ordinary time saving; and
- the construction-cost ladder is monotone and fully reported; no omitted CPU preprocessing or uploaded per-ray schedule may be called end-to-end.

Passing authorizes renderer integration and external TFDM/RMIP/Ogaki comparisons. Failing stops the end-to-end speed claim but preserves O1 as a mechanism envelope and the certified topology/event-local-contraction contribution.

## 7. Immediate implementation order

1. Freeze O2.1 binary schema and CPU event-vector exporter.
2. Unit-test packing, group identities, CPU reference brackets, and Decimal roots.
3. Implement the CUDA binary64 interval-Newton primitive and differential output.
4. Run all 4,777 event groups; diagnose every fallback/disagreement before O2.2.
5. Only after O2.1 passes, write the detailed O2.2 event-stream data layout and implement it.

## 8. Decision log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-30 | Authorize O2 after O1 passes. | Ordered contracted traversal has `46.4%` timing headroom on held-out ordinary rays versus fair NRT-QT. |
| 2026-08-30 | Port `IN1` before event generation. | It is independently testable and was the mechanism that changed O0 from `0.9291×` to the `0.5405×` ideal leaf-work ratio. |
| 2026-08-30 | Require the full kernel to read no CPU schedule. | O1 deliberately excluded construction; only a fused device path can support an end-to-end method claim. |
| 2026-08-30 | Include NRT fallback time in the candidate. | Unsupported rays are part of the method distribution and cannot be removed from performance results. |
| 2026-08-30 | Make closed multi-axis groups retain the original bracket without an exact simultaneity proof. | Five near-proportional binary64 corner groups produced roots separated by a few ulps; intersecting their outward contractions could exclude both roots. The conservative fallback matches the Decimal CPU semantics. |
| 2026-08-30 | Size O2.2 from T1 runtime refinement, not T0 exact chords. | T1 reaches 14 accepted segments, 38 events, 27 attempts, and depth 13; the former 4/16 draft capacities confused a different segmentation statistic. |

## 9. O2.1 result

Authoritative run: `ray-o21-in1-6e70fd371f28`. The CUDA primitive processes all 4,777 event groups / 4,825 boundary polynomials and passes every frozen gate:

- zero Decimal-root omission;
- zero device/CPU bracket disjointness;
- zero identity, status, or fallback-semantics mismatch;
- exactly 48 uncontracted groups (`1.0048%`), matching CPU O0.5; and
- device/CPU width ratio p50 `1.0`, p95 `1.0000000000457`, p99 `1.0000000002123`, maximum `1.0000004812`.

The initial aligned device implementation revealed a robustness issue in approximate corner simultaneity: six two-axis groups produced one-ulp overlaps between contractions of distinct binary64 polynomial roots, whereas the Decimal CPU path detected an empty intersection and retained the original bracket. The final device contract does not infer exact simultaneity from near proportionality. All two-axis closed groups retain their original bracket; single-axis groups use directed-FP64 `IN1`. This is conservative, matches the frozen CPU work schedule, and should remain explicit in the method/limitations text.

O2.1 therefore establishes the arithmetic primitive required by the fused method. It provides no timing result and does not yet charge chord event generation or separator certification.

## 10. O2.2 result

Both staged differentials pass.

- O2.2a authoritative run: `ray-o22a-events-6c0be4eb9bb0`. Starting only from 1,632 accepted segment endpoints, derivative signs, and rational coefficients, the device regenerates all 4,777 event groups and 7,693 per-segment cells. Event identities, representatives, separator work, `IN1` fallback work, cell order, and Decimal-root containment all match; there are no missing roots or disjoint cell intervals.
- O2.2b authoritative run: `ray-o22b-segmentation-62f317cd404e`. Starting from the 864 parent monotone intervals, the device reproduces all 1,632 accepted segments, all 4,777 events, and the 6,025 merged per-ray cells. Every parent resolves with zero partition, signature, order, interval, or work-counter mismatch.

The first O2.2b implementation incorrectly discarded the separator at which the ordering/sign certificate failed and used the interval midpoint. CPU T1 splits at that failed separator. Propagating the failed separator as the CUDA split hint restores exact recursive agreement. This is an implementation correction, not a relaxed gate: the failed run remains non-authoritative.

O2.2 establishes device chord segmentation, event reconstruction, local contraction, and ordered closed-cell emission when given a valid monotone parent. It still excludes construction of that parent and therefore is not an end-to-end timing result.

## 11. Frozen O2.3 diagnostic contract

O2.3 removes the CPU-provided valid/monotone parent in two observable layers while retaining one device kernel and one differential artifact.

### 11.1 Input and event families

Each diagnostic record contains only the rational center polynomials `A`, `B`, `D`, `U`, `V`, and `T`, `|r|^2`, the closed displacement height band, and the ray's closed `[t_min,t_max]`. It contains no valid interval, turn location, derivative sign, chord segment, event stream, cell, or traversal order.

Mandatory support polynomials are

\[
D,\quad A,\quad B,\quad C=D-A-B,\quad
T-t_{min}|r|^2D,\quad T-t_{max}|r|^2D.
\]

The derivative numerators are `U'D-UD'`, `V'D-VD'`, and `T'D-TD'`. The first two are at most quadratic and the last is at most quartic after cancellation. Their signs fix monotone `u`, `v`, and world-ray `t` orientation.

### 11.2 Runtime root policy

Use a fixed-capacity binary64 degree-at-most-four root isolator. It recursively partitions a polynomial by its derivative roots, detects sign-changing roots on monotone pieces, retains near-zero derivative critical points as repeated-root candidates, refines by safeguarded Newton/bisection, sorts/deduplicates, and emits a conservative closed bracket around every representative. Any nonfinite arithmetic, degree ambiguity, unresolved sign, capacity pressure, or denominator root in the supported band returns an explicit NRT fallback for the entire ray.

Support is composed by sorting and merging mandatory event brackets, classifying the regular open gaps, and attaching each closed non-pole event bracket to every adjacent valid component. Exact leaf evaluation continues to recheck proxy and ray-range validity. Turn events then subdivide supported components, and a filtered midpoint sign is attached to each open monotone interval.

The fixed diagnostic capacities are 32 event brackets, eight supported components, and 16 monotone intervals. They are diagnostic capacities, not universal mathematical bounds; overflow is a reported fallback.

### 11.3 Differential corpus and outputs

Run all 864 frozen practical rays from their raw rational coefficients and height/ray domains. The practical inventory currently contains 641 rays with no mandatory interior root, 216 with one, and seven with two; the closest two distinct roots are separated by about `0.32546`. Valid boundaries are the height band or proxy `A/C` events. No practical ray has a denominator pole or `u/v/t` turn, so this corpus exercises the fast path but is not sufficient alone.

Add analytic strata for denominator poles, proxy entry/exit and tangency, ray-range entry/exit, affine/degree-reduced polynomials, repeated and clustered roots, simultaneous event kinds, `u`/`v`/`t` turns, endpoint roots, and deliberate capacity/unresolved fallback. Persist:

- event kind, representative, and closed bracket;
- supported component endpoints and attached mandatory-event masks;
- monotone interval endpoints and `u/v/t` signs; and
- fallback status/reason plus root/refinement counters.

### 11.4 O2.3 gates

- every independently audited Decimal mandatory/turn root is covered by a same-kind device bracket;
- every CPU valid component is covered by the union of device supported components;
- every emitted open component satisfies proxy and ray-range predicates away from retained event uncertainty;
- zero wrong `u/v/t` derivative sign and zero omitted monotone component;
- denominator poles and all unresolved/capacity cases take the declared whole-ray NRT fallback;
- no ordinary fallback on the frozen 864-ray corpus; and
- feeding every ordinary device monotone interval into the already-passing O2.2 path produces no missing exact owner or unsafe front-to-back order.

O2.3 is a correctness and construction-work checkpoint. Timing begins only after its outputs are consumed inside the fused O2.4 kernel.

## 12. O2.3 result

Authoritative interval run: `ray-o23-intervals-193a27fbac7b`. The device receives raw rational polynomials and height/ray domains for 864 practical plus nine analytic records. It emits all 243 independently audited root kinds with zero root omission, covers every valid and monotone interval, attaches the correct `u/v/t` signs, takes the declared pole fallback, and has zero practical fallback.

Authoritative composition run: `ray-o23-o22-pipeline-fdb9d471a86e`. Device-produced O2.3 intervals drive the already validated O2.2 segmentation executable. All 864 practical parents resolve. The stream contains all 4,777 expected events plus ten conservative closed-support endpoint events, but the extras introduce no additional cells: both references contain 6,025 cells. There are zero Decimal event-root omissions, zero exact-owner omissions, zero relative-order errors, and every emitted cell interval contains the exact-event cell interval.

Seven grazing rays produce 14 cell intervals that are a few micro-heights tighter than the CPU `IN1` runtime enclosure. This is not a safety failure: both are conservative approximations, and the device interval contains the independent exact-event interval. The composition gate deliberately compares against that exact reference rather than requiring one conservative enclosure to contain another.

O2.3 therefore removes the last CPU interval/sign decision. It remains a two-kernel host-mediated differential and provides no end-to-end timing claim.

## 13. Frozen O2.4 fused implementation order

O2.4 is staged so the first timing result cannot hide host construction.

1. **O2.4a / `DEV-EVENT`:** one CUDA kernel reads the packed replay, constructs the rational coefficients using the already validated C1 algebra, constructs O2.3 intervals, performs O2.2 segmentation/events/`IN1`, and emits the diagnostic cell schedule. Compare it with `ray-o23-o22-pipeline-fdb9d471a86e`. This checkpoint may materialize a per-thread schedule but no CPU coefficient, interval, event, or cell input is allowed.
2. **O2.4b / `FULL-NO-STOP`:** add height fetches, leaf min/max rejection, and the exact two-microtriangle cubic solver. Visit every admitted ordered cell. The closest-hit checksum, exact-test count, fallback result, and all construction counters must match a host-composed replay of the validated stages.
3. **O2.4c / `FULL`:** add suffix world-`t` bounds and terminate only after all cells in the current closed event batch are complete and the minimum lower bound of every unprocessed cell is strictly beyond the current closest hit. Differential-test every early stop against `FULL-NO-STOP`, Mode 0, and fair NRT-QT.
4. **O2.4d / paired timing:** only after correctness, expand the same frozen cohorts to 32,768 records and time `PRE-IN1`, `DEV-IN1`, `DEV-EVENT`, `FULL-NO-STOP`, `FULL`, and fair ordered `NRT-QT` with identical launch geometry and paired trial order.

The fused input is the packed proxy positions, displacement directions, atlas coordinates, ray origin/direction/range, and the 65x65 height window/min-max hierarchy. Rational coefficients may live in registers/local state after device construction; they may not be uploaded as a second per-ray input. Diagnostic schedules may be written only by O2.4a and are forbidden from the timed `FULL` path.

The first implementation keeps the validated fixed capacities and reports overflow to the in-kernel NRT fallback. It may retain per-thread arrays for at most 16 accepted segments, 48 event groups, and 64 unique cells. Schedule streaming and capacity reduction are later optimizations, not correctness changes.

O2.4a gates are exact agreement of coefficient status, interval/sign status, expected-event ordered subsequence, exact owner set/order, and exact-event interval coverage, with zero ordinary fallback. O2.4b/c gates are the frozen checksums, no unresolved output, no unsafe stop, and fallback correctness. Performance gates remain those in Section 6.

## 14. O2.4a coefficient-construction repair

The first fused diagnostic used the O1 dominant-axis annihilator frame. It resolved all 864 practical rays and reproduced the packed oracle's 5,888 owner cells and their relative order, but 1,383 tightly contracted device event brackets did not contain the corresponding Decimal root of the independently stored packed coefficients. The owner and interval-coverage gates pass, so this is a coefficient-representation mismatch rather than a lost DDA cell; O2.4a nevertheless remains failed because a tight root certificate may not silently change its represented polynomial.

A diagnostic attempt reproduced the packed oracle's normalized least-aligned-axis frame in binary64. It reduced root omissions only from 1,383 to 1,367, proving that matching the frame convention does not make point-coefficient `IN1` a coefficient-construction certificate. The maximum discrepancy was only `3.70e-15` in height, but a generic epsilon would hide rather than solve the issue.

The frozen repair therefore uses the normalization-free dominant-axis annihilator from O1 and evaluates the same coefficient expression tree a second time with directed interval operations on the exact packed binary32 inputs. For each boundary polynomial, the interval coefficient error is converted to a root displacement using a conservative lower bound on the center polynomial's derivative over the owning separator interval. The contracted interval is expanded by that displacement; if the derivative cannot be separated from zero, it reverts to the full separator interval. O1's exact world-space leaf solver remains unchanged and does not supply traversal events.

## 15. O2.4a result

Authoritative run `ray-o24a-fused-schedule-8d49b0e81392` passes. One CUDA kernel starts from the 864 packed shell/ray replays, constructs dominant-axis rational coefficients and directed-rounding coefficient enclosures, performs O2.3 support/monotonicity construction, O2.2 segmentation/event generation, and uncertainty-aware `IN1`, then emits the diagnostic ordered cell stream.

The initial point-coefficient contraction exposed 1,383 root-bracket mismatches even though owner/order and exact cell-interval coverage were already correct. Their normalized-frame diagnostic displacement was tiny (median `2.70e-16`, maximum `3.70e-15` in height), but treating device coefficients as exact was not a valid certificate. The repair evaluates the coefficient expression tree with directed rounding and expands each contracted root by the coefficient value-error divided by a conservative lower derivative magnitude. The independent root oracle now uses 80-digit arithmetic on the exact dyadic packed inputs and the normalization-free dominant-axis algebra; the packed exhaustive surface oracle remains the independent owner and exact-hit truth.

Final counts are 5,017 expected events, 5,027 device events, and 5,888 cells on both sides. The ten extras are conservative closed-support endpoint events and create no extra cells. There are zero exact root omissions, owner omissions, relative-order errors, cell-interval coverage errors, status failures, or ordinary fallbacks. O2.4a therefore removes every host-provided coefficient, interval, segment, event, and cell decision from the runtime path. It is a correctness checkpoint, not yet an end-to-end traversal timing result.

## 16. Frozen O2.4b `FULL-NO-STOP` contract

O2.4b changes only the consumer of the passing O2.4a schedule. It must not alter coefficient construction, support partitioning, segmentation, event contraction, cell ownership, or order.

### 16.1 Device algorithm

For each packed ray, one kernel will:

1. run O2.4a into fixed thread-local storage;
2. visit every emitted unique leaf cell in its recorded front-to-back order, with no closest-hit termination;
3. intersect the cell's uncertainty-aware height interval with the level-six scalar min/max interval;
4. reject an empty intersection, otherwise fetch the four packed heights and test both represented microtriangles with the validated binary64 cubic isolator, packed four-variable world refinement, closed barycentric/proxy/ray-range filters, and canonical `(t,h,owner)` tie break; and
5. return the closest hit only after every admitted cell has been processed.

The static min/max pyramid may be constructed once from the packed displacement window before upload; it is surface data, not a per-ray host decision. No CPU coefficient, interval, segment, event, cell, candidate, or hit input is allowed. `FULL-NO-STOP` deliberately ignores the available front-to-back stopping opportunity so O2.4c can isolate that benefit.

### 16.2 Implementation boundary

Do not include the monolithic A1/O1 translation unit in the fused source: the attempted inclusion caused multi-gigabyte compiler state and is not a viable implementation. Add a compact O2 leaf-runtime component containing only the already validated cubic derivative partition, sign-changing root refinement, packed world refinement, hit filter, and closest-hit update under O2-prefixed names. Formulas and tolerances must remain identical to O1; differential outputs, rather than source identity, are the gate.

The diagnostic output stores the complete closest hit plus: construction status, segment/event/cell counts, attempt/split/certificate counts, min/max fetches, admitted candidates, exact leaf/root tests, candidate hash sum/xor, and fallback count. It may retain O2.4a's fixed local arrays. Streaming is a later performance optimization.

### 16.3 Differential gates

On all 864 frozen practical rays require:

- O2.4a construction counters and ordered-cell inventory unchanged;
- zero nonregular or fallback result;
- exact agreement with the packed exhaustive oracle's hit/miss, closest owner, and checksum;
- closest `t`, height, texture coordinates, proxy coordinates, and world residual within the frozen A2 tolerances;
- candidate count/hash equal to an independent host replay of leaf min/max over the exact O2.4a cell intervals;
- `leafTestCount = 2 * candidateCount`, with exact root-test accounting; and
- byte-deterministic input packing plus corruption/stale-binary rejection.

Also compare the hit checksum with O1 and fair NRT-QT. Timing remains forbidden in O2.4b; its purpose is to prove that the fully device-constructed ordered stream reaches the same represented surface before early termination is introduced.

## 17. O2.4b result

Authoritative run `ray-o24b-full-no-stop-1b8c395d9afd` passes all 864 practical rays. It starts from raw packed shell/ray inputs, constructs the O2.4a stream on device, visits every emitted cell, performs leaf min/max rejection, and tests the exact represented microtriangles. The final checksum `12832192919833793057` exactly matches fair NRT-QT/O1. There are zero status failures, fallbacks, hit/miss errors, canonical-owner errors, coordinate errors, world-residual failures, candidate-inventory errors, or leaf-accounting errors.

The full corpus constructs 1,303 accepted segments, 5,027 events, and 5,888 unique DDA cells. Leaf min/max admits 1,403 cells, producing 2,806 exact microtriangle tests and 1,642 isolated roots. The largest difference from the independent packed exhaustive oracle is below `3.70e-13` in any reported hit coordinate.

This stage exposed two tolerance-conversion bugs before passing. Ten proxy-edge hits lay up to about `5e-8` in height beyond a support root even though their proxy residual satisfied the closed leaf tolerance; the fixed height pad was replaced by `coordinate tolerance * |D| / |P'|` plus coefficient uncertainty. One grid-corner tie selected the wrong canonical owner because the adjacent DDA interval began `6.6e-10` too late; the scaled-cell ownership tolerance `2e-9 / 64` is now converted through the same slope-aware rule at every grid event. The repaired O2.4a rerun `ray-o24a-fused-schedule-325c9353dbad` retains zero exact-root, owner, order, or interval failures.

## 18. Frozen O2.4c `FULL` early-stop contract

O2.4c adds only closest-hit termination to the passing O2.4b kernel. It must use the identical construction, tolerance-expanded intervals, min/max test, exact leaf solver, and canonical hit update.

For each emitted cell interval $I_i$, compute a conservative world-ray range by interval-evaluating

$$
t(h)=\frac{T(h)}{\|d\|^2D(h)},\qquad h\in I_i,
$$

using the directed coefficient enclosures already produced by O2.4a. If either denominator factor cannot be separated from zero, assign that cell a lower bound of $-\infty$ and disable stopping across it; do not use a point estimate. Build the reverse suffix minimum $L_i=\min_{j\ge i}\underline t_j$ in thread-local storage.

After completely testing the current closed cell, stop only when a closest hit exists and

$$
L_{i+1} > t_\text{hit}+2\times10^{-9}\max(1,|t_\text{hit}|).
$$

The strict inequality and hit tolerance preserve all seam/corner owners tied with the current hit. A same-event neighboring cell necessarily remains in the suffix with an overlapping conservative range, so a closed event batch cannot be cut in half.

The O2.4c diagnostic must run `FULL` and `FULL-NO-STOP` on the same packed inputs and require, per ray:

- identical status, hit/miss, canonical owner, rounded checksum item, and hit coordinates;
- no fallback and no early stop on a miss;
- identical construction counters and total cell inventory;
- processed cells, min/max fetches, candidates, leaf tests, and root tests no greater than `FULL-NO-STOP`;
- at least one safely stopped hit ray and a reported distribution of skipped cells; and
- the same global checksum as the packed exhaustive oracle, O1, and fair NRT-QT.

No timing conclusion is allowed until this differential passes. O2.4d will then measure whether the saved leaf work repays suffix-range construction and the full device event cost.

## 19. O2.4c result

Authoritative run `ray-o24c-full-e74578e59bd5` passes all 864 practical rays. `FULL` and `FULL-NO-STOP` are executed from the same packed input together with an independent O2.4a schedule replay. There are zero status, fallback, construction, hit, canonical-owner, processed-prefix candidate, leaf-accounting, or checksum mismatches. Every returned hit record is byte-equivalent to `FULL-NO-STOP`, and checksum `12832192919833793057` again matches the packed exhaustive oracle, O1, and fair NRT-QT.

Certified suffix termination stops 491 hit rays (`56.83%` of all records) and never stops a miss. It skips 2,422 of 5,888 emitted DDA cells (`41.13%`), reducing admitted candidates from 1,403 to 1,122, exact microtriangle tests from 2,806 to 2,244, and isolated leaf roots from 1,642 to 1,523. Among stopped rays, skipped-cell p50/p95/p99/max are `3/11/19/37`.

This proves safe front-to-back closest-hit termination for the frozen corpus. It does not yet prove a speedup: every ray still pays raw coefficient construction, support/turn isolation, adaptive segmentation, event construction/contraction, and the reverse suffix pass. O2.4d must measure those costs together.

## 20. Frozen O2.4d paired timing contract

O2.4d is split into a decisive end-to-end comparison and a diagnostic construction ladder. The end-to-end result is measured first but is not interpreted until the ladder identifies where time is spent.

### 20.1 Inputs, cohorts, and baseline

Use the unchanged O1 split manifest and cohorts. Expand each cohort cyclically to 32,768 records; do not resample after observing timing. Static surface data consists of the same packed 65x65 windows and a full seven-level scalar min/max pyramid. `FULL` and `FULL-NO-STOP` read only raw packed replay fields plus surface data. The uploaded O1 schedule is visible only to the explicitly optimistic `PRE-IN1` ablation and must not be read by either end-to-end method.

The baseline is the already validated ordered closest-hit NRT-QT algorithm: float outward rational `u/v` ranges at each scalar min/max quadtree node, child ordering by the monotone world-`t` lower endpoint, exact same two-microtriangle leaf solver, and the same canonical `(t,h,owner)` update. To favor the baseline and preserve direct comparability with O1, its per-ray monotone `t` sign may be uploaded from the frozen O1 schedule. Report that advantage explicitly; `FULL` constructs its signs on device.

### 20.2 One-process paired protocol

Place `PRE-IN1`, `DEV-EVENT`, `FULL-NO-STOP`, `FULL`, and NRT-QT kernels in one executable. Use block size 64 for every timed method, at least 20 untimed alternating warmups, per-method repetition calibration to at least 10 ms, 50 trials, and a deterministic randomized method order within every trial. CUDA events surround only repeated kernel launches; allocation, upload, output download, checksum, and static pyramid construction are excluded for all methods.

`DEV-EVENT` runs raw packed replay through coefficient construction, support/turn isolation, segmentation, event contraction, and ordered unique-cell emission, but performs no min/max or leaf work. It reports the isolated device schedule-construction cost, not an end-to-end method. `PRE-IN1` traverses the frozen CPU-contracted schedule and is the O1 optimistic envelope. `FULL-NO-STOP` isolates termination savings; `FULL` is the only candidate used for the paper performance gate.

The remaining `DEV-IN1` checkpoint is a second diagnostic executable over the same expanded ray identities: it consumes frozen event identities/brackets and times only device `IN1` plus schedule consumption. It is not allowed to delay the decisive `FULL/NRT` result, but must be completed before a final construction-ladder paper plot.

### 20.3 Outputs and gates

For every cohort and method record all 50 launch times, median nanoseconds/ray, CV, paired median ratios and bootstrap confidence intervals, checksum, nonregular/fallback count, registers, local bytes/thread, and active blocks/SM. Persist the randomized order and calibrated repetition count. For untimed diagnostic launches also aggregate segments, events, cells, min/max fetches, candidates, leaf/root tests, and stopped rays.

All end-to-end checksums must match the frozen oracle. `DEV-EVENT` must reproduce O2.4a status and construction inventories; `PRE-IN1` remains labeled optimistic. Apply the Section 6 performance gates without revision. If `FULL` fails them, report the measured result and use `DEV-EVENT`, `FULL-NO-STOP`, resource usage, and O1 `PRE-IN1` to decide whether schedule streaming/local-memory reduction is a justified new optimization stage; do not reinterpret the failure as a speedup.

## 21. O2.4d result

Authoritative run `ray-o24d-timing-f3235f8d772d` is correctness-clean and performance-failed. Every end-to-end method matches the frozen checksum in every 32,768-record cohort, all methods are regular with zero fallback, and all timing CVs are below `5%`. The performance gates fail without revision:

- held-out decision ordinary `FULL/NRT = 1.8852x` (`219.35` versus `116.35 ns/ray`), not at most `0.85x`;
- decision S1/S2 are `1.9067x/1.8538x`, not at most `0.95x`;
- decision oblique/grazing are `2.2417x/2.7783x`, exceeding the family gate;
- the absolute O1 saving is not retained because `FULL` is slower than NRT; and
- the mixed all-corpus ratio `0.9548x` and development-ordinary ratio `0.9163x` do not override the held-out failure.

The work ablation localizes the failure. On decision ordinary, optimistic uploaded `PRE-IN1` is `0.5068x` NRT, while isolated raw-to-schedule `DEV-EVENT` is already `1.4274x` NRT. Thus DDA traversal is still advantageous after a schedule exists, but schedule construction more than consumes the saving. `FULL-NO-STOP/FULL` are `1.8219x/1.8852x`: the suffix pass reduces fetches from 251,689 to 148,333 and leaf tests from 86,008 to 77,358, yet its arithmetic/storage overhead exceeds that saved work on this cohort.

Resource usage identifies the immediate implementation risk. `DEV-EVENT`, `FULL-NO-STOP`, and `FULL` use respectively `22,080/22,128/22,656` stack bytes per thread, compared with `1,360` for NRT and `208` for PRE-IN1. The validated `OutputB` schedule is 5,072 bytes, while nested interval/event/segmentation scratch accounts for the rest. The failure therefore supports a memory-layout diagnostic, not yet a claim that streaming will succeed.

## 22. Frozen O3.1 local-versus-global schedule diagnostic

Before rewriting the mathematics, compare two device-only realizations on the unchanged frozen cohorts:

1. `FUSED-LOCAL`: authoritative O2.4d `FULL` with its thread-local `OutputB` schedule.
2. `TWO-PASS-GLOBAL`: kernel A runs the identical passing `computeO24Schedule` directly into a device-global `OutputB[ray]`; kernel B reads only its cell stream, recomputes the robust context, performs the identical suffix bounds, min/max tests, exact leaves, and canonical update.

Both passes use block size 64. CUDA events cover both launches for `TWO-PASS-GLOBAL`; allocation and transfer remain excluded. No CPU schedule or result may enter either path. Require exact per-ray status, construction counters, hit record, work counters, and checksum agreement with fused `FULL`; report both kernels' registers, stack bytes, occupancy, global schedule bytes/ray, and paired timing.

Run the 864-ray smoke corpus first, then at minimum decision ordinary, decision S1, decision S2, decision oblique, and decision grazing at 32,768 records. The diagnostic authorizes a compact/streaming rewrite only if either:

- two-pass global is at least `15%` faster than fused local on decision ordinary, showing that stack layout materially dominates despite global traffic; or
- its schedule pass is materially faster but the second pass/global traffic erases the gain, showing that a compact streamed consumer has a measurable target.

If neither occurs, do not pursue schedule streaming as the primary rescue. Profile arithmetic/instruction count and design a certified affine/low-event fast path instead. `TWO-PASS-GLOBAL` is an ablation, not the proposed final paper method.

## 23. O3.1 result

O3.1 rejects global schedule materialization as the primary rescue. The 864-ray smoke run `ray-o24d-timing-33be5520b863` and all five 32,768-record decision runs are byte- and work-equivalent to fused `FULL`. The global consumer stack falls from `22,656` to `736` bytes/thread, but the unchanged producer still uses `17,008` bytes and writes `5,072` bytes/ray. Paired `TWO-PASS-GLOBAL/FUSED-LOCAL` ratios are:

- decision ordinary `1.0710`;
- decision S1 `1.0164`;
- decision S2 `1.0630`;
- decision oblique `1.0287`; and
- decision grazing `1.0388`.

Neither O3.1 authorization condition holds. A generic global or compact streaming layout cannot reasonably be credited with closing the much larger held-out gap. The next experiment must reduce event-construction arithmetic itself.

## 24. Frozen O3.2 certified direct-one-segment hybrid

The current accepted-one-segment path duplicates its dominant work. `computeB` first calls `decideB`, which builds and certifies the event groups. After accepting the segment, it calls the O2.2a `compute` routine, which rebuilds and recertifies the same groups before emitting contracted events/cells. This is required by the staged diagnostic architecture but not by the runtime method.

O3.2 introduces two kernels without changing the represented surface:

1. `DIRECT1` constructs robust coefficients and O2.3 supported/monotone intervals. Only when exactly one monotone interval exists, it applies the validated proxy endpoint expansion and calls the passing O2.2a event/cell constructor once over that whole interval. If its separator certificate accepts, it applies the same coefficient-uncertainty event inflation, computes suffix world-`t` bounds over those cells, and runs identical min/max, exact leaves, and canonical closest-hit termination.
2. `GENERAL-FALLBACK` reads a per-ray marker. It returns immediately for accepted `DIRECT1` rays and runs authoritative fused `FULL` only for rejected/unsupported rays. Both kernel launches are included in hybrid timing. No CPU compaction, schedule, interval, or event input is allowed.

Keeping the kernels separate is intentional: placing the general branch inside `DIRECT1` would force every fast ray to inherit the general kernel's 22 KB stack frame. Rejection is an ordinary runtime branch, not a failure, and its reason is counted (`multiple monotone intervals`, `direct certificate rejected`, `direct capacity/status`, or context/support failure).

### 24.1 Correctness and diagnostic gates

On all 864 practical rays require the hybrid's final `O24FullOutput` to be byte-identical to authoritative `FULL` for status, hit record, canonical owner, construction-independent work fields, and checksum. Construction counters may be lower because duplicate certification is removed, but accepted `DIRECT1` cells after uncertainty inflation must exactly equal O2.4a's cells. Every rejected ray must be byte-identical to `FULL` after fallback. Report direct acceptance by split/shell/family and both kernels' resource usage.

### 24.2 Timing go/no-go

Run the frozen decision ordinary, S1, S2, oblique, and grazing cohorts. Continue to an exact structural affine path only if `DIRECT1+fallback` is at least `20%` faster than fused `FULL` on decision ordinary or makes either S1/S2 no slower than `1.25x` NRT while preserving zero fallback error. Otherwise, the duplicated certification is not large enough to justify further specialization; retain the negative O2 result and return focus to the broader multi-application paper.

O3.2 is an optimization hypothesis, not a revised paper gate. The original Section 6 `FULL/NRT` gates remain unchanged.

## 25. O3.2 result and isolated-query decision

O3.2 is exact and performance-negative. Smoke run `ray-o24d-timing-3929a3799131` accepts 679/864 rays (`78.59%`) through `DIRECT1`; its final records and all work totals are byte-identical to fused `FULL`. The direct kernel reduces stack from `22,656` to `15,712` bytes/thread. Nevertheless, `DIRECT1+fallback` is `23.4%` slower than fused `FULL` on the smoke corpus.

Held-out run `ray-o24d-timing-8f4336c2b29e` accepts 28,674/32,768 ordinary rays (`87.51%`) but is `2.6009x` NRT versus fused FULL's `1.8851x`: it is about `38.0%` slower than the already-failing fused implementation. All checksums, byte hashes, work totals, and CV gates pass. The optimization go/no-go fails decisively, so S1/S2 timing and an affine-only special case are not authorized by O3.2.

The isolated candidate-triangle performance-rescue branch is therefore closed. The evidence supports the following bounded conclusions:

- the topology-preserving shell-space DDA, uncertainty-aware local event contraction, exact represented-surface leaves, and conservative front-to-back stopping are implemented and correctness-validated;
- the traversal mechanism is strong after a contracted schedule exists (`PRE-IN1/NRT = 0.5068x` on held-out ordinary), so the original ordering intuition is valid as a mechanism result;
- raw on-device support/segmentation/event construction is the dominant cost (`DEV-EVENT/NRT = 1.4274x`) and makes the isolated complete candidate query noncompetitive (`FULL/NRT = 1.8852x`);
- neither global schedule materialization nor a certified direct-one-segment hybrid improves that outcome; and
- the paper must not claim that this ray application outperforms NRT-QT, Ogaki, RMIP, or TFDM. External comparisons are not warranted until a materially different construction algorithm exists.

This result does not include scene BVH traversal, multiple candidate proxies, OptiX scheduling, complete primary/secondary/visibility mixtures, shading, or frame coherence. Consequently it is not a renderer-FPS result. Full-renderer validation is reopened under [[Plan — R1 full-renderer validation of ordered shell DDA]]. For the unified conservative-query project, ray tracing remains a correctness/architecture application unless R1 produces a positive whole-frame result. The unfinished `DEV-IN1` micro-ablation is no longer decision-critical and should be completed only if a final ray construction-ladder figure is retained in the paper.
