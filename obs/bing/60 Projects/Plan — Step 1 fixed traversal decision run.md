---
title: Plan — Step 1 fixed traversal decision run
tags: [plan, step-1, first-order, min-max, traversal, ablation, experiment]
status: complete
created: 2026-08-23
updated: 2026-08-23
parent: "[[Plan — Step 1 ray reference experiment]]"
implementation-status: complete
input-rays: rays-935162cc636e
input-oracle: oracle-dd072597b14a/a0001
---

# Plan — Step 1 fixed traversal decision run

> [!abstract] T1 decision
> This is the first run allowed to test the contribution's pruning hypothesis. `G-MM` and `G-FO` receive the **same frozen ray, rational curve, valid intervals, certified global segments, exact leaves, and oracle**. The only changed predicate is scalar min/max versus the first-order plane-plus-remainder slab. No lazy-directional method, DDA, CUDA timing, or external baseline is included.

## 1. Accepted inputs

The runner hard-checks and records:

- D2 ray suite `rays-935162cc636e`, ray hash `0073118bbea522bb44998de5b3794944609af10c928f9e94c71ea7037d6d7243`;
- O1 float annotations `oracle-dd072597b14a/a0001`, hash `b8a05ab6afd954484ec3517ab2731ea8ae51abc9820db82437183426e3f44453`;
- O1 high-precision annotations `oracle-hp-791061ae1b7d/a0001`, hash `1e5a18c4a8b66108afb7f5095b5cad08aea5e7e6ef8ed9ed555cc9db52bdc995`;
- 15 D1 grid identities, three D2 shell IDs, 45 case keys, and 11,520 ray IDs; and
- `cells=32`, global tube target $\eta=0.25$ texel, thickness scale one.

Any missing or changed reference is a hard configuration error.

## 2. Frozen method isolation

For every non-structural ray:

1. reconstruct the exact D2 world ray and analytic triangle shell;
2. form one rational shell curve $q(h)$;
3. build one globally certified componentwise curve partition at $\eta=0.25$;
4. pass the exact same immutable segment list to both methods;
5. traverse the same quadtree and proxy-overlap rules;
6. use `node_height_overlap(..., mode="minmax")` for `G-MM` and `mode="taylor"` for `G-FO`; and
7. call the same analytic cubic solver on every admitted leaf/segment pair.

`G-MM` is the zero-order control, not a full TFDM implementation. `G-FO/G-MM` therefore isolates the stored first-order displacement correlation; it cannot by itself support a TFDM superiority claim.

The hierarchy is built once per grid. Segment construction is built once per ray and reused; it is not charged differently to either method. Candidate-hit grouping reuses O1's physical-root grouping code.

## 3. Structural and fallback policy

- The 360 one-ULP `F04` range rays are marked `expected_range_fallback` and excluded from ordinary method correctness/work aggregates. Neither method may claim a win on them until a certified range predicate is implemented.
- If global segment construction returns `denominator_root`, `denominator_conditioning`, `event_isolation`, or `resource_limit`, both methods receive the same fallback status and zero traversal work. These rates are reported by family/shell/map and tested against G1/G3; they are not silently dropped.
- O1 reports 225 rays with a denominator root somewhere in the full height range. T1 must distinguish an actually relevant singular valid interval from an irrelevant root outside the proxy/ray-valid domain. The first run keeps the existing conservative fallback. A tighter validity-aware partition is a separately planned revision only if the frozen G1 fallback gate fails.
- A resource cap never becomes a miss. Any unplanned exception writes a witness and blocks finalization.

## 4. Static hierarchy evidence

Before tracing, persist every hierarchy node once per grid with:

```text
grid_id, node_id, level, normalized_depth, ix, iy, size,
proxy_overlap_fraction, h_min, h_max, minmax_width,
h0, g_u, g_v, remainder_r, residual_width,
exact_residual_min, exact_residual_max, residual_slack_ratio
```

Check exact descendant containment and report all nodes versus proxy-overlapping nodes. Static tightness is independent of ray selection; ray-visited tightness is joined later through visit counts.

The primary bound metric is `residual_width/minmax_width`, with exactly planar zero-width cases separated rather than divided by zero.

## 5. Per-ray and per-node work records

For each active method record:

```text
status, fallback_reason, hit, physical_root_count,
root_set_mismatch, closest_t_error, candidate_owner_omissions,
node_tests, internal_nodes, unique_candidate_leaves,
exact_leaf_segment_tests, rejected_uv, rejected_height,
global_segments, valid_intervals, segment_linearizations,
segment_splits, isolated_points, max_eps_u_texels, max_eps_v_texels
```

`exact_leaf_segment_tests` counts each analytic cell test for each overlapping segment; it is not the same as unique candidate cells. Per-node records store visit and decision counts for both methods so sums close exactly to per-ray counters.

Correctness compares complete grouped root sets to persisted O1, not only hit/miss or closest $t$. Candidate-owner omissions additionally require at least one admitted owner for every O1 physical root group.

Root-set comparison uses the parent Step 1 numeric contract: equal physical-root count and ordered $t$ agreement within `max(1e-10, 64 ulp(t))`, with world position inheriting the same bound for unit D2 directions. Recovered $h,u,v$ are checked with the existing `2e-9` ownership tolerance as diagnostics; they are not required to agree to a tighter value than the root solver guarantees when the same cubic is isolated over a narrower segment interval.

