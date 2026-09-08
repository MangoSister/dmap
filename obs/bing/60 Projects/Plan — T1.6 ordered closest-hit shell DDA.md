---
title: Plan — T1.6 ordered closest-hit shell DDA
tags: [plan, ray-tracing, shell-space, DDA, closest-hit, early-termination]
status: o1-passed-full-device-pending
created: 2026-08-30
updated: 2026-08-30
parent: "[[Plan — T1.5 measured separator-DDA performance]]"
---

# Plan — T1.6 ordered closest-hit shell DDA

> [!abstract] Correction and purpose
> T1.5-A measured an unordered all-candidate leaf schedule: it merged T1 cells globally, sorted them lexicographically, and tested every min/max survivor. That experiment remains a valid throughput measurement, but it discarded the principal benefit of a topology-preserving DDA—**front-to-back closest-hit termination**. T1.6 reopens only this missing hypothesis. It does not reinterpret T1.5 as an end-to-end performance pass.

## 1. Query contract

The paper/rendering query is closest hit along increasing world-ray parameter $t$, not enumeration of every represented-surface intersection. Candidate and baseline must return the same closest-hit owner and coordinates under the frozen tolerance.

On every accepted T1 interval, $u(h)$, $v(h)$, and world-ray $t(h)$ have certified fixed derivative signs. The separator certificate fixes the exact interleaving of grid events. Therefore:

- segments can be ordered by their conservative world-$t$ range;
- within a segment, DDA cells can be emitted in exact curve-topology order;
- event-owner cells are processed as a closed batch before advancing the frontier; and
- after a hit at $t_*$, traversal may stop only when every unprocessed cell has a conservative lower bound strictly greater than $t_*$ plus the frozen tie tolerance.

The endpoint chord supplies event order, not exact event height. For adjacent chord separators $s_{i-1},s_i$, the exact event satisfies $e_i\in[s_{i-1},s_i]$. These brackets induce conservative cell-height spans and hence conservative future-$t$ lower bounds. Overlap can delay termination but may never permit an unsafe early stop.

## 2. O0 — ordered CPU/oracle study

Before CUDA changes, reconstruct an ordered traversal stream from the frozen T1 segments without using exact event roots for runtime decisions.

For each practical ray:

1. retain accepted segments and monotone-interval signs rather than merging them into a global cell dictionary;
2. emit alternating open-region and closed-event-owner batches from the chord event stream;
3. orient batches by increasing world $t$ using the certified sign of $t'(h)$;
4. associate every batch/cell occurrence with its T1 conservative height span;
5. map that span to a conservative world-$t$ interval using the rational $t(h)$ endpoint range and outward widening;
6. process cells in order, apply the existing leaf min/max test, and update the closest exact A2 hit belonging to that leaf; and
7. terminate only when the minimum lower bound over the unprocessed suffix is greater than the current closest hit under closed-owner tolerance.

Three schedules are compared:

- **ALL:** current no-termination schedule;
- **IDEAL:** exact T0 event-height spans, used only as an upper-bound oracle; and
- **SEP:** current separator-derived conservative spans, used by the realizable candidate.

Report by split, shell, family, asset, and hit/miss status:

- cells fetched and min/max survivors before termination;
- exact leaf triangles tested;
- index of the first correct closest hit;
- additional cells caused by separator-span overlap (`SEP - IDEAL`);
- rays on which conservative overlap prevents any early stop;
- duplicate closed-owner work; and
- closest-hit identity/coordinate failures.

O0 correctness gates:

- zero closest-hit owner or coordinate mismatch against the passing exact A2 oracle;
- zero unsafe stop under exact T0 audit;
- zero omitted closed edge/corner owner;
- reverse traversal produces the corresponding far-to-near stream; and
- no exact event root participates in the SEP acceptance or stop decision.

O0 continuation gates:

- on held-out ordinary hit rays, SEP exact-leaf tests are at most `0.75×` ALL;
- SEP retains at least `80%` of the IDEAL early-termination saving;
- at least two ordinary hit families save at least `20%` of exact-leaf tests;
- no ordinary family performs more leaf tests than ALL; and
- miss rays perform exactly the expected full traversal, with no fabricated early-exit benefit.

