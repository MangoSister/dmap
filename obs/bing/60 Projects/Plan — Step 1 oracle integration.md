---
title: Plan — Step 1 oracle integration
tags: [plan, step-1, ray-tracing, oracle, exact-intersection, experiment]
status: complete
created: 2026-08-23
updated: 2026-08-23
parent: "[[Plan — Step 1 ray reference experiment]]"
implementation-status: complete
input-suite: rays-935162cc636e
---

# Plan — Step 1 oracle integration

> [!abstract] O1 decision
> O1 attaches exhaustive intersection truth to the frozen D2 inputs before any hierarchy method sees them. It enumerates every analytic height microtriangle, groups boundary-equivalent roots, validates constructed roots, and persists resumable per-case annotations. It reports correctness and corpus composition only—no first-order benefit, traversal count, or timing claim.

## 1. Fixed input and non-goals

The only accepted ordinary input is D2 suite `rays-935162cc636e`, configuration hash `935162cc636e3181fd9229032404fdaa3989379bcf5c238abbeff9f12e820197`, ray-stream hash `0073118bbea522bb44998de5b3794944609af10c928f9e94c71ea7037d6d7243`. O1 must recheck those values before computing a root.

O1 does not build a hierarchy, segment a rational curve, run `G-MM/G-FO/L-CW/L-DIR`, time a baseline, or change a ray after observing its result. It reuses the exact analytic surface already frozen by D1/D2.

## 2. Oracle equation and outputs

For each ray, form its rational shell coordinates

$$q(h)=(U(h)/D(h),V(h)/D(h),h).$$

For every fixed-diagonal affine height microtriangle with plane

$$\alpha u+\beta v+\gamma h+\kappa=0,$$

substitution produces the existing cubic

$$G(h)=\alpha U(h)+\beta V(h)+\gamma hD(h)+\kappa D(h)=0.$$

Isolate all real roots over the complete grid height range, retain only roots satisfying the ray range, proxy barycentrics, and microtriangle ownership, and recover world-ray $t$. This enumerates all `2*cells²=2048` leaves for every 32-cell ray and shares no hierarchy rejection predicate.

Persist both raw owner hits and grouped physical roots. A group may combine multiple microtriangle owners only if its members agree in $t,h,u,v$ within the frozen CPU comparison tolerances and the UV point belongs to the corresponding shared edge/vertex ownership class. Do not merge merely because two heights are close.

Each oracle annotation records:

```text
ray_id, case_key, input_sequence_index,
raw_owner_hit_count, physical_root_count,
closest_t, closest_h, closest_uv, closest_owner_class,
measured_incidence_degrees,
min_abs_D, denominator_root_in_height_range,
root_groups[{t,h,u,v,a,b,owners,owner_class}],
constructed_root_expected, constructed_root_present,
constructed_root_t_error, high_precision_checked,
high_precision_polynomial_count, high_precision_mismatch
```

Measured incidence uses the exact represented-surface tangents $X_u,X_v$ from D2, including the affine height gradient. It must not use only constant-height shell tangents.

## 3. Constructed-root expectations

- `F00-aimed` and `F02-grazing`: the declared target root must be present, although it need not be closest.
- `F03-boundary`: exact and interior surface targets must be present; the marked exterior proxy continuation has no expectation.
- `F04-origin-range`: surface/inside-shell targets must be present. The `near-t-min/max` members differ from the constructed root by one binary64 `nextafter` step, which is smaller than the production float root error and the legacy solver's `2e-9` validity pad. O1 therefore records these as `range_boundary_ambiguous` structural cases and verifies their construction relation independently; it does not pretend a rounded float root can decide inclusion. A later certified range predicate or declared fallback must resolve them before they enter an ordinary method-correctness aggregate.
- `F01-free` and `F05-near-miss`: no hit/miss expectation is imposed.

A constructed-root failure is a generator/oracle witness and blocks method comparison. It cannot be removed from the corpus.

## 4. Boundary-equivalent grouping

