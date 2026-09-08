---
title: Plan — P-D2 residual clipping ablation
tags: [plan, displacement-mapping, ray-tracing, first-order, DDA, ablation]
status: active
created: 2026-08-24
updated: 2026-08-24
parent: "[[Plan — Practical first-order DDA ray traversal]]"
implementation-status: p-d2-0c-complete-negative
---

# Plan — P-D2 residual clipping ablation

> [!abstract] P-D2 decision
> P-D1 proved that the triangle-proxy shell ray, certified UV tube, closed event supercover, and represented cubic leaf equation recover exhaustive roots. P-D2 asks the first contribution-specific performance question: on that identical certified traversal, does a stored first-order height predictor reject or shorten materially more hierarchy work than scalar min/max? P-D2 reports operation traces and a predictive break-even model, not elapsed GPU performance.

## 1. Scope and non-scope

P-D2 includes:

1. a shared hierarchical event-replay traversal over the passing P-D1 tubes;
2. scalar min/max clipping, first-order residual tests, first-order residual clipping, and their intersection;
3. exact represented-leaf roots through the unchanged P-D1.5 evaluator;
4. all 48 practical correctness windows and all 864 frozen rays;
5. per-node/per-ray mechanism counts and a measured-cost budget for the later GPU replay; and
6. a predeclared kill gate before building full-resolution hierarchy arrays.

P-D2 excludes CUDA/OptiX timing, packed payloads, external TFDM/Ogaki/RMIP/PDM/DMM comparisons, new ray segmentation policies, Catmull–Clark surfaces, and paper claims of faster tracing. P-D3 owns payload/layout cost; P-D4 owns the internal GPU speed gate.

## 2. Immutable source chain

Consume only the corrected passing artifacts:

- P-D0 practical manifest `p-d0-71627ba7b07f`;
- P-D1.2 shell/ray intervals `p-d1-2-e2749e3ff000`;
- exact recursive coverage `p-d1-3-62f0f7416547`;
- closed event coverage `p-d1-4-dd1251769c51`; and
- full represented-root oracle `p-d1-6-e254f252e010`.

The new configuration serializes all five source hashes. P-D2 may not alter tubes, event ownership, proxy filtering, the fixed displacement diagonal, shell families, rays, or leaf roots. Any discovered correctness defect returns to its producer and regenerates the chain; it is not hidden by a P-D2 tolerance.

## 3. Shared hierarchical traversal

For one accepted shell-height interval, use normalized `s∈[0,1]` with

$$
h(s)=h_a+s(h_b-h_a),
$$

UV chord `q̄_uv(s)` and certified componentwise radii `(ε_u,ε_v)`. P-D1.4 already supplies every closed tube-overlapped cell interval at levels `0..6`.

P-D2 replays those integer event records as a hierarchy:

1. begin with the root state `(root,[0,1])`;
2. intersect a child's exact P-D1.4 UV interval with the clipped interval inherited from its parent;
3. apply the selected height/residual predicate and retain its returned subinterval;
4. visit surviving children in `(entry parameter,Morton code,exit parameter)` order; and
5. at a surviving leaf, call the unchanged represented cubic evaluator and canonical root grouping.

Every variant receives the same tube/event records and differs only in the one-dimensional height predicate. A node is counted when its payload/predicate would be evaluated. A child excluded by the common UV event record is not credited to any height representation.

## 4. Predicates

For node `A`, P-D1.2 stores

$$
p_A(u,v)=c_A+g_{u,A}(u-u_A)+g_{v,A}(v-v_A),
$$

$$
d(u,v)-p_A(u,v)\in[-r_A,r_A],
$$

plus exact folded scalar bounds `[d_A^-,d_A^+]`.

### 4.1 Scalar min/max

Because shell height itself is the curve parameter, min/max clipping solves exactly

$$
I_{MM}=I\cap\{s:h(s)\in[d_A^-,d_A^+]\}.
$$

This is stronger and fairer than a boolean overlap test. `MM-test` is retained only to isolate the value of interval clipping; `MM-clip` is the internal baseline.

### 4.2 Componentwise first order

Along the UV chord, define the affine residual ray

$$
\bar\rho_A(s)=h(s)-p_A(\bar u(s),\bar v(s)).
$$

The implementable componentwise error is

$$
E_{CW,A}=|g_{u,A}|\epsilon_u+|g_{v,A}|\epsilon_v.
$$

