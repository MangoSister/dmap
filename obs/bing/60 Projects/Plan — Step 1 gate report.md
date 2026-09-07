---
title: Plan — Step 1 gate report
tags: [plan, step-1, first-order, traversal, evidence, gates]
status: complete
created: 2026-08-23
updated: 2026-08-23
parent: "[[Plan — Step 1 fixed traversal decision run]]"
implementation-status: complete
input-fixed-run: fixed-b78c227f53c2/a0001
---

# Plan — Step 1 gate report

> [!abstract] T3 decision
> T3 converts the immutable T0–T2 records into the predeclared G1–G3 decision. It may aggregate, tabulate, and plot; it may not rerun traversal, alter a certificate, remove an inconvenient case, or tune a threshold.

## 1. Immutable inputs

The report must verify and record the hashes of:

- D2 `rays-935162cc636e`;
- O1 `oracle-dd072597b14a/a0001` and O1-HP `oracle-hp-791061ae1b7d/a0001`;
- T0 `static-b8915692cfeb`; and
- T2 `fixed-b78c227f53c2/a0001`.

Every merged stream is reopened, hashed, and checked against its result/manifest before any gate is evaluated. The 45 case keys and 11,520 ray IDs must close exactly.

## 2. Frozen cohorts and denominators

- **Coherent G2 maps:** `P01`, `P02`, `P03`, `P04`, `R00`, `R01`, `R02`, `R03`, and `R05`, using their full persisted map IDs.
- **Real-map successes:** only the `R*` members of that coherent cohort.
- **Ordinary non-grazing fallback denominator:** all `F00-aimed` and `F01-free` rays, across every map and shell.
- **Ordinary including grazing denominator:** add `F02-grazing`.
- `F03-boundary`, `F04-origin-range`, and `F05-near-miss` are reported separately as ownership/range/near-miss stress families. The 360 expected F04 range fallbacks remain excluded from ordinary fallback rates.
- Segment distributions use active, non-range, non-segment-fallback rays. They use `shared_segments`, since G-MM and G-FO consume the identical partition.

No per-ray or per-case average substitutes for a declared pooled total ratio. Both are reported, but G2 work gates use pooled raw totals.

## 3. Visited-node tightness

For each coherent map/shell case, form the set of unique hierarchy nodes at levels 2–4 from the leaves for which `G-MM_visits + G-FO_visits > 0`. Join those nodes to T0 by exact `node_id` and take the unweighted median of `residual_to_minmax_ratio` over the set. A case passes the useful-level condition at median `<= 0.75`.

This union-of-visited-nodes definition avoids counting a node twice because both methods visited it and avoids letting repeated rays dominate the stored-bound question. Supplemental tables report each level separately and a visit-count-weighted median, but neither changes the gate. Planar, widened-roundoff ratios remain valid near-zero values; zero min/max-width nodes are reported separately and excluded from division.

## 4. Gate calculations

### G1 — correctness and certification

Evaluate the frozen conditions directly: T0 containment violations, O1-HP root mismatches, T2 root-set mismatches, candidate-owner omissions, symmetric fallback status, and ordinary fallback rates. The existing O1 sampled/certified ratio is copied from its persisted diagnostics; a missing required field is `not_evaluated`, never silently treated as a pass.

### G2 — material first-order benefit

Report:

1. the fraction of 27 coherent map/shell cases passing visited tightness;
2. per-case and per-map pooled node/exact-test ratios;
3. the number and identities of procedural and real maps satisfying the frozen `0.85/1.05` pair;
4. coherent pooled raw totals and ratios; and
5. every non-declared-high-frequency case with node ratio above `1.20`.

The declared adversarial/high-frequency control set is `P05b`, `P06`, `P07`, `P08`, and `R04`. It remains visible in all-case tables but does not rescue or veto the coherent-cohort thresholds except where the original gate explicitly asks for all ordinary cases.

### G3 — affordable certification

Compute nearest-rank p50/p95/p99 and maximum segment counts for:

- non-grazing `F00/F01` rays in `S00/S01`;
- all ordinary `F00/F01/F02` rays in all shells; and
- each family and shell separately.

Report denominator-root/conditioning fallback counts and rates for both ordinary cohorts and all stress families. The not-yet-run resolution/eta sweep is explicitly `pending`; therefore T3 cannot declare full G3 pass even if the default-setting tails pass.