For a candidate group, compare $t$ with `max(1e-10, 64 ulp(t))` and compare $h,u,v$ with a scale-aware 64-ulp floor. Then verify ownership geometrically:

- same microtriangle owner: one physical root;
- different owners: the common UV point must lie on both closed microtriangles within the same tolerance; and
- unrelated owners or separated UV points: keep separate even if $t$ is close.

Store every raw owner in the group. The grouped closest root is ordered by $t$; ties use `(h,u,v,owner)` only for deterministic persistence.

## 5. Independent high-precision cross-check

Check two frozen rays per case (90 total): one ordinary aimed/grazing ray and one boundary/origin-range ray selected by family-local index before results. Reuse the existing 80-digit Decimal cubic path, enumerate every leaf, group with the same geometric rule, and compare the complete physical root set—not only the closest hit.

Every later minimized failure also receives the high-precision check. Dense samples are not a root oracle.

## 6. Resumable lifecycle

Use a new annotation schema `step1-oracle-v1` and an immutable logical run ID derived from the input hashes, oracle version, tolerances, and high-precision selection. Write a `running` manifest before work. Each of the 45 cases is an atomic shard containing 256 annotations plus a case summary. Resume only a hash-valid completed shard whose configuration and input case hash match. Final publication merges shards in frozen case/ray order and records hashes for every stream.

Layout:

```text
experiments/step1_oracle/<run_id>/attempts/aNNNN/
  run_manifest.json
  case_shards/<case_key>.jsonl.gz
  high_precision_shards/<case_key>.jsonl.gz
  failures/<failure_id>.json
  oracle_annotations.jsonl.gz
  case_summaries.jsonl.gz
  result.json
```

Preserve partial/failed attempts. Never overwrite a complete attempt. Reuse the atomic-write, hash, checkpoint, and resume semantics already tested in `step1_run_lifecycle.py`; do not route O1 through the scientific `ray` schema with fake method fields.

The float64 corpus and the slower 80-digit subset are two linked immutable logical runs, not a mutation of a completed float attempt. The high-precision configuration records the completed float annotation hash and input-suite hashes; the final O1 gate table joins them by `ray_id`. This keeps a complete attempt immutable while retaining exact provenance.

## 7. Staged execution and budget gate

1. Adapter tests: input hashes, ray reconstruction, exact case/grid/shell association, and round-trip annotation records.
2. One-case benchmark on `P03-sine-bump × S02-stress`, all 256 rays, float64 oracle only. Wall time is an execution-budget measurement, not a paper result.
3. If projected full float64 time is at most 45 minutes, run/resume all 45 cases. Otherwise optimize only result-independent enumeration/caching—specifically cached leaf planes and conservative omission of texture microtriangles whose closed UV domain does not overlap the proxy triangle—demonstrate root-set equivalence to the original exhaustive loop on the benchmark, and remeasure.
4. Run the frozen 90-ray high-precision subset after all float64 shards complete.
5. Independently reopen every stream and verify hashes, counts, references, sequence, grouping closure, and target expectations.

No hierarchy integration starts while O1 has an unresolved failure.

## 8. Tests and O1 gate

1. Input suite/configuration/case/ray hashes and counts match D2 exactly.
2. Record order and IDs are stable across repeated one-case runs.
3. Raw hits close exactly into stored groups; no raw owner is lost or duplicated.
4. Synthetic interior, shared-edge, shared-vertex, two-close-root, multiple-root, miss, and ray-range cases group correctly.
5. The exact represented-surface normal used for measured incidence matches a finite-difference diagnostic away from edges.
6. Every ordinary expected constructed root is present. Every one-ULP `F04` range case has the correct recorded construction relation and is classified as the declared structural ambiguity rather than silently passed or failed by a padded float predicate.
7. All 90 high-precision root sets match the float64 grouped root sets.
8. Case interruption/resume produces byte-identical final annotations to uninterrupted execution.
9. Final output has 45 case summaries and 11,520 annotations with unique input references and contiguous sequence.
10. All existing 61 P1–D2 tests remain unchanged and pass.

