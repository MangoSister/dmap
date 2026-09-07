---
title: Plan — Step 1 persistence and instrumentation
tags: [plan, step-1, persistence, instrumentation, jsonl, reproducibility, ray-tracing]
status: active-p2-next
created: 2026-08-23
updated: 2026-08-23
parent: "[[Plan — Step 1 ray reference experiment]]"
implementation-status: p1-schema-complete
---

# Plan — Step 1 persistence and instrumentation

> [!abstract] Purpose
> Add deterministic, validated experiment records around the existing CPU reference without changing its mathematical predicates. This is the first implementation unit of Step 1. It ends when the current smoke cases can be written, read, validated, and reproduced with unchanged correctness/work counters. It does **not** add the new dataset, new ray families, plots, DDA, or CUDA.

## 1. Scope

### In scope

- Versioned schemas for run, map, case, node, ray, method, summary, and failure records.
- Stable IDs derived from canonical experiment inputs.
- Deterministic JSONL/GZIP writers and streaming readers.
- Optional traversal observers that record existing decisions and counters.
- A thin experiment runner around `prototype_first_order_ray.py`.
- Validation, resume safety, failure capture, and regression tests.
- One persisted execution of the existing development smoke cases.

### Out of scope

- New displacement-map loaders or crop logic.
- The 256-ray family and held-out/fuzz manifests.
- Changes to event isolation, curve certificates, node folds, or leaf roots.
- Publication plots and go/no-go evaluation.
- Parallel execution, GPU profiling, DDA, or external baselines.

## 2. Design constraints

1. **No predicate duplication.** The runner calls the existing authoritative functions.
2. **No hidden notebook state.** The command-line run is authoritative; notebooks only read records later.
3. **No silent schema drift.** Every record carries `schema_version="step1-v1"`.
4. **No NaN or Infinity in JSON.** Non-finite quantities become `null` plus an explicit status/reason field.
5. **No silent overwrite.** A completed run directory is immutable. Resume is allowed only when its configuration hash matches.
6. **No absolute-path identity.** IDs use logical dataset/source IDs and content hashes, not machine-specific paths.
7. **No timing claim.** Wall-clock fields exist for experiment budgeting only.
8. **No counter changes by observation.** A no-op observer and recording observer must produce identical hits and aggregate counters.

## 3. Proposed files

| File | Responsibility |
|---|---|
| `scripts/step1_experiment_schema.py` | Schema version, canonical JSON, stable IDs, validators, JSONL/GZIP streaming I/O |
| `scripts/run_step1_ray_reference.py` | CLI orchestration, run lifecycle, current-case adapter, summaries, failure exit code |
| `scripts/test_step1_experiment_schema.py` | ID, validation, round-trip, deterministic-record, resume, and non-finite tests |
| `scripts/test_step1_instrumentation.py` | Observer/no-observer equivalence and current smoke regression |
| `experiments/step1_ray_reference/README.md` | Generated-data policy and exact reproduction command; no large run data committed by default |

The existing `prototype_first_order_ray.py` remains the authoritative algorithm. `first_order_ray_core.py` remains the notebook-facing analysis layer and does not become the persistence layer.

## 4. Record model

### 4.1 Run manifest

One `run_manifest.json` contains:

```text
schema_version, run_id, state, configuration_hash,
started_utc, completed_utc, elapsed_seconds,
command_argv, working_directory_logical,
git_commit, git_dirty, python_version, numpy_version, platform,
input_file_hashes, seeds, expected_case_ids,
completed_case_ids, failure_count, record_counts
```

`state` is one of `running`, `complete`, or `failed`. The initial manifest is written with `running`; finalization atomically replaces it with `complete` or `failed`.

### 4.2 Data records