## 5. Decision vocabulary

Each atomic condition is `pass`, `fail`, or `pending`. A gate is:

- `pass` only when all its atomic conditions pass;
- `fail` when any mandatory condition fails; or
- `pending` when none fails but at least one required condition has not yet been run.

The overall authorization follows the parent decision matrix. In particular, a G2 failure blocks a GPU performance claim; a G2 pass with G3 pending supports the hierarchy mechanism but does not yet authorize the fixed-segmentation GPU ablation under the frozen rules.

## 6. Outputs and tests

Write a versioned directory containing:

- `gate_decision.json` with atomic conditions, raw numerators/denominators, input hashes, and the overall decision;
- `case_metrics.jsonl.gz`, `map_metrics.jsonl.gz`, and `segment_metrics.jsonl.gz`;
- compact Markdown tables for the project plan/paper notebook; and
- PDF/SVG/PNG figures for the case work heatmap, paired work plot, visited tightness, and segment tails.

Tests independently reconstruct aggregate totals, verify exact joins and quantiles, permute input order without changing scientific output, and include synthetic pass/fail/pending gate fixtures. Only after these tests pass may the project plan record the contribution conclusion.

---

Related: [[Plan — Step 1 fixed traversal decision run]] · [[Plan — Step 1 ray reference experiment]] · [[Guide — Eurographics paper draft]]

## 7. T3 completion — 2026-08-23

The provenance-corrected frozen report is `report-f37590d2fb1a`, generated only from immutable T0–T2 records. Ten T1/T3 tests pass, including aggregate closure, exact T0 joins, input-order invariance, fallback-class separation, the `R00` synthetic-source classification, and the existing fixed-traversal smoke totals. The report reopens all input hashes before aggregation and emits machine-readable gate conditions plus four figure families under `figures/step1_gate_report/report-f37590d2fb1a/`.

Observed evidence:

- all 45 cases: G-FO/G-MM node ratio `0.9373`, exact leaf/segment-test ratio `0.7569`;
- coherent 27-case cohort: node ratio `0.8972`, exact ratio `0.6906`;
- zero active root-set mismatches, zero candidate-owner omissions, zero T0 containment violations, and zero mismatches in the linked 90-ray Decimal cross-check;
- eight coherent maps pass the per-map `0.85/1.05` work criterion: four analytic/procedural maps, the file-backed synthetic `R00`, and three practical Poly Haven maps (`R01`–`R03`);
- certified segment tails pass comfortably: ordinary non-grazing `p95=2`, `p99=2`, `max=4`; all ordinary `p95=3`, `p99=4`, `max=6`;
- useful-level static tightness passes only `9/27` coherent map/shell cases, below the frozen `70%` requirement; and
- denominator-root fallback occurs for `169/5760 = 2.93%` of ordinary non-grazing rays and `207/7920 = 2.61%` including grazing, above both frozen limits.

Therefore G1, G2, and G3 are formally `fail` under the predeclared rules. This is not a correctness miss: every traversed active ray matches the oracle. It is a scope/robustness decision. The direct first-order slab has strong conditional pruning evidence, but the broad claim that stored first-order residuals are routinely much tighter than min/max is not supported, and the current full-height denominator-root fallback is too coarse for a GPU port.

The next permitted ray experiment is a separately planned validity-aware singularity partition. It must retain fallback for a denominator singularity inside an actually valid traversal interval and remove only roots proven irrelevant to the proxy/ray-valid domain. Even if that repairs G1/G3, the failed static-tightness condition remains visible; it cannot be erased post hoc. A later decision must either accept the frozen no-go for the ray-performance branch or explicitly motivate a new orientation-aware slab-overlap hypothesis and freeze a new experiment before implementation.

> [!note] Provenance-label correction before report regeneration — 2026-08-23
> The original T3 aggregation inferred a "real" map from an `R*` prefix. `R00-test-macro` is actually the file-backed synthetic `data/test_disp.png`, not a captured/material displacement asset. The corrected reporting registry counts only `R01`–`R06` as practical file-backed material maps. R00 remains in the already frozen coherent work cohort, so no pooled work or tightness value changes. The successful practical-real set changes from the incorrectly reported four IDs to `R01`, `R02`, and `R03`, which still passes the predeclared requirement of at least two. This is a provenance correction, not a threshold or traversal change.