The conservative residual interval is `[-r_A-E_CW,A, r_A+E_CW,A]`. `FO-CW-test` performs only overlap; `FO-CW-clip` solves the affine inequalities and passes the contracted interval to children.

### 4.3 Direct directional oracle

`FO-DIR-clip` replaces `E_CW,A` by the direct certified error of `g_u u+g_v v` over the current interval. This is a mechanism upper bound, not the presumed GPU default: computing a fresh rational directional certificate at every node may be too expensive. Retain it only if it materially changes downstream work relative to `FO-CW-clip`.

### 4.4 Hybrid

`HYB-clip` intersects `I_MM` and `I_FO-CW`. The first-order hierarchy already carries scalar bounds for construction/validation, so this tests whether plane correlation complements rather than strictly replaces min/max. Report its additional payload/fetch requirement explicitly; do not label a hybrid win as a pure first-order-only memory win.

## 5. Frozen P-D2.0 variants

Run, in this order:

1. `MM-test`;
2. `MM-clip`;
3. `FO-CW-test`;
4. `FO-CW-clip`;
5. `FO-DIR-clip`; and
6. `HYB-clip`.

All six use the same ordered hierarchy replay and exact leaf evaluator. No per-variant segmentation, UV widening, leaf approximation, early any-hit termination, or candidate cap is allowed. Closest-hit is the primary correctness record; an any-hit work simulation may be derived afterward from the same canonical root ordering.

## 6. Frozen cohorts

Run all 864 P-D1 cases. Report without retuning:

- **coherent/non-grazing:** `front-hit`, `oblique-hit`, and `grid-corner-hit`;
- **boundary/stress:** `grazing-hit` and `proxy-edge-hit`;
- **empty-space diagnostic:** `near-miss` (the name describes construction only; P-D1 showed that it can still intersect elsewhere);
- each of 12 asset identities;
- source bit depth/resolution;
- `S0/S1/S2` shell conditioning; and
- smooth/sparse/high-frequency descriptors already frozen by P-D0.

No case is removed because it loses. The first-order hypothesis is evaluated primarily on the coherent/non-grazing cohort and separately on stress/empty-space cases.

## 7. Records and metrics

Per `(tube,variant)` persist:

- UV event nodes offered, height predicates evaluated, height rejects, descended nodes, and admitted leaves;
- interval width before/after each predicate and cumulative parent-to-leaf contraction;
- scalar overlap width, componentwise residual width, direct-directional width, and `E_DIR/E_CW`;
- leaf polynomials solved, raw/grouped roots, closest root, owner sets, and constructed-target retention;
- p50/p95/p99/max depth, nodes, leaves, and contraction per ray;
- ordered node trace `(level,ix,iy,entry,exit,decision)` for later GPU replay; and
- all numerical/denominator/resource fallbacks.

Primary paired outputs are `FO-CW-clip/MM-clip`, `FO-DIR-clip/FO-CW-clip`, and `HYB-clip/MM-clip` for node predicates and leaf polynomials. Width plots remain explanatory, not the decision metric.

## 8. Predictive cost model

Let `N_v` and `L_v` be node predicates and leaf solves for variant `v`. For measured or assumed per-operation costs `c_N,MM`, `c_N,FO`, and `c_L`, predict

$$
C_v=N_vc_{N,v}+L_vc_L+C_{common}.
$$

Report the maximum tolerable first-order node overhead

$$
\delta_{max}
=\frac{0.9(N_{MM}c_{N,MM}+L_{MM}c_L)-L_{FO}c_L}
       {N_{FO}c_{N,MM}}-1
$$

needed for a projected `10%` internal speed win, over a sweep of `c_L/c_N`. Negative or very small `δ_max` is an early warning that no payload layout can rescue the ray claim. This model selects what P-D3 must measure; it is not a performance result.

## 9. Correctness and usefulness gates

### Correctness gate

Every variant must reproduce P-D1.6's canonical closest/grouped roots and exact owner sets, retain every constructed target, satisfy the same residual tolerances, and report every fallback. Any mismatch blocks all later work.

### P-D2.0 first-order usefulness gate

The first-order ray hypothesis advances to full-resolution traces only if:

1. `FO-CW-clip` or `HYB-clip` reduces pooled admitted leaf polynomials by at least `15%` versus `MM-clip` on the coherent/non-grazing cohort;
2. it wins on at least 7 of 12 asset identities in that cohort;
3. pooled node predicates do not increase by more than `10%`;
4. no non-adversarial asset has an unexplained leaf/node regression above `20%`; and
5. the predicted `δ_max` is positive for at least the middle two frozen `c_L/c_N` scenarios.