## 6. Persistence and execution phases

Use `step1-fixed-v1` records and the O1 attempt lifecycle: running manifest, immutable logical configuration, atomic case shards, hash receipts, resume of a running attempt, preserved failure witness, deterministic merged scientific records, and separate non-deterministic budget timings.

Phases, each with tests before execution:

1. **T0 static:** build/freeze the 15 hierarchies; validate exact containment and emit per-level tightness records. No ray traversal.
2. **T1 adapter smoke:** `P03-sine-bump × S02-stress`, all 256 rays; compare both complete root sets to O1 and close every counter.
3. **T2 decision:** all 45 cases with atomic/resumable case workers.
4. **T3 report:** regenerate G1–G3 tables/plots only from persisted records and write a machine-readable gate decision.

Do not run T2 if T1 has one non-fallback mismatch or counter-closure error.

> [!note] T1 comparison-adapter correction — 2026-08-23
> The first P03/S02 smoke had equal three-root counts and zero owner omissions on two rays, but the adapter marked both methods mismatched because it required `1e-12` agreement in recovered UV. Narrowed segment isolation changed UV by at most `1.63e-12`, while $t$ differed by at most `2.08e-12`—well inside the predeclared `1e-10` CPU root tolerance. The comparison is corrected to the frozen $t$/position contract plus `2e-9` UV/height ownership diagnostics. No traversal predicate, segment, hit, or work count changes.

## 7. Required tests

1. All D2/O1 hashes, IDs, counts, and references reopen exactly.
2. T0 hierarchy contains all descendant analytic microtriangles; planar P01/P02 recover zero residual up to widened roundoff.
3. Observed and unobserved traversal return byte-identical hits/counters.
4. `G-MM` and `G-FO` receive byte-identical segment records for every ray.
5. Synthetic reject/descend/leaf decisions independently reproduce each predicate.
6. Complete grouped roots match O1 for every active method; shared-edge/vertex owners do not create false mismatches.
7. Per-node visits and exact-test decisions sum exactly to per-ray/case totals.
8. The 360 range rays and every segment fallback have symmetric method status and zero claimed traversal advantage.
9. Interrupted/resumed smoke output matches uninterrupted scientific bytes.
10. All existing 70 P1–O1 tests remain unchanged and pass.

## 8. Primary tables and plots

1. Per-level `residual_width/minmax_width` distributions for all and proxy-overlapping nodes, split by map class.
2. Paired `G-MM` versus `G-FO` node tests and exact leaf-segment tests, equality line plus paired ECDF.
3. Case heatmap of `G-FO/G-MM` exact tests over map × shell, with family/incidence drill-down.
4. Segment/fallback distribution by family and shell, including denominator-root conditioning.
5. Cost-model sensitivity over the predeclared node-byte and leaf-cost ranges.
6. Correctness/gate dashboard with root mismatches, owner omissions, fallback classes, and counter closure.

Ratios use totals in the declared cohort. Always show raw totals and case counts beside ratios.

## 9. Frozen interpretation gates

Use G1–G3 exactly as defined in [[Plan — Step 1 ray reference experiment#13. Explicit gates]]. In particular:

- zero non-fallback root-set mismatch or owner omission;
- coherent pooled exact leaf-test reduction at least 10%, with node tests no worse than 5%;
- at least one coherent procedural and two real map IDs at `≤0.85` exact-test ratio and `≤1.05` node ratio;
- useful-level residual ratio criterion in at least 70% of coherent cases; and
- global segment tails/fallbacks within G3.

The existing small P3 result—13.9% fewer leaf tests—is not substituted for this decision run.

## 10. What a pass and failure mean

- **G1–G3 pass:** authorize planning of the fixed-segmentation GPU slab ablation. Still no claim against TFDM/Ogaki/RMIP/PDM/DMM.
- **G1 pass, G2 marginal:** one predeclared layout/fold revision may be planned, then the complete frozen corpus reruns.
- **G1 pass, G2 no-go:** stop the ray-performance branch as a primary contribution; retain the hierarchy for the area-sampling test.
- **G1 fails:** repair the reference witness before interpreting any work count.
- **G3 fails from irrelevant denominator roots:** plan a validity-aware singularity partition; do not simply suppress fallback.

## 11. Paper writing checkpoint

After T3, the user can safely draft the ray-results subsection only if it says exactly what the gate establishes: first-order versus zero-order pruning on the same certified curve and exact surface. External-baseline and GPU paragraphs remain placeholders until later phases.

---

Related: [[Plan — Step 1 oracle integration]] · [[Plan — Step 1 ray reference experiment]] · [[Guide — Eurographics paper draft]]

## 12. T0–T2 completion — 2026-08-23

T0 `static-b8915692cfeb` contains 20,475 nodes over 15 maps with zero descendant-containment violations. T1 `P03-sine-bump × S02-stress` closes all per-node/per-ray counters and matches every active O1 root; it reduces exact leaf/segment tests from 869 to 583. T2 `fixed-b78c227f53c2/a0001` processes all 45 cases and 11,520 rays with zero active root-set mismatches or owner omissions. Aggregate G-FO/G-MM ratios are `0.9373` for node tests and `0.7569` for exact tests. The formal gate interpretation is recorded in [[Plan — Step 1 gate report]].