Failure means DDA order is mathematically available but the current conservative brackets prevent it from producing enough closest-hit work reduction. Passing authorizes O1 only.

## 3. O1 — fair ordered CUDA envelope

If O0 passes, implement two closest-hit device kernels on identical buffers.

### Candidate

- consume the ordered T1 schedule and conservative suffix world-$t$ lower bounds;
- test closed event-owner batches before advancing the stop frontier;
- retain only the closest hit and terminate by the certified suffix bound; and
- initially keep schedule construction free, exactly as an optimistic envelope.

### NRT-QT baseline

- attach an outward conservative world-$t$ interval to every surviving quadtree task;
- process the task with the smallest lower bound first, using a bounded priority structure or an equivalently audited ordered stack;
- terminate only when the queue minimum is behind the closest hit; and
- use the identical exact cubic leaf test and closest-hit tie rule.

Timing uses the frozen A2 development/decision split, 32,768 records per cohort, randomized paired trials, checksum equality, CV below `5%`, and device-resource reporting.

O1 continuation gates:

- zero closest-hit checksum mismatch and zero unsafe early stop;
- median candidate/NRT time ratio at most `0.80` on held-out ordinary rays;
- median ratio at most `0.90` on S1-moderate and S2-stress;
- no held-out ordinary family above `1.10`; and
- at least `20%` timing headroom remains before charging separator construction.

Only an O1 pass authorizes a full on-device separator/segmentation implementation. External TFDM/RMIP/Ogaki performance comparisons and renderer integration remain blocked until that full method passes.

## 4. If conservative intervals are the limiter

Do not silently use chord entry/exit heights: they are not conservative for the exact rational ray. Pose a separate tightening ablation using the boundary polynomial

$$
P_i(h)=N_x(h)-\frac{k}{R}D(h).
$$

Because the coordinate is monotone, the event root is unique. Starting at the chord crossing $\hat e_i$, a derivative lower bound gives