If only `HYB-clip` passes, continue with an explicitly hybrid claim and account for both payloads. If neither passes, stop the first-order ray-acceleration branch before full-resolution/GPU work; retain P-D1 as correctness infrastructure for the shared-query paper only if the area application justifies it.

`FO-DIR-clip` is retained beyond P-D2.0 only if it reduces pooled node or leaf work by at least another `5%` versus `FO-CW-clip`; otherwise the direct per-node certificate is dropped from the implementation and reported as a negative ablation.

## 10. Ordered implementation

1. `P-D2.0a`: implement/test the affine interval clip primitives on analytic constant, parallel, tangent, reversed, point, and nearly parallel cases.
2. `P-D2.0b`: implement one shared hierarchy replay and verify all variants against P-D1.6 on the 18-case P-D1.5 anchor.
3. `P-D2.0c`: run all 864 cases, generate paired mechanism plots, and make the usefulness call.
4. Only if the gate passes, write a separate `P-D2.1` manifest for vectorized full-resolution pyramids and frozen ray batches; do not infer its sample/grid convention from the 65×65 windows.
5. After P-D2.1, write P-D3's GPU payload replay plan before any CUDA/OptiX integration.

Stop and record results after every numbered substep. No P-D2 code is authorized until this plan is reviewed.

## 12. Frozen P-D2.0b anchor replay

P-D2.0b is an integration gate over the same `PH-rock05-1k-W0` anchor used by P-D1.5. It contains exactly 18 cases: three frozen shell families times six frozen ray families, with one reconstructed valid tube per case. It consumes:

- ray records from `p-d1-2-e2749e3ff000`;
- tube records from `p-d1-3-62f0f7416547`;
- all event levels `0..6` from `p-d1-4-dd1251769c51`;
- the P-D1.5 full-Decimal anchor `p-d1-5-b7bcc9a005a6`; and
- the corresponding authoritative root/owner records in `p-d1-6-e254f252e010`.

The runner hashes every consumed file. P-D1.5 and P-D1.6 must agree on the 18 oracle root groups before a P-D2 variant is evaluated.

### 12.1 Hierarchy reconstruction and level map

Rebuild the `64×64` first-order hierarchy through the exact-envelope `load_windows` producer, persist its new hierarchy hash in the P-D2 configuration, and require the inductive exact-rational enclosure audit to pass. Do not compare it to the superseded single-ULP-fold hash from the older hierarchy-dependent P-D1.2 run. Event level `\ell_e` uses resolution $2^{\ell_e}$ and maps to stored node level

$$
\ell_n=6-\ell_e,
$$

with stored base-cell origin `(node.ix,node.iy)=(event_ix·node.size,event_iy·node.size)`. Every event candidate must map to exactly one node, and every traversed child must have the expected parent.

### 12.2 One shared replay

For each tube:

1. start from the event-level-0 root interval;
2. intersect each node's closed P-D1.4 event interval with its parent's surviving clipped interval;
3. count the node only when that common UV interval is nonempty and its height predicate is evaluated;
4. apply exactly one of the six frozen predicates;
5. sort surviving children by `(entry parameter, Morton code, exit parameter)`; and
6. emit each surviving level-6 cell and its final closed interval to the unchanged P-D1.5 cubic evaluator.

Closed interval intersection is inclusive. No interval-width epsilon, candidate cap, early hit termination, per-variant UV widening, or per-variant segmentation is allowed.

### 12.3 Directional oracle definition

`FO-DIR-clip` remains referenced to the same global certified chord used by every other variant. Over the current inherited interval, form the scalar rational error

$$
e_A(h)=g_A^T\left(q_{uv}(h)-\bar q_{uv}(h)\right)
=\frac{Z_A(h)-\ell_A(h)D(h)}{D(h)},
$$

where $\ell_A$ is the projection of the fixed global chord. Bound its numerator and denominator using exact-rational Bernstein hulls of the binary64 coefficients. A denominator interval containing zero is an explicit fallback and blocks the anchor gate. The implemented directional width is the tighter of this direct certificate and the already certified componentwise width; record both values and their ratio. This is a mechanism oracle, not the presumed GPU predicate.

### 12.4 Frozen trace schema