**Gate O1:** all ten checks pass with zero unresolved ordinary target-root, grouping, or high-precision mismatch, and every one-ULP range case is explicitly accounted for. O1 then authorizes planning—not yet running—the fixed `G-MM` versus `G-FO` traversal integration on these exact annotations.

> [!note] O1 benchmark amendment — 2026-08-23
> The first frozen benchmark case took 65.64 s, projecting to roughly 49.2 min for 45 cases, so the result-independent caching/proxy-overlap optimization above is authorized by the predeclared budget gate. The same run reported four apparent excluded-range failures, all from `F04` one-ULP bounds: the legacy `valid_at` pad is orders of magnitude larger and the recovered $t$ differs by roughly `9e-15–1.1e-14`. These are now honestly classified as structural range ambiguity; thresholds and ordinary ray geometry are unchanged.

> [!note] Boundary-incidence adapter amendment — 2026-08-23
> The first parallel v1 attempt preserved two completed cases, then failed while annotating a valid proxy-edge root: root ownership accepts the existing `2e-9` closed-domain tolerance, while the D2 surface-normal helper reapplied an eight-ulp proxy-inside test. For measured incidence only, O1 v2 evaluates the hit's canonical microtriangle affine continuation at the recovered UV. This does not admit a root, alter grouping, or change ray geometry; it merely makes the diagnostic normal use the same boundary owner already accepted by the oracle. The failed v1 attempt remains immutable and v2 receives a new logical run ID.

## 9. Paper use

O1 supplies corpus facts only: hit/miss balance, multiple-root frequency, incidence strata, boundary/root-conditioning coverage, and oracle correctness. It cannot support a speed or pruning claim. The eventual paper should cite it as the independent truth path and report its zero-mismatch gate, while leaving its CPU wall time to supplemental reproducibility notes.

## 10. O1 completion — 2026-08-23

O1 passes for the frozen ordinary corpus, with the declared structural range exception accounted for.

Float64 run `oracle-dd072597b14a/a0001`:

- configuration SHA-256 `dd072597b14a361af64ed24afe01e4ab55b7e44f8b44b12e887791654921420d`;
- 45/45 atomic case shards and 11,520/11,520 unique annotations;
- 9,822 hit rays, 1,698 misses, and 3,902 rays with multiple physical roots;
- 29,300 raw owner hits closed exactly into 25,387 physical roots;
- zero ordinary constructed-root expectation failures;
- exactly 360 declared one-ULP `F04` range ambiguities; and
- annotation SHA-256 `b8a05ab6afd954484ec3517ab2731ea8ae51abc9820db82437183426e3f44453`.

The four-worker run took 281.72 s wall time; summed case compute was 1,080.23 s. These are execution-budget values, not paper performance.

Linked 80-digit run `oracle-hp-791061ae1b7d/a0001`:

- configuration SHA-256 `791061ae1b7d0b702b8b6e8044be892da04b1c027d37b61619e9aee90333d216`;
- two preselected rays per case, 90 total;
- 100,980 independently isolated cubic polynomials;
- 255 float root groups and 255 Decimal root groups;
- zero complete-root-set mismatches; and
- annotation SHA-256 `1e5a18c4a8b66108afb7f5095b5cad08aea5e7e6ef8ed9ed555cc9db52bdc995`, linked to the float annotation hash above.

Independent final readback verifies hashes, contiguous input sequence, unique references, case closure, root order, raw-owner/group closure, non-null hit incidences, target expectations, selection indices, and all 45 case links. The complete P1–O1 regression suite has **70 passing tests**.

The preserved failed v1 attempt is diagnostic history, not a result. It stopped after two valid shards at the boundary-incidence adapter mismatch described above. O1 v2 fixed only diagnostic-normal evaluation and reran every case under a new logical ID.

---

Related: [[Plan — Step 1 controlled ray generator]] · [[Plan — Step 1 persistence and instrumentation]] · [[Plan — Step 1 ray reference experiment]]