$$
|e_i-\hat e_i|\le \frac{|P_i(\hat e_i)|}{\min_I |P_i'(h)|},
$$

or one outward interval-Newton step can contract the bracket. Measure the added predicates against reduced termination delay before treating this as part of the method. This is an event-local root bracket, not repeated nonlinear range solving at every hierarchy node.

## 5. Claim boundary

Until O1 passes, safe language is limited to: topology-preserving chords provide a certifiable front-to-back cell order, while the practical benefit of conservative closest-hit termination remains under test. T1.5's unordered timings must not be cited as the final performance of ordered DDA.

## 6. Decision log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-30 | Reopen the closest-hit hypothesis after T1.5-A. | T1.5 merged and lexicographically sorted cells and never used hit-based termination, so it did not measure the intended DDA-order advantage. |
| 2026-08-30 | Require an ordered CPU oracle before another CUDA kernel. | It cheaply separates potential early-exit savings from delay caused by conservative event brackets. |
| 2026-08-30 | Give NRT-QT the same closest-hit semantics. | Comparing ordered candidate traversal against an all-hit or unordered baseline would not isolate the method fairly. |
| 2026-08-30 | Stop before O1 with the current separator spans. | Correct ordered termination saves only 7.1% of held-out ordinary leaf tests and retains 15.4% of the ideal event-height saving, failing the frozen O0 gates. |
| 2026-08-30 | Reopen O1 after the separately frozen O0.5 contraction passes. | `IN1` recovers all IDEAL leaf-test saving on held-out ordinary rays with zero root omission and about 1% uncontracted events. |
| 2026-08-30 | Accept the O1 CUDA envelope and authorize full on-device construction. | The fair ordered closest-hit comparison passes every checksum/resource/timing gate; held-out ordinary candidate/NRT is `0.5360×`. |

## 7. O0 result

Authoritative run: `ray-ordered-o0-972a8b4bd571` over all 864 practical rays. Every ray has one practical monotone interval with $t'(h)<0$, so the certified front-to-back direction is descending shell height. All closest-hit coordinates/owners match, all closed owners are present, all reverse streams match, and miss rays perform the full traversal.

On the 144 held-out ordinary hit rays:

| schedule | min/max fetches | exact leaf-triangle tests | leaf ratio to ALL |
|---|---:|---:|---:|
| ALL, no early stop | `1106` | `592` | `1.000×` |
| IDEAL exact event spans | `624` | `320` | `0.5405×` |
| SEP current separator spans | `710` | `550` | `0.9291×` |

The topology order has substantial potential: exact event spans would remove `45.9%` of exact-leaf work. Current separator spans remove only `7.1%`, retaining `15.4%` of the ideal saving. SEP stops early on `52.1%` of held-out ordinary hits versus `70.8%` for IDEAL, but its overlapping height spans trigger a median two and p95 four additional leaf-triangle tests per ray relative to IDEAL.

Ordinary-family SEP/ALL leaf ratios are:

- front hit: `1.000×` (already one cell);
- grazing hit: `0.845×`;
- near miss: `0.949×`; and
- oblique hit: `1.000×`.

Grid-corner hits, outside the frozen ordinary cohort, reach `0.723×`. The 21 held-out true miss rays correctly remain at `1.000×` because closest-hit termination cannot help a miss.

The O0 gate fails: the primary ratio is not `≤0.75×`, no two ordinary families save at least 20%, and SEP retains far less than 80% of ideal savings. Do not build the ordered CUDA envelope with the current spans.

This is a sharper and more favorable diagnosis than the unordered T1.5 result. DDA ordering is not the missing idea; **event-location tightness is the missing mechanism**. The next permissible hypothesis is the explicitly separate event-local contraction in Section 4. It should first measure derivative-bound or one-step interval-Newton bracket width, predicate cost, retained ideal early-exit saving, and closest-hit correctness on this same frozen O0 corpus.

## 8. O1 ordered CUDA envelope result

O0.5 subsequently supplied the missing event-location mechanism and passed its independent gates. The authoritative CUDA envelope is `ray-o1-closest-b35fd77510db` on an RTX 5090. Both candidate and fair ordered NRT-QT use the identical exact cubic leaf solver, closest-hit tolerance/owner rule, packed surface data, 32,768 records per cohort, and 50 paired GPU-event trials. NRT-QT uses a bounded depth-first hierarchy stack whose children are sorted by conservative world-ray $t$; it has zero overflow and zero unresolved output.

All candidate and NRT closest-hit checksums match the Mode 0 oracle across the full corpus, including grid-corner and proxy-edge cohorts. All timing CVs are below `1.4%`. Key median timings are:

| cohort | ordered IN1 schedule | ordered NRT-QT | candidate/NRT | speedup |
|---|---:|---:|---:|---:|
| decision ordinary | `56.15 ns/ray` | `104.75 ns/ray` | `0.5360×` | `1.87×` |
| decision S1 moderate | `58.40 ns/ray` | `100.97 ns/ray` | `0.5784×` | `1.73×` |
| decision S2 stress | `60.57 ns/ray` | `116.45 ns/ray` | `0.5201×` | `1.92×` |
| all rays | `48.72 ns/ray` | `204.88 ns/ray` | `0.2378×` | `4.20×` |

Held-out ordinary-family ratios are front `0.1325×`, grazing `0.7249×`, near-miss `0.4673×`, and oblique `0.5639×`; none exceeds the frozen `1.10×` cap. The candidate uses 208 bytes of reported local memory versus 1,424 bytes for NRT, with both kernels reporting four active blocks per SM in this build.

This is a positive traversal result, but it is an optimistic envelope: CPU-precomputed monotone segmentation, event labels, `IN1` brackets, DDA order, and suffix-$t$ bounds are uploaded with each schedule and are excluded from candidate timing. Therefore it supports the mechanism claim that contracted ordered DDA can beat fair ordered NRT-QT after construction. It does **not** yet support an end-to-end method or external TFDM/RMIP/Ogaki superiority claim. The next authorized stage is the full on-device construction and traversal ablation described in [[Plan — O2 on-device ordered shell DDA]].