Persist one record per evaluated node with:

- `tube_id`, `variant`, `event_level`, `node_level`, `ix`, `iy`, `size`;
- offered event interval, inherited interval, pre-predicate intersection, and surviving interval;
- decision in `{height-reject, descend, leaf}`;
- width before/after and cumulative root-to-node contraction;
- `h_min/h_max`, `h0`, `g`, `r`, tube radii;
- `E_CW`, raw `E_DIR`, chosen directional error, and fallback reason where applicable; and
- child visit rank for deterministic-order replay.

Per `(tube,variant)` records contain offered/evaluated/rejected/descended/leaf counts, leaf-polynomial count, grouped roots, closest root, owner sets, target retention, and all fallbacks.

### 12.5 P-D2.0b gate

The stage passes only if:

1. all 108 `(18 cases × 6 variants)` traversals reproduce P-D1.6's grouped roots, closest root, and exact owner sets;
2. every constructed target is retained;
3. P-D1.5 and P-D1.6 oracle groups agree for all 18 cases;
4. no event/node mapping, denominator, numerical, or resource fallback occurs;
5. every child interval is contained in its parent interval up to outward endpoint representation; and
6. repeated replay produces byte-identical canonical records.

Report paired node/leaf/interval counts as an early mechanism diagnostic, but make no usefulness decision from this single window. P-D2.0c owns the frozen 864-case gate.

## 13. Execution record

### 2026-08-24 — P-D2.0a complete

Implemented the analytic closed-interval primitives in `scripts/p_d2_residual_clipping.py` with focused tests in `scripts/test_p_d2_residual_clipping.py`.

The CPU reference provides:

- a generic affine-band clip;
- endpoint-form affine interpolation without a rounded `value1-value0` pre-step;
- `MM-test` and `MM-clip`;
- `FO-CW-test` and `FO-CW-clip`, including the exact binary64-rational value of $E_{CW}=\sum_i|g_i|\epsilon_i$;
- conservative intersection of two clip results for the future hybrid predicate; and
- explicit validation of finite inputs, ordered bands/intervals, and nonnegative uncertainty radii.

For this mathematical CPU reference, every finite binary64 input is interpreted as an exact rational. Clip roots are solved in rational arithmetic and only the returned parameter endpoints are rounded outward. This is intentionally not the proposed GPU implementation or its cost model. P-D3 must replace it with a packed floating-point policy and validate that policy against this reference.

The frozen analytic cases cover constant/parallel inside and outside, reversed direction, tangent/point intersections, point input intervals, nearly parallel endpoint differences, nonrepresentable clip roots, componentwise tube widening, boolean-test versus interval-clip behavior, and hybrid intersection.

Verification:

- P-D2.0a: 14/14 tests pass;
- P-D1 regression: 29/29 currently discovered P-D1 tests pass; and
- no P-D1 producer or artifact was changed.

This stage establishes only the predicate contract. It contains no hierarchy replay, practical-case work reduction, cost result, or paper-speed evidence.

**Historical stop point, now cleared:** P-D2.0b was not yet implemented at this checkpoint; its fixture mapping, hierarchy reconstruction, trace schema, and root/owner assertions were frozen before the implementation recorded below.

### 2026-08-24 — P-D2.0b complete

Authoritative passing run: `p-d2-0b-ec2b12fcdf82` under `experiments/p_d2_anchor_replay/`.

The first complete replay, preserved as failed run `p-d2-0b-a07134967c65`, lost two tolerance-owned microtriangles for one `S0-affine/grid-corner-hit` under `FO-DIR-clip`. It retained the physical root and every constructed target, but failed the frozen exact-owner gate. The failure exposed a hierarchy-construction defect: the previous bottom-up fold performed several rounded operations and then widened by only one ULP. The anchor's float audit permitted a positive residual violation of `1.66e-16` under its old `2e-12` tolerance; the wide componentwise tube masked that defect while the much tighter direct certificate exposed it.

The defect was corrected at the hierarchy producer. Leaf and parent envelopes now compute their centered residual ranges exactly over the binary64 inputs using rational arithmetic, then round the stored radius upward with one additional operational ULP. An inductive exact-rational audit checks leaf affine vertices and every child slab against its parent. The corrected anchor hierarchy hash is `3375ab0f46fe6f89c18c67b932280da16733276307bba2c59da1f93a3de986fa` and passes that exact enclosure audit.