The complete field contract remains in [[Plan — Step 1 ray reference experiment#9. Persisted data and metric schema]]. This implementation unit adds the following common envelope to every JSONL record:

```text
schema_version, record_type, run_id, record_id, case_id, sequence_index
```

Record types:

- `map` — one per generated height grid, identified by `grid_id` so resolution sweeps cannot collide;
- `case` — one per map/shell/parameter combination;
- `node` — one per hierarchy node;
- `ray` — one common ray/oracle record with a nested `methods` object;
- `failure` — a lightweight reference to the complete file in `failures/`; and
- `case_summary` — exact totals and quantiles derived from the per-ray records.

Method keys are frozen as `G-MM`, `G-FO`, `L-CW`, and `L-DIR`. Missing or not-yet-run methods use `status="not_run"`; fields are not fabricated as zero.

## 5. Stable identity and canonicalization

### 5.1 Canonical JSON

Hash inputs are UTF-8 JSON with:

- keys sorted lexicographically;
- separators `(',', ':')`;
- `ensure_ascii=False`;
- integer values preserved as integers;
- floats encoded by Python's shortest round-trippable representation; and
- no timestamps, output paths, host names, or process IDs.

### 5.2 IDs

- `configuration_hash = sha256(canonical_config_json)`.
- `run_id = "step1-" + configuration_hash[:12]`; repeated identical configurations share the logical run ID.
- `grid_id = <map_id>__n<cells>__<grid-hash8>`.
- `case_id = <map_id>__<shell_id>__n<cells>__e<eta-token>__h<thickness-token>__<case-hash8>`.
- `ray_id = <case_id>__<family>__r<zero-padded-index>`.
- `node_id = <case_id>__l<level>__x<ix>__y<iy>__s<size>`.
- `failure_id = sha256(canonical_failure_witness)[:16]`.

Decimal tokens replace `.` with `p` and `-` with `m`, so IDs remain path-safe.

## 6. Output lifecycle and recoverability

Default root:

```text
experiments/step1_ray_reference/<run_id>/
```

Lifecycle:

1. Resolve and validate the exact target beneath the declared experiment root.
2. If absent, create the run directory and a `running` manifest.
3. Write each stream to `<name>.jsonl.gz.partial`.
4. Flush and close a case at a time; update a small checkpoint containing completed case IDs and stream counts.
5. On success, validate every stream, rename partial files to final names, write `summaries.json`, and atomically finalize the manifest.
6. On failure, preserve partial files, write the witness, and finalize with `state="failed"`.

Resume rules:

- `--resume` requires an existing `running` or `failed` directory with exactly the same configuration hash and schema version.
- Complete cases are skipped only after their record counts and terminal case marker validate.
- A `complete` run is never modified. A rerun uses a separate physical attempt directory while retaining the same logical configuration hash.
- There is no `--force-delete` option.

GZIP streams use deterministic headers (`mtime=0`, empty logical filename) so record-stream bytes are repeatable for identical content. The run manifest itself legitimately differs in timestamps and elapsed time.

## 7. Instrumentation boundary

### 7.1 Observer interface

Traversal functions gain an optional observer with no-op default. The minimal event vocabulary is:

```text
on_valid_interval
on_global_segment
on_node_test
on_node_decision
on_local_linearization
on_split
on_candidate_leaf
on_exact_leaf_test
on_fallback
on_hit
```

Events carry immutable scalar/tuple snapshots, never live `Node`, `Segment`, or mutable NumPy arrays.

### 7.2 Predicate protection

- Predicates compute their result first.
- The observer receives that already-computed result.
- Observer return values are ignored.
- Exceptions from the observer abort the experiment rather than altering traversal.
- The default path allocates no event record.

The first instrumentation implementation should prefer wrapper-level reconstruction where possible. Modify a traversal predicate only when the required value does not otherwise exist, and keep that change to a single observer call after the decision.

### 7.3 Per-node visits

Stable node keys use `(level, ix, iy, size)`, not Python object identity. The recording observer accumulates visit counts separately for all four method variants. The static node record is emitted once after all rays in a case complete.

## 8. Derived summaries

Summaries are recomputed from persisted ray records, never incrementally trusted as a second source of truth.

For every case, family, method, and hit/miss stratum, store:

- sample and fallback counts;
- sums for nodes, leaves, segments, and linearizations;
- p50, p95, p99, and max using one frozen quantile definition;
- candidate omissions and hit mismatches;
- ratio-of-total paired comparisons; and
- minimum/maximum certificate diagnostics.

Use NumPy's `method="linear"` quantiles for Step 1 v1 and record that method in `summaries.json`.

## 9. Validation rules

Schema validation rejects:

- missing required keys;
- unknown method IDs or fallback reasons;
- non-finite JSON numbers;
- negative counts;
- a hit with null `hit_t`;
- an inactive method with fabricated work counts;
- a completed case without the expected ray count;
- record IDs inconsistent with their fields;
- duplicate `record_id` values; and
- summary totals that differ from the source ray records.

Cross-record validation checks that every ray references an existing case, every case references an existing map, every node references one case, and manifest counts match all streams.

## 10. Tests required before the smoke run

### 10.1 Schema and I/O tests

1. Canonical JSON and IDs are stable across dictionary insertion order.
2. JSONL/GZIP round-trip preserves every supported scalar/list/object field.
3. Record streams are byte-identical for identical inputs.
4. NaN and Infinity are rejected or normalized to `null` with status.
5. Duplicate records and broken references fail validation.
6. Resume rejects a mismatched configuration.
7. A complete run cannot be overwritten or resumed.
8. Interrupted partial output remains readable up to the last complete record.

### 10.2 Algorithm-equivalence tests

For the current default reference suite:

- no observer versus no-op observer versus recording observer produce identical oracle roots, hit classifications, candidate sets, aggregate counters, and fallback reasons;
- the existing command-line output and exit code remain unchanged when persistence is not requested; and
- the existing 384-ray/6,144-high-precision-polynomial gate remains at zero failures.

## 11. Implementation sequence

### P1 — pure schema module

- Implement canonical JSON, stable IDs, record validators, and deterministic JSONL/GZIP I/O.
- Test entirely with synthetic records.

**Gate P1:** all schema/I/O tests pass; no reference-algorithm file changed.

### P2 — current-summary adapter

- Adapt existing `RunSummary` and `details` output to versioned case/map/summary records.
- Do not add per-ray or per-node instrumentation yet.

**Gate P2:** the persisted totals exactly match current CLI totals for the development smoke set.

### P3 — observer and detailed records

- Add the no-op/recording observer boundary.
- Emit per-ray method records and stable per-node visits.
- Recompute summaries from those records.

**Gate P3:** observer equivalence tests pass and derived summaries equal current aggregate counters.

### P4 — run lifecycle

- Add run manifests, partial files, resume validation, failure witnesses, and finalization.
- Add the experiment-data README.

**Gate P4:** simulated interruption/resume succeeds without duplicate or missing cases.

### P5 — persisted smoke run

- Execute only the existing development cases.
- Validate all streams and archive the manifest/configuration hash.

**Gate P5:** zero correctness regressions, complete schema validation, and repeatable content records.

## 12. Completion criteria

This implementation unit is complete only when:

1. every P1–P5 gate passes;
2. no algorithmic certificate or traversal decision was intentionally changed;
3. the old non-persistence CLI remains usable;
4. a clean reader can reproduce every summary from record streams alone;
5. interrupted runs are recoverable without destructive cleanup; and
6. the next dataset-loader step can consume the schema without modifying it.

## 13. Review questions before implementation

1. Is JSONL/GZIP acceptable as the long-form storage format, or is there a strong reason to add a Parquet dependency now?
2. Should generated smoke records remain untracked by default while manifests/summaries are committed selectively? The recommendation is yes.
3. Do we want physical rerun attempts nested below one logical `run_id`, or sibling directories with an attempt suffix? The recommendation is nested attempts so the logical configuration remains obvious.

Unless changed during review, the recommended choices above become the implementation contract.

## 14. Progress record — 2026-08-23

Phase P1 is complete:

- added `scripts/step1_experiment_schema.py` with `step1-v1`, canonical JSON, stable configuration/case/ray/node/failure IDs, record and manifest validation, reference validation, deterministic JSONL/GZIP streams, and no-overwrite canonical JSON output;
- added `scripts/test_step1_experiment_schema.py` using the standard-library `unittest` runner, so no package installation was required;
- all 17 synthetic schema/I/O tests pass under the project Conda environment, including deterministic compressed bytes, truncated-record rejection, non-finite rejection, manifest-hash validation, cross-record references, and no-overwrite behavior; and
- the tests caught and fixed a decimal-token alias in which `10.0` initially collapsed to `1`.

No ray, hierarchy, traversal, certificate, or leaf file was changed in P1. The next implementation unit is P2, the current-summary adapter. Before P2 code, freeze the exact mapping from the existing `RunSummary`/`details` fields into `map`, `case`, and `case_summary` records.

## 15. P2 current-summary adapter plan — 2026-08-23

P2 persists existing aggregate evidence only. It does not add observer calls or per-ray/node records.

### 15.1 Stable grid identity

Add `grid_id` to map and case records:

```text
grid_id = <map_id>__n<cells>__<hash8 of map generator/source/crop/cells/seed>
```

This amendment is required before P2 because the resolution sweep contains the same logical `map_id` at multiple grid sizes. `record_id` equals `grid_id` for a map record. A case references both `grid_id` and the human-readable `map_id`.

### 15.2 Existing aggregate mapping

| Existing source | Persisted location |
|---|---|
| `RunSummary.rays`, `fallbacks`, `oracle_hits`, `oracle_intersections`, `multiple_hit_rays` | `case_summary.common` |
| `minmax_nodes`, `minmax_leaves`, min/max mismatches and omissions | `method_summaries.G-MM` |
| `taylor_nodes`, `taylor_leaves`, Taylor mismatches and omissions | `method_summaries.G-FO` |
| shared `segments` | both global method summaries, explicitly labeled a shared total |
| lazy methods | `status="not_run"`; no fabricated zeros interpreted as measurements |
| high-precision rays/polynomials/mismatches | `case_summary.correctness.high_precision` |
| maximum tube ratio and maximum $t$ errors | `case_summary.correctness` |
| independent/first-order box counts and omissions | `case_summary.diagnostics.world_boxes`; not a primary method result |
| `details.widths_by_level` | `case_summary.widths_by_level` |
| `details.world_boxes_by_level` | `case_summary.diagnostics.world_boxes_by_level` |
| `details.failure_records` | `case_summary.failure_records` in P2; separate witness files begin in P4 |

P2 summaries carry `aggregate_only=true`. They contain no p50/p95/p99 fields because those cannot be reconstructed from current totals. Quantiles begin only after P3 emits per-ray records.

### 15.3 Map descriptors

For current procedural grids:

- obtain the two exact affine gradients per cell from the frozen microtriangles;
- compute RMS and p95 over those gradient magnitudes;
- use the standard five-point interior discrete Laplacian with grid-spacing scaling;
- fit $h=c+g_uu+g_vv$ by least squares at all grid vertices and record residual RMS/$R^2$;
- compute high-frequency energy from the mean-removed 2D FFT, with radial frequency above one quarter of Nyquist; and
- define `high_gradient_cell_fraction` as the fraction of cells whose maximum microtriangle gradient is at least the global triangle-gradient p95.

These definitions become part of `step1-v1` and must be unit-tested on constant and exact-ramp grids.

### 15.4 Shell regularity in P2

For the triangle shell, $\det J_F(a,b,h)$ is linear in $(a,b)$ at fixed $h$, so its extrema over the proxy triangle occur at the three proxy vertices. At each vertex it is quadratic in $h$. Evaluate both height endpoints and any stationary point inside the interval, widen outward with `nextafter`, and combine all three vertex ranges.

Record:

- `shell_regular=true` only when the widened determinant interval excludes zero;
- `min_abs_det_j` from that widened interval; and
- extra field `shell_regularity_method="vertex-quadratic-float-widened"`.

This is a CPU certificate for the fixed polynomial shell, not a GPU directed-rounding claim.

### 15.5 P2 files and tests

- Add aggregate-adapter functions to `scripts/run_step1_ray_reference.py`.
- Add `scripts/test_step1_summary_adapter.py`.
- Do not change `prototype_first_order_ray.py`.

Tests must establish:

1. `grid_id` differs across cells or generator seed and remains stable otherwise.
2. Constant and ramp map descriptors have the expected zero curvature/plane residual.
3. Shell determinant bounds contain dense diagnostic samples and certify both current shells.
4. Every existing `RunSummary` field is either mapped or explicitly listed as diagnostic/not-yet-persisted.
5. A small real `run_case` produces persisted aggregate totals identical to its in-memory summary.
6. `L-CW` and `L-DIR` are marked `not_run`, not reported as zero-work results.

**Gate P2:** persisted totals exactly match the existing reference for the full development smoke set, and the unchanged reference correctness gate remains at zero failures.

### 15.6 P2 completion record — 2026-08-23

P2 is complete for the current development corpus.

- Added `scripts/run_step1_ray_reference.py`, which persists current procedural map, case, structural-check, and aggregate `RunSummary` evidence without changing the ray reference.
- Added `scripts/test_step1_summary_adapter.py`; together with the P1 suite, 26 tests pass.
- Executed run `p2-40a9c6e5e680`: seven procedural maps × two shells × 64 rays = 14 cases and 896 rays, with two independent high-precision oracle rays per case.
- All 14 summaries and their map/case references validate under `step1-v1`.
- There were zero candidate omissions, closest-hit mismatches, and high-precision mismatches. The maximum sampled componentwise tube ratio was `0.9980866686`, and the maximum hierarchy sampling violation was `3.28e-15`.
- Pooled over this deliberately small legacy ray suite, `G-FO/G-MM` was `0.9709` for node tests (14,277/14,705) and `0.8610` for accepted leaf-interval tests (1,146/1,331). These are aggregate mechanism counts only. They are not per-ray distributions, do not include the lazy methods, contain no real maps, and are not performance evidence.

Saved evidence is under `experiments/step1_ray_reference/p2-40a9c6e5e680/`. The P2 aggregate files are immutable inputs to the P3 equivalence test.

## 16. P3 detailed observer and record plan — 2026-08-23

P3 adds observation, not a new traversal algorithm. The existing `run_case` result is the authority for equivalence. The new detailed path must reproduce its `G-MM` and `G-FO` oracle outcomes, candidate sets, fallback count, and aggregate work counts exactly before any new result is interpreted.

### 16.1 Scope and non-goals

P3 uses only the current procedural maps, current flat/twisted shells, and current deterministic legacy ray generator. It does not add real displacement assets, new ray families, DDA, CUDA, timing claims, packing, or external baselines. Those begin only after the detailed reference records are trustworthy.

P3 does run the already implemented lazy reference variants, `L-CW` and `L-DIR`, so their per-ray mechanism counts can be persisted. This is a new measurement path, not a change to the existing default CLI.

### 16.2 Observer boundary

The geometry module receives an optional observer whose default is `None`. Predicates and counters compute first; immutable scalar/tuple snapshots are emitted afterward. Observer return values are ignored. The default path creates no event objects or record lists.

The recording adapter owns all schema knowledge. The geometry module must not import `step1_experiment_schema.py`. The minimum P3 events are:

| Event | Emission point | Required snapshot |
|---|---|---|
| valid interval | after event classification | interval index, `h0`, `h1`, point/open ownership |
| global segment | after tube acceptance | `h0`, `h1`, endpoint UV, componentwise radii |
| node test | immediately after incrementing the node counter | method ID and stable `(level,ix,iy,size)` key |
| node decision | after UV/height/leaf decision | reject reason or descend/leaf, interval, projected error if evaluated |
| local linearization | after node-local chord construction | method, interval, refinement depth |
| split | after split decision | UV or bow reason and split location |
| exact leaf test | immediately before the existing cell-level analytic leaf call | method, cell, interval |
| fallback | after the cause is known | stable reason code |
| hit | after hit sorting | closest result and total unique roots |

`build_segments` may expose its already-existing failure cause through `SegmentBuildCounters.fallback_reason`; this must not alter its existing `(segments, fallback)` return or acceptance predicates.

### 16.3 Frozen counting semantics

- `node_tests`: traversal entries, including repeated visits to the same node from different owned intervals or segments.
- `internal_nodes`: node tests for which `node.is_leaf` is false, regardless of the eventual reject decision.
- `candidate_leaves`: distinct `(ix,iy)` cells admitted at least once for that ray/method.
- `exact_leaf_tests`: admitted **cell–interval** tests. One such test evaluates the cell's two analytic microtriangles; it is not counted as two here.
- `global_segments`: accepted elements of the completed global event/tube partition. It is shared only by `G-MM` and `G-FO`.
- `local_linearizations`: every successful node-local endpoint-chord construction in a lazy method, including the root interval construction and constructions after clipping or splitting.
- `uv_splits` and `bow_splits`: split decisions, not the two children they create.
- `directional_bound_evaluations`: calls that actually evaluate the direct gradient-projected certificate.
- Per-node `visit_count_by_variant`: the same repeated node-test definition as `node_tests`; sums across node records must therefore reproduce the per-ray totals.

### 16.4 Frozen per-ray semantics

- `valid_interval_count` counts midpoint-valid open event intervals plus valid isolated point owners not covered by an open interval.
- `min_abs_D` is the conservative quadratic minimum magnitude over the declared displacement-height range; it is zero when that range contains a denominator root.
- `measured_hit_incidence` is `abs(dot(normalized(ray.direction), normalized(shell_normal)))` at the closest oracle hit: zero is grazing and one is normal incidence. Misses store `null`.
- `oracle_owner_class` is `interior`, `micro_edge`, or `micro_vertex`, classified from barycentric coordinates of the owning height microtriangle with the frozen root/ownership tolerance; misses store `none`.
- A primary-method fallback stores `status="fallback"`, `active=false`, `fallback_taken=true`, and the exact nonlinear oracle as the returned hit. Work completed before fallback remains counted; unexecuted traversal work remains zero.
- Fallback reasons are resolved in this order: interior denominator root, denominator conditioning/failed tube, event isolation, resource limit, then `oracle_requested`. Unknown causes are errors rather than silently labeled.
- If both oracle and method miss, `hit_t_error` and `hit_position_error` are `null`; if both hit, position error is `abs(t_method-t_oracle)*length(ray.direction)`.
- `eps_u_texels_max` and `eps_v_texels_max` are maxima over accepted chords. Projected-error maxima are taken only over node decisions where that certificate is evaluated; an unevaluated quantity is `null`, not zero.

### 16.5 Static per-node records

Node geometry is emitted once per case in stable depth/row/column order.

- `proxy_overlap_fraction` is the exact area fraction obtained by clipping the node UV rectangle against the proxy UV triangle; it is not sample-estimated.
- `exact_residual_min/max` are extrema of `H(u,v)-[h0+g·(uv-center)]` over every affine microtriangle covered by the node. Because the residual is affine per microtriangle, checking its three vertices is exact in real arithmetic; persisted values are widened outward.
- `residual_slack_ratio = 2r/(exact_max-exact_min)`. When the exact width is numerically zero, the ratio is `null` and an extra `constant_residual=true` flag is stored.
- `visited_by_any_ray` is true iff any method has a positive visit count. `all_node=true` is retained so later reports can explicitly contrast all-node and visited-node distributions.

### 16.6 Detailed adapter and summary derivation

Add a detailed case runner in `run_step1_ray_reference.py`; keep the P2 aggregate path callable and unchanged. The detailed runner:

1. builds the same grid, shell, hierarchy, box cache, and deterministic rays;
2. records the exhaustive analytic oracle and optional Decimal cross-check;
3. runs `G-MM` and `G-FO` with one shared global segment partition;
4. runs `L-CW` and `L-DIR` with identical ownership/refinement settings except for the projected-error certificate;
5. validates every ray record immediately;
6. derives case summaries only from the completed ray records; and
7. asserts the `G-MM`/`G-FO` derived totals against the untouched P2/legacy `RunSummary` fields.

Sequence indices and all output ordering are deterministic: maps, cases, static nodes, rays, then summaries each have their own monotonically increasing stream index. No runtime object identity or set iteration order may reach a persisted stream.

### 16.7 P3 tests

1. No observer, a no-op observer, and a recording observer produce identical segments, candidate sets, sorted hits, counters, and fallback outcomes.
2. `collect_records=false/true` leaves both lazy variants bitwise-equivalent in their returned hits and counters.
3. For every method and ray, the sum of decision classes is consistent with node tests; exact leaf events equal the frozen `exact_leaf_tests` count.
4. Summed per-node visits equal summed per-ray `node_tests` for each method.
5. Summaries derived from ray records exactly equal the legacy aggregate totals for `G-MM` and `G-FO` on the development smoke set.
6. Both lazy variants have zero candidate omissions and closest-hit mismatches on the smoke set; otherwise P3 records a witness and fails rather than hiding the method.
7. Static node residual extrema contain dense diagnostic samples, and proxy-overlap fractions pass analytic corner/edge cases.
8. Repeating the same detailed run produces byte-identical node and ray streams.
9. The old CLI without persistence retains its output, exit code, and zero-failure 384-ray/6,144-polynomial gate.

**Gate P3:** all nine tests pass; the development detailed stream validates; derived global-method summaries exactly match P2; per-node visit sums close exactly; and all four measured methods have zero correctness failures. Only then may P4 lifecycle work or the dataset/ray-family expansion begin.

### 16.8 P3 completion record — 2026-08-23

P3 passes its development gate.

- Added the optional scalar/tuple-only `TraversalObserver` boundary, stable node keys, internal-node counters, explicit global fallback reasons, and post-predicate events to `scripts/prototype_first_order_ray.py`. With `observer=None`, the original path constructs no event records.
- Added detailed per-ray/per-node conversion and all-four-method execution to `scripts/run_step1_ray_reference.py`. The P2 aggregate entry path remains separately callable.
- Added `scripts/test_step1_observer_equivalence.py` and `scripts/test_step1_detailed_adapter.py`. All 32 combined P1–P3 tests pass, including no-op/recording equivalence, lazy `collect_records` equivalence, analytic proxy-overlap cases, exact residual containment, per-node/per-ray count closure, legacy aggregate equality, and byte-deterministic detailed streams.
- The unchanged legacy CLI gate passes 384 rays and 6,144 independent high-precision polynomial checks with `failures=0`.
- Persisted run `p3-f29f4158c87f` contains 7 maps, 14 cases, 4,774 static nodes, 896 rays, and 14 summaries. Independent stream readback and cross-reference validation pass; all stream sequence indices are complete and monotone.
- Across all methods there are zero candidate omissions, closest-hit mismatches, or high-precision mismatches. Thirteen rays take the declared exact fallback, leaving 883 active rays per method.

Initial per-ray mechanism results for this **legacy procedural/mixed-ray suite only**:

- Fixed global segmentation remains cheap: global segments per active ray are p50=1, p95=1, p99=2, max=2.
- `G-FO/G-MM` pooled work is 0.9709 for node tests (14,277/14,705) and 0.8610 for exact leaf-interval tests (1,146/1,331).
- `L-CW` uses 14,057 node tests, 1,092 exact leaf-interval tests, and 3,444 local linearizations.
- `L-DIR` uses 14,055 node tests, 1,092 exact leaf-interval tests, and 3,440 local linearizations, while evaluating 5,300 direct directional bounds.
- Only one of 883 active rays uses fewer local linearizations under `L-DIR`; the other 882 are equal. Bow splits change from two to one and neither method makes a UV split.

Therefore the fixed first-order slab remains mechanically promising in this small suite, but the broad directional-lazy knot-reduction claim is **not supported by P3**. The illustrative directional-margin figure remains an explanation of a possible local mechanism, not empirical evidence of common savings. G4 is not formally decided because P3 lacks the frozen incidence families and real maps, but the current evidence raises the bar: the later suite must show benefits large enough to amortize directional-bound evaluations.

Content hashes for the detailed evidence are:

- `nodes.jsonl.gz`: `8a6a0436e126857b1a8e5e1450d63e97d9d82c2f35a500cd9d069c52ad317622`
- `rays.jsonl.gz`: `05d2d52aa7c33d38389e22cad948afc6adfc2e0a58bd0e1c1cfb8b08032d1cd3`
- `case_summaries.jsonl.gz`: `ad770a74345ef1b266e10adf80aa3df0375b320ec989b6c805d2ad0e43f759e6`

The next implementation unit is P4 run lifecycle. It must be planned before code and should directly address the empty directory left by an interrupted pre-manifest launch: create a running manifest first, write case-complete partial streams/checkpoints, and resume without deletion.

## 17. P4 atomic lifecycle and resume plan — 2026-08-23

P4 changes experiment storage lifecycle only. It must not change map generation, ray generation, certificates, traversal decisions, leaf roots, counters, or the P3 record schema.

### 17.1 Logical run and physical attempts

Use the schema-native logical ID `step1-<configuration_hash[:12]>` and layout:

```text
experiments/step1_ray_reference/<logical_run_id>/
  configuration.json
  attempts/
    a0001/
      run_manifest.json
      checkpoint.json
      shards/
        maps.jsonl.gz
        cases.jsonl.gz
        <case_id>/nodes.jsonl.gz
        <case_id>/rays.jsonl.gz
        <case_id>/case_summary.jsonl.gz
      failures/
      final/
        maps.jsonl.gz
        cases.jsonl.gz
        nodes.jsonl.gz
        rays.jsonl.gz
        case_summaries.jsonl.gz
        README.md
```

The logical configuration file is created exclusively and must byte-match on reuse. Attempt `a0001` is default. `--resume` names an existing running/failed attempt; `--new-attempt` creates the next unused attempt after an earlier complete or failed run. A complete attempt is immutable. P4 never deletes an attempt or silently overwrites a final stream.

### 17.2 Creation order and atomicity

1. Resolve the experiment root and verify every target remains below it.
2. Exclusively create the logical directory/configuration if absent, or validate its exact configuration hash if present.
3. Exclusively create the attempt directory.
4. Write `run_manifest.json` with `state="running"` **before** structural checks or case execution.
5. Write each file to a sibling `.tmp`, flush/close it, validate it by reopening, compute its SHA-256, then publish it with same-filesystem `os.replace`.
6. After a complete case shard is published, atomically replace `checkpoint.json` with the updated completed-case list, counts, and shard hashes.
7. On finalization, merge validated shards in frozen expected-case order into final `.tmp` streams, validate and hash them, publish them atomically, then atomically replace the manifest with `state="complete"`.

No scientific result is considered complete because files merely exist. Only a complete manifest whose hashes/counts validate every final stream is a completed run.

### 17.3 Case-shard transaction

A case is the recovery unit. Its node, ray, and summary shards are first written under a temporary case directory. The case becomes checkpoint-complete only after:

- all records validate individually;
- all records reference the expected case/grid;
- node and ray sequence indices are complete within the case;
- per-node visit sums equal per-ray method totals;
- the detailed/global legacy-equivalence assertion passes; and
- all three shard hashes and counts are recorded.

On resume, checkpoint-complete cases are reopened and fully revalidated before being skipped. Uncheckpointed temporary shards remain forensic artifacts and are never treated as complete. P4 does not delete them automatically.

### 17.4 Resume contract

`--resume` requires:

- identical schema version and canonical configuration hash;
- manifest state `running` or `failed`;
- exact expected case IDs and order;
- a valid checkpoint;
- every completed shard present with the recorded hash/count; and
- no final stream that conflicts with the manifest state.

A missing/corrupt completed shard, configuration mismatch, duplicate record, unexpected case, or inconsistent sequence blocks resume with a precise diagnostic. The runner must not “repair” evidence by dropping a case. A complete attempt rejects resume.

### 17.5 Failure and interruption behavior

On a caught scientific/schema exception, write a canonical failure witness under `failures/`, preserve all shards, and atomically set the manifest to `state="failed"`. On process termination that prevents cleanup, the last published manifest/checkpoint still identify exactly which case transactions are valid. An orphan attempt directory without a manifest is reported and left untouched; the user may inspect it or start a new attempt.

For testing only, expose an injectable stop-after-N-cases hook in the Python API. It must interrupt after checkpoint publication, not inside an algorithm, so resume behavior can be tested deterministically without OS-specific process killing.

### 17.6 Final merge and deterministic content

Final stream order is fixed:

- maps by map manifest order;
- cases by expected case order;
- nodes by case order then `(level,iy,ix,size)`;
- rays by case order then family/subfamily/index; and
- summaries by case order.

Final GZIP writers retain empty filename and `mtime=0`. Two uninterrupted/resumed attempts with the same configuration must have byte-identical final record streams even though manifests have different attempt IDs, timestamps, and elapsed time.

### 17.7 Manifest and README

The final manifest records all fields required by `step1-v1`, plus attempt ID, stage, final file hashes, checkpoint hash, structural-check results/hash, case-shard hashes, observer mode, fallback policy, and the exact Conda/Python command.

`final/README.md` explains:

- the declared analytic surface and four method IDs;
- the counting semantics frozen in P3;
- record relationships and units;
- how to validate hashes and regenerate summaries;
- that CPU counts are not GPU timing; and
- how fallbacks and null fields must be interpreted.

### 17.8 P4 tests

1. An uninterrupted two-case run and a stop-after-one-case/resume run have byte-identical final streams.
2. Resume does not duplicate maps, cases, nodes, rays, or summaries; all final sequence indices are complete.
3. Resume rejects a changed seed, map list, ray count, eta, schema version, or expected case order.
4. Resume rejects a corrupted/missing checkpoint-complete shard and preserves it for inspection.
5. A complete attempt rejects mutation/resume; `--new-attempt` creates a sibling attempt.
6. A caught synthetic scientific failure writes a witness and `state="failed"`, and a valid resume can continue from prior complete cases.
7. An orphan attempt directory without a manifest is detected and never auto-deleted.
8. Manifest counts and hashes agree with a fresh independent stream readback.
9. P1–P3 tests, the legacy 384-ray gate, and a small P3 detailed equality run remain unchanged.

**Gate P4:** all nine lifecycle tests pass, including byte-identical interrupted/resumed final streams, and no scientific source or P3 record field changes. Only then run P5 through the lifecycle runner and begin the dataset-loader plan.

### 17.9 P4/P5 development completion — 2026-08-23

P4 is implemented in `scripts/step1_run_lifecycle.py` with a separate storage boundary around the unchanged P3 scientific runner. Seven dedicated lifecycle tests cover all nine planned behaviors: interrupted/resumed byte equality, complete sequence/count closure, changed-configuration refusal, corruption refusal with preservation, complete-attempt immutability and sibling attempts, persisted scientific failure and resume, orphan preservation, and independent manifest hash/count readback. The full P1–P4 suite contains 39 passing tests.

Manifest-backed development attempt `step1-512846b663fc/attempts/a0001` completed with:

- state `complete`, 14/14 checkpointed cases, and zero failure witnesses;
- 7 maps, 14 cases, 4,774 nodes, 896 rays, and 14 case summaries;
- independently verified final hashes, counts, cross-record references, and complete monotone sequence indices; and
- node/ray/summary scientific records exactly equal to P3 after removing only the intentionally different logical `run_id`.

The scientific totals are unchanged: 13 exact fallbacks, zero omissions/mismatches, 14,705/14,277 `G-MM/G-FO` nodes, 1,331/1,146 exact leaf-interval tests, and the same adverse directional-lazy result. This attempt is the P5 development smoke through the final lifecycle, not the frozen decision corpus.

Final evidence is under `experiments/step1_ray_reference_lifecycle/step1-512846b663fc/attempts/a0001/final/`; its manifest is the authority for hashes and counts.

---

Related: [[Plan — Step 1 ray reference experiment]] · [[Plan — Ray application method, prototype, and baselines]]
