---
title: Plan — O0.5 event-local height contraction
tags: [plan, ray-tracing, DDA, closest-hit, interval-newton, event-brackets]
status: passed-o1-authorized
created: 2026-08-30
updated: 2026-08-30
parent: "[[Plan — T1.6 ordered closest-hit shell DDA]]"
---

# Plan — O0.5 event-local height contraction

> [!abstract] Purpose
> Ordered O0 proves that exact event spans would reduce held-out ordinary closest-hit leaf work to `0.5405×`, while current separator spans reach only `0.9291×`. O0.5 tests the missing mechanism before a GPU port: **can each already-certified grid event receive a tight conservative height bracket using constant event-local arithmetic?**

## 1. Fixed mathematical contract

For a boundary event with axis numerator $N_x(h)$, denominator $D(h)$, grid boundary $k/R$, and current separator bracket $I=[a,b]$, define

$$
P(h)=N_x(h)-\frac{k}{R}D(h).
$$

The T1 topology certificate already proves that the exact event is unique inside $I$. O0.5 contracts $I$ without using the exact T0 root in any runtime decision.

The fixed primary method is one interval-Newton step (`IN1`). With midpoint $m$ and an outward interval enclosure $P'(I)$ that excludes zero,

$$
I' = I \cap \left(m-\frac{P(m)}{P'(I)}\right).
$$

If $0\in P'(I)$, division is unresolved, or the intersection is empty, retain the original separator bracket and count an uncontracted event. Multi-axis corner events contract each proportional boundary polynomial and intersect their results. Empty intersection is a counted failure, never accepted.

Reference arithmetic uses Decimal evaluation of the frozen binary64 coefficients followed by outward binary64 conversion. This establishes the mathematical target for a later outward device implementation; Decimal is not part of the proposed runtime.

Two ablations are reported but cannot replace the fixed primary after seeing decision data:

- `MVT`: chord-centered mean-value radius $|P(\hat e)|/\min_I|P'|$;
- `IN2`: two interval-Newton steps, an optimistic arithmetic/tightness variant.

Exact T0 event brackets remain validation and the `IDEAL` upper bound only.

## 2. Rebuilding ordered traversal spans

For every accepted T1 segment:

1. preserve the chord event order and closed event-owner batches;
2. replace each separator event bracket by the contracted bracket;
3. rebuild conservative open-cell and event-owner height spans using the same ownership construction as T1;
4. merge only repeated intervals for the same leaf without changing first DDA occurrence order;
5. map height spans to outward conservative world-$t$ lower bounds; and
6. run the O0 closest-hit stop rule.

The segmentation and event order are frozen. O0.5 changes only event-height tightness.

## 3. Corpus and reported work

Use all 864 practical rays and the frozen A2 development/decision split. Report by method, split, shell, family, asset, and hit/miss status:

- contracted width divided by original separator width;
- contracted width divided by exact T0 bracket/hull where meaningful;
- polynomial/derivative interval evaluations per event;
- derivative-zero, empty-intersection, and uncontracted counts;
- min/max fetches, survivors, and exact leaf-triangle tests before closest-hit termination;
- early-stop fraction and additional tests relative to `IDEAL`; and
- fraction of the ideal early-termination saving retained.

## 4. Frozen gates for the fixed `IN1` method

Correctness:

- zero omission of an independently evaluated high-precision boundary-polynomial root;
- every contracted bracket overlaps the independent T0 root enclosure (a tighter enclosure is not required to contain the entirety of the older enclosure);
- zero closest-hit owner/coordinate mismatch;
- zero unsafe early stop;
- zero closed-owner omission or reverse-order mismatch; and
- miss rays retain full traversal behavior.

Structural continuation:

- no more than `5%` of practical events remain uncontracted;
- on held-out ordinary hit rays, exact leaf-triangle tests are at most `0.70×` `ALL`;
- `IN1` retains at least `80%` of `IDEAL` leaf-test savings;
- at least two ordinary hit families save at least `20%` versus `ALL`; and
- no ordinary family performs more work than current `SEP`.

Passing authorizes the ordered closest-hit CUDA implementation in T1.6 O1. Failing stops the current event-local realization; GPU DDA engineering cannot recover a mathematical frontier bound that remains too wide.

## 5. GPU continuation if O0.5 passes

Port in ordered checkpoints:

1. outward boundary-polynomial and derivative interval primitive;
2. one-step interval-Newton event bracket with CPU differential vectors;
3. lazy monotone segment/event generation;
4. incremental zero-radius DDA with closed boundary batches;
5. leaf min/max and exact cubic closest-hit update;
6. next-event conservative world-$t$ termination;
7. explicit fallback for poles, turns, unresolved intervals, or capacity; and
8. fair NRT-QT closest-hit traversal with conservative node world-$t$ ordering.

GPU ablations must include no early stop, original separator spans, contracted spans with precomputed schedules, and the full on-device method. Only the final variant supports an end-to-end method claim.

## 6. Decision log

| Date | Decision | Reason |
|---|---|---|
| 2026-08-30 | Fix `IN1` before evaluating decision data. | It offers strong local contraction with one polynomial value and one derivative interval; MVT and IN2 remain explanatory ablations. |
| 2026-08-30 | Hold segmentation and topology fixed. | O0 already isolates event-span width as the loss; changing multiple mechanisms would obscure causality. |
| 2026-08-30 | Require 80% of ideal saving. | Ordered O0 exposes a large `0.5405×` upper bound, so the contraction must recover most of it before paying GPU construction cost. |
| 2026-08-30 | Accept `IN1` and authorize O1. | The full frozen corpus has zero Decimal-root omission, only `48/4777` uncontracted events, and held-out ordinary leaf work exactly matches the IDEAL schedule. |

## 7. O0.5 result

Authoritative run: `ray-event-o05-d1747e69d975` over all 864 practical rays and 4,777 certified grid events. `IN1` evaluates 4,825 polynomial/derivative records (the excess is from closed multi-axis corner labels). Only 48 events remain uncontracted or empty, about `1.0%`, below the frozen `5%` limit. Independent Decimal boundary-root auditing reports zero omissions, and every contracted bracket overlaps the independent T0 enclosure.

The median contracted/original width ratio is `0.0001754`; p95 is `0.0062387`. On the 144 held-out ordinary hit rays, exact leaf-triangle tests fall from `592` for ALL and `550` for the original separator spans to `320` for `IN1`, exactly matching the IDEAL event-span oracle (`0.54054×` ALL) and retaining `100%` of the ideal saving. Ordinary-family IN1/ALL ratios are front `1.000×`, grazing `0.427×`, near-miss `0.519×`, and oblique `0.507×`. Grid-corner and proxy-edge rays remain deliberately harder because unresolved closed-boundary events retain their original brackets.

All correctness and structural gates pass. This result establishes that one local interval-Newton contraction recovers the front-to-back opportunity exposed by O0 without solving nonlinear range equations at every hierarchy node. It authorizes the ordered CUDA envelope, but it is not a GPU performance result by itself.