The hierarchy-dependent P-D1.2 sweep was regenerated as `p-d1-2-ec0dd6a036f5`: all 15,552 traces pass, and the earlier negative segmentation conclusion is unchanged (`5` pooled CW split events versus `3` DIR, with median `0` for both). Its aborted 180-second invocation is preserved as `p-d1-2-ec0dd6a036f5.partial-timeout`; it is not an experiment result. P-D1.3 through P-D1.6 remain valid because their persisted tubes, UV events, and leaf roots depend on the unchanged shell curve, ray records, and displacement microtriangles rather than hierarchy residual radii.

The final P-D2.0b gate passes all 108 traversals:

- all six variants reproduce every P-D1.6 grouped root, closest root, and exact owner set;
- every constructed target is retained;
- P-D1.5 and P-D1.6 oracle groups agree;
- all child/predicate intervals are contained;
- the exact hierarchy audit passes;
- no predicate or leaf fallback occurs; and
- repeated replay is byte-identical.

Anchor mechanism counts relative to `MM-clip` are:

| Variant | Node predicates | Node ratio | Leaves/cubics | Leaf ratio |
|---|---:|---:|---:|---:|
| `MM-test` | 313 | 1.208 | 47 / 94 | 1.000 |
| `MM-clip` | 259 | 1.000 | 47 / 94 | 1.000 |
| `FO-CW-test` | 275 | 1.062 | 37 / 74 | 0.787 |
| `FO-CW-clip` | 235 | 0.907 | 37 / 74 | 0.787 |
| `FO-DIR-clip` | 234 | 0.903 | 37 / 74 | 0.787 |
| `HYB-clip` | 229 | 0.884 | 35 / 70 | 0.745 |

This is encouraging integration evidence: clipping propagation matters, first-order correlation removes downstream leaf work on the anchor, and the hybrid is strongest. It is not the P-D2 usefulness verdict because it covers one window and only 18 rays. The direct oracle saves no additional leaf polynomial and only one node versus `FO-CW-clip`; unless P-D2.0c finds a broader gain of at least `5%`, it will be dropped.

Verification after the correction: 29/29 P-D1 tests and 20/20 P-D2 tests pass.

**Historical stop point, now cleared:** P-D2.0c was unimplemented at this checkpoint. Its corrected hierarchy/source manifest, full-corpus output schema, strata summaries, break-even scenarios, and exact automatic gate calculation were frozen before the execution recorded below.

## 14. Frozen P-D2.0c full-corpus decision

P-D2.0c is the first scientific usefulness decision for the ray-side first-order predicate. It reuses the P-D2.0b replay without changing any predicate, interval, event, or root policy.

### 14.1 Corrected immutable manifest

Consume and hash:

- P-D0 asset/window metadata from `p-d0-71627ba7b07f`;
- rays and corrected hierarchy hashes from `p-d1-2-ec0dd6a036f5`;
- unchanged certified tubes from `p-d1-3-62f0f7416547`;
- unchanged level-0-through-6 events from `p-d1-4-dd1251769c51`;
- the authoritative full root/owner oracle from `p-d1-6-e254f252e010`; and
- the passing P-D2.0b result `p-d2-0b-ec2b12fcdf82` as the replay-schema anchor.

Rebuild all 48 hierarchies from the frozen P-D0 windows. Every rebuilt hash must equal the regenerated P-D1.2 hierarchy record, and every hierarchy must pass the inductive exact-rational enclosure audit. Reconstruct exactly 864 tubes/cases and require the existing `(window,shell,ray-family)` uniqueness.

### 14.2 Frozen cohorts and strata

Primary cohorts are:

- `coherent`: `front-hit`, `oblique-hit`, `grid-corner-hit` (432 cases);
- `boundary-stress`: `grazing-hit`, `proxy-edge-hit` (288 cases);
- `near-miss`: `near-miss` (144 cases); and
- `all`: all 864 cases.

Report paired totals and p50/p95/p99/max per ray by cohort, 12 source asset IDs, 10 material identities, shell family, ray family, bit depth, source resolution, and P-D0 content regime. The predeclared `7/12` gate refers to source asset IDs, including the three separately decoded Rock 05 resolutions; also report the 10-identity aggregation without using it for the gate.

### 14.3 Frozen records

Persist:

1. the 48 hierarchy hash/exact-audit records;
2. the 864 root-case manifest;
3. one compact traversal record per `(case,variant)` (5,184 total);
4. every evaluated-node trace from the unchanged replay;
5. one paired per-case record containing all variant node/leaf ratios to `MM-clip`;
6. aggregate strata and automatic gate terms; and
7. a deterministic replay audit for one content-hash-selected case per source asset ID.

The compact traversal record adds `asset_id`, `material_identity`, `bit_depth`, source resolution, content regime, cohort, and oracle comparisons. No closest-hit early termination is introduced: all variants continue to recover the same complete canonical root/owner groups.

### 14.4 Cost scenarios and break-even

Normalize `c_N,MM=1` and freeze

$$
c_L/c_N\in\{2,4,8,16\}.
$$

For each candidate and cohort, report the previously defined `delta_max` for a projected `10%` internal win. The usefulness gate requires positive `delta_max` at the two middle scenarios `4` and `8`; the endpoint scenarios are sensitivity analysis. This remains predictive operation accounting, not timing.

### 14.5 Exact automatic decision

Evaluate `FO-CW-clip` and `HYB-clip` independently against `MM-clip` on the coherent cohort. A candidate passes only if:

1. pooled leaf-polynomial ratio is at most `0.85`;
2. it has strictly fewer pooled leaf polynomials on at least 7 of 12 source asset IDs;
3. pooled node-predicate ratio is at most `1.10`;
4. no source asset ID has either a leaf or node ratio above `1.20` in the coherent cohort; and
5. `delta_max>0` for `c_L/c_N=4` and `8`.

If `FO-CW-clip` passes, the pure first-order branch advances. If only `HYB-clip` passes, advance an explicitly hybrid branch. If both pass, carry both into payload replay and let measured GPU cost select. If neither passes, stop the first-order ray-acceleration claim before P-D2.1/P-D3.

Retain `FO-DIR-clip` only if its coherent pooled node or leaf-polynomial count is at least `5%` below `FO-CW-clip`; otherwise drop it after reporting the negative ablation.

#### Directional numerical-ownership guard frozen after the first full run

Failed full run `p-d2-0c-3b6b77577951` completed all 5,184 traversals and passed hierarchy hashes/enclosures, target retention, interval containment, fallback, and determinism checks, but `FO-DIR-clip` omitted tolerance-owned adjacent microtriangles for two `S0-affine/grid-corner-hit` cases. Both physical roots were unchanged. The exact directional curve certificate was valid for the real rational curve, but narrower than P-D1.6's numerical closed-owner contract near a grid face.

The rerun therefore adds one correctness-only guard, frozen before examining usefulness counts: at an active interval endpoint, if the certified chord centerline lies within `ROOT_COORDINATE_TOLERANCE + epsilon_axis` of any node UV face, `FO-DIR-clip` uses the certified componentwise projection for that node. Otherwise it uses the tighter of the direct and componentwise bounds as before. Record every guard activation separately; it is not a denominator/resource fallback. This guard can only retain more work and cannot improve the directional ablation. No other variant or gate changes.

All scientific decisions are subordinate to the correctness gate: every variant must match P-D1.6 grouped roots, closest root, and owner sets; retain every target; contain every propagated interval; match every corrected hierarchy hash; pass every exact hierarchy audit; and take no fallback.

### 14.6 Outputs and stop rule

Generate one four-panel mechanism figure: coherent per-asset leaf ratios, cohort node/leaf ratios, coherent per-case node-versus-leaf scatter, and `delta_max` cost sweeps. The figure and counts may support planning language only. Do not write “faster” until the selected payload survives measured GPU replay and end-to-end timing.

Stop after recording the automatic decision. A passing decision authorizes writing—but not implementing—a separate P-D2.1 full-resolution/vectorized trace plan. It does not authorize CUDA/OptiX changes.

### 14.7 2026-08-24 — P-D2.0c complete; ray-acceleration branch stopped

Authoritative passing run: `p-d2-0c-47849fef3202` under `experiments/p_d2_full_corpus/`. The four-panel mechanism figure is `figures/p_d2_full_corpus/residual_clipping_p-d2-0c-47849fef3202` in PNG, PDF, and SVG form.

The run rebuilt and exact-audited all 48 frozen hierarchies, reconstructed all 864 cases, and completed 5,184 `(case,variant)` traversals. Every hierarchy hash, exact enclosure, grouped root, closest root, closed owner set, constructed target, propagated interval, no-fallback requirement, and deterministic replay check passes. The final P-D2 regression suite passes 25/25 tests.

The first complete artifact `p-d2-0c-3b6b77577951` is preserved as a failed correctness run. Its only discrepancies were two tolerance-owned adjacent microtriangles under `FO-DIR-clip` at `S0-affine/grid-corner-hit`. The frozen endpoint owner guard described above corrected those discrepancies. The guard activated 5,851 times in the passing corpus and made the direct directional variant identical to `FO-CW-clip` in pooled node and leaf counts. Direct directional clipping is therefore dropped.

The predeclared coherent-cohort usefulness gate fails for both candidates:

| Candidate | Node ratio to `MM-clip` | Leaf-cubic ratio | Winning assets | `delta_max`, cost 4 | `delta_max`, cost 8 | Decision |
|---|---:|---:|---:|---:|---:|---|
| `FO-CW-clip` | `0.9966` | `0.9674` | `7/12` | `-0.1900` | `-0.2831` | fail |
| `HYB-clip` | `0.9906` | `0.9622` | `7/12` | `-0.1778` | `-0.2642` | fail |

The candidates reduce coherent leaf cubics by only `3.26%` and `3.78%`, respectively, far short of the required `15%`. Both break-even margins are negative even before charging any additional first-order fetch or arithmetic cost. This rules out the current formulation as a credible route to the required `10%` internal traversal win.

The result is regime-dependent but does not rescue the primary hypothesis. Across all cases, `FO-CW-clip` uses `0.9702×` nodes and `0.9108×` leaf cubics, while `HYB-clip` uses `0.9422×` nodes and `0.8685×` leaves. The hybrid's larger reductions occur mainly in boundary-stress (`0.8813×` nodes, `0.7460×` leaves) and near-miss (`0.9049×`, `0.8091×`) cohorts; front-hit is exactly `1.0×/1.0×`, and most coherent per-asset ratios remain close to one. These are useful mechanism and limitation results, not an acceleration claim.

**Decision:** stop P-D2.1, P-D3, and P-D4 for the present ray-specific first-order speed branch. Do not port this predicate to CUDA/OptiX and do not implement external ray baselines for a performance claim based on it. Retain P-D1's certified nonlinear shell-ray, tube-supercover, ownership, and exact cubic-leaf construction as the ray application's technical contribution. Return to the shared-project plan and test whether the first-order hierarchy provides a decisive benefit for the area-sampling application. A future ray-performance branch requires a separately motivated hypothesis and a newly frozen gate; it must not reinterpret this result after the fact.

**Post-decision planning direction — 2026-08-24:** area sampling is temporarily deferred. Before accepting the broad first-order ray no-go, perform the bounded, plan-first representation audit in [[Plan — First-order ray representation oracle]]. It tests whether an optimal conservative single plane or a small multi-slope support envelope has substantially more pruning headroom than the current arithmetic-average slope. This does not reopen P-D2.1/P-D3/P-D4: the oracle must first establish a new, positive operation-count hypothesis under separately frozen gates.

**Representation-audit resolution — 2026-08-24:** the full oracle run `p-d2-r0-05a8b7b3f3c4` passes all correctness checks but confirms the no-go. `HYB-OPT1` retains `0.9611×` coherent leaf work, and an idealized recertified support hull retains `0.9548×`; both have negative break-even margins and fail their frozen gates. Therefore do not reopen P-D2.1/P-D3/P-D4 for optimized planes, query-adaptive slopes, or finite affine support-strip dictionaries on this topology. The scalar min/max-DDA architecture remains logically separate and would require a new, explicitly frozen comparison plan.

## 15. Paper checkpoint

P-D2 supports the formulation and a negative performance conclusion, not a first-order ray-acceleration contribution. Safe result language is:

> We formulate scalar min/max and plane-compensated residual tests as comparable one-dimensional interval clips over the same certified shell-ray tube. On the frozen practical corpus, both first-order candidates preserve the exhaustive root and ownership oracle but reduce coherent leaf work by less than four percent and fail the predeclared break-even gate.

Do not write “faster,” “materially fewer DDA steps,” or “outperforms TFDM/Ogaki” from P-D2. GPU timing is no longer the next step for this formulation because its zero-overhead predictive gate already fails.

---

Related: [[Plan — P-D1 certified tube-supercover DDA reference]] · [[Plan — Practical first-order DDA ray traversal]] · [[Guide — Eurographics paper draft]]
