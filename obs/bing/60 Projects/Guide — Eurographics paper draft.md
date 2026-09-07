---
title: Guide — Eurographics paper draft
tags: [writing-guide, eurographics, paper, theory, experiments, ablations]
status: active
created: 2026-08-23
updated: 2026-09-07
parent: "[[Project — Conservative first-order queries on displacement maps]]"
authoring-model: user-writes-full-draft
---

# Guide — Eurographics paper draft

> [!important] Current application organization — 2026-09-07
> The maintained authoring order is **Application 1: Area/Sampling**,
> **Application 2: Ray Tracing**, and **Application 3: pending**. The complete
> ray-tracing record through P30 is [[Paper — Application 2 — Tessellation-free
> ray tracing]]. Older ray-first numbering and earlier negative checkpoints are
> retained as development history; they do not override the current chapter or
> its later frozen evidence.

> [!success] P19 ray-only first-order evidence — 2026-09-05
> P19 now supplies the previously missing same-renderer scalar/first-order
> ablation for a narrow slope-dominant regime. A lean packed first-order 4x4
> hierarchy for secondary-closest and visibility rays measures `0.9505x` the
> scalar full-frame geomean over three maps and five geometries, is faster in
> 14/15 cases, and produces byte-exact scalar/first-order outputs. A strict
> residual gate keeps cobble and the five sensitive rough regressions on the
> scalar P18 path. This supports first order as a regime-specific ray
> acceleration, not a universal renderer label and not a revival of the
> rejected shared ray-plus-area performance thesis. See
> `docs/p19_first_order_ray_tracing.md`.

> [!warning] P18 renderer claim audit — 2026-09-05
> The later strict linear-only renderer materially supersedes the old Mode-1
> implementation measured on 2026-08-30, but it does **not** revive a broad
> first-order or universal-speed claim. Its current name is **event-segmented
> linear shell-ray DDA**. The first-order hierarchy is preprocessing-selected
> supporting acceleration and is not selected on twisted + rocky-trail 4K.
> The latest capacity and matching-leaf specialization removes that case's
> visible secondary-ray leak and is `0.9888x` Mode-0 frame time in the final
> 10x30 run. A short four-case regression is
> faster on curved, Bunny, Armadillo, and torus while passing all accuracy
> gates. Do not call the method `constant affine`, do not
> call its midpoint chord bound certified, and do not claim TFDM superiority
> before a same-quality full-renderer comparison. The authoritative current
> wording and complete pipeline are in `docs/p18_method_claim_audit.md`.

> [!warning] Do not merge the two paper stories
> The broad P18 speed evidence supports an **event-segmented linear shell-ray
> DDA** story. P19 separately establishes a residual-gated first-order
> secondary-ray benefit on slope-dominant maps. Twisted rocky-trail, Bunny,
> Armadillo, and the rough regression still select scalar, so their gains must
> not be credited to first order. Likewise, the failed area-sampling result is
> not repaired by the narrower ray ablation.

> [!warning] Final ray-performance decision — 2026-08-30
> The completed performance-recovery pass does not support a competitive ray-speed claim. Final Mode 1 path time is `18.39x` Mode 0 on twisted-rock and `5.76x` on curved-cobble; the same-surface A2 candidate is `1.261x` NRT-QT on held-out ordinary rays despite a `0.383x` front-ray result. First-order and even ideal support-hull pruning fail break-even. Use [[Note — Final ray application decision after Mode 1 recovery]] as the current claim boundary; do not draft TFDM/RMIP superiority or universal-speed language.

> [!note] Current drafting handoff
> For the current evidence cutoff through the completed P-D2 representation oracle, use [[Note — EG draft handoff after P-D1]]. It lists safe wording, authoritative artifact IDs, the final affine-support ray no-go, and the newly frozen scalar ray-architecture experiment whose results remain pending.

> [!abstract] Purpose
> This is a guide for the author's own Eurographics draft, not a substitute manuscript. It records the defensible paper story, proof obligations, evidence gaps, experiment/ablation checklist, and prompts for raw author notes. Quantitative claims remain placeholders until their experiment gates pass.

> [!warning] Active scope
> Catmull–Clark, coarse-to-residual conversion, “thin-shell compression,” and universal fastest-ray claims are not part of the active paper. Older files in `docs/paper_contributions.md`, `docs/paper_notes.md`, and `docs/plan_eg2027.md` are historical.

## 1. Current paper thesis

Working title:

> **A Conservative First-Order Displacement Hierarchy for Visibility and Surface Measure**

The paper begins from a structural limitation of zero-order displacement hierarchies. A scalar min/max interval contains both unpredictable residual variation and predictable variation caused by local slope. It can therefore remain wide over a smooth tilted region. At the same time, surface-area queries require height derivatives rather than height alone.

The common idea is to store a conservative local height plane, its residual, and gradient uncertainty. The two initial consumers are deliberately different:

1. **ray visibility**, which composes a certified nonlinear shell-ray tube with a tilted node slab and exact analytic leaves; and
2. **surface measure/area sampling**, which propagates the same height/gradient information into bounds on the induced metric and area density.

The intended thesis is not “segments plus DDA.” It is:

> One conservative first-order displacement hierarchy can support visibility and surface-measure queries without displacement-resolution tessellation, while retaining enough height–gradient correlation to outperform zero-order bounds in slope-dominated regimes.

The final sentence must remain conditional until both applications and the combined memory/build/update comparison exist.

## 2. Recommended section-level argument

### 2.1 Introduction

Develop this sequence:

1. Fine displacement maps compactly represent geometric detail, but geometric queries still require conservative acceleration structures.
2. Most specialized structures keep a zero-order range or precompute a final scalar distribution, discarding predictable local slope or duplicating preprocessing across queries.
3. A first-order node separates a local plane from a conservative residual and retains gradient uncertainty.
4. Ray tracing and area sampling consume different projections of the same information.
5. The paper derives, implements, and evaluates the shared hierarchy and the two consumers.

Avoid opening with Ogaki's curved ray or DDA. Those are introduced only after the common representation is clear.

### 2.2 Related work

Organize around four boundaries:

- tessellation-free ray intersection: Ogaki, TFDM, RMIP, and PDM;
- dense triangles, hardware micromesh/DMM, and preprocessing-based alternatives;
- conservative centered forms, affine arithmetic, Taylor models, gradient pyramids, and non-conservative gradient-mipmap precedents such as LEADR; and
- surface-area/product sampling, scalar area distributions, dense alias/CDFs, mesh-light trees, and ray-cast uniform surface sampling.

Until the focused centered-form/tangent-bound audit is complete, use “we believe this composition is new” rather than claiming the first Taylor-like displacement pyramid.

### 2.3 Surface and query model

Freeze the common analytic surface:

$$
F(a,b,h)=P(a,b)+hN(a,b),
\qquad
S_\tau(u,v)=F(a(u,v),b(u,v),H_\tau(u,v)),
$$

where $H_\tau$ is affine on each fixed texture microtriangle. State:

- shell regularity;
- valid texture domain and filtering contract;
- shared-edge and root ownership;
- exact fallback conditions; and
- that ray hits, derivatives, area, and sampling PDFs refer to this same surface.

### 2.4 Conservative first-order hierarchy

Introduce the logical node:

$$
|h-(h_0+g^T(x-x_0))|\le r,
\qquad
|h_u-g_u|\le\rho_u,
\qquad
|h_v-g_v|\le\rho_v.
$$

Present:

- exact two-microtriangle leaf construction;
- quadrant-aware conservative parent fold;
- recovered scalar min/max view;
- the ray-specific four-value view; and
- the smooth-field intuition: first-order residual $O(s^2)$ versus zero-order slope variation $O(s\|\nabla h\|)$ under the declared regularity assumptions.

Both application figures should point visibly to the same stored fields.

### 2.5 Certified visibility query

Derive:

$$
q(h)=\left(\frac{U(h)}{D(h)},\frac{V(h)}{D(h)},h\right),
$$

then cover:

1. proxy/ray/turning events and valid intervals;
2. denominator conditioning and exact fallback;
3. Bernstein-certified componentwise chord tubes;
4. conservative UV ownership;
5. first-order tube–slab rejection;
6. optional lazy node-local linearization and direct $g^T\Delta x$ certification; and
7. exact cubic leaf roots and the closest-hit invariant.

Keep two roles separate in the prose: the UV tube proves spatial ownership; the directional certificate only tightens the height test.

### 2.6 Surface-measure query

Derive the displaced metric and the area density

$$
J(u,v)=\sqrt{\det G(u,v)}.
$$

The section must eventually freeze one actual sampling algorithm—hierarchical proposal, rejection, or adaptive integration—and derive its pointwise parameter-space and area-space PDFs. Conservative weights alone are not a correctness proof for a sampler.

### 2.7 Implementation

Explain:

- the logical six-value shared node versus packed application views;
- outward rounding and quantization widening;
- exact fallback and resource-exhaustion behavior;
- shared-edge ownership;
- CPU recursive traversal as the reference; and
- GPU traversal/DDA as a performance policy rather than the proof foundation.

### 2.8 Evaluation

Answer in this order:

1. Are the enclosures and queries correct?
2. Are first-order residuals tighter at useful levels?
3. Does that reduce ray work with segmentation held fixed?
4. Are certified ray tubes affordable?
5. Does the gain survive GPU fetch/arithmetic cost?
6. Does the shared hierarchy support correct and competitive area/product sampling?
7. Is one shared structure preferable to two specialized structures in memory, build, update, and combined workloads?

### 2.9 Limitations

Discuss high-frequency maps, thick or poorly conditioned shells, grazing rays, larger node bandwidth, static scenes favorable to DMM/dense triangles, and static sampling workloads favorable to fused scalar $EJ$ distributions. Conversion from arbitrary source geometry and subdivision surfaces remains out of scope.

## 3. Theory and proof obligations

| ID | Statement to establish | Likely location | Status |
|---|---|---|---|
| `T1` | The four-corner residual construction encloses both affine height microtriangles. | Main paper | Derived; formal write-up needed |
| `T2` | The quadrant-aware fold conservatively encloses every child height and gradient contract. | Main/supplement | Implemented; formal proof needed |
| `T3` | Scalar min/max recovery from $(h_0,g,r)$ is conservative. | Main paper | Straightforward |
| `T4` | Under bounded Hessian, residual width is $O(s^2)$ while zero-order slope variation is generally $O(s)$. | Main paper | Assumptions/write-up needed |
| `T5` | $U,V,D,T$ and the event partition cover every valid shell-ray interval with closed endpoint ownership. | Main/supplement | Derived; interval-root audit remains |
| `T6` | The Bernstein/denominator construction encloses the curve–chord error. | Main/supplement | Implemented in CPU reference |
| `T7` | Inflated UV clipping cannot omit an entered node, and the widened first-order slab cannot reject a true hit. | Main paper | Derived; formal proof needed |
| `T8` | $E_{dir}\le E_{cw}$ for exact bounds; the numerical implementation safely chooses the tighter certified enclosure. | Main paper | Exact relation clear; implementation policy to freeze |
| `T9` | Substitution into an affine height microtriangle yields the complete cubic leaf equation. | Supplement | Implemented/cross-checked |
| `T10` | With successful certificates or declared exact fallback, traversal returns the exhaustive oracle's closest hit. | Main theorem | Depends on T5–T9 and ownership |
| `T11` | The joint node propagates conservatively to $G$, $\det G$, and $J$. | Main/supplement | Not implemented |
| `T12` | The chosen area sampler has a normalized PDF and yields an unbiased estimator. | Main paper | Blocked on sampler choice |

Floating-point obligations must also cover event isolating intervals, outward rounding, packed-node widening, shared edges, and resource exhaustion falling back rather than missing.

## 4. Experiment and ablation checklist

### 4.1 Main figures

1. Unified method overview: common hierarchy feeding ray visibility and area sampling.
2. Per-level residual/minmax width for all nodes and ray-visited nodes.
3. Paired fixed-segmentation `G-MM` versus `G-FO` nodes and exact leaves.
4. Regime heatmap over map class, shell variation, and incidence.
5. Certification Pareto: tube tolerance versus segments, nodes, leaves, and fallbacks.
6. Directional mechanism: $E_{cw}/E_{dir}$ and `L-CW`/`L-DIR` paired work.
7. GPU quality–time–memory comparison against external ray baselines.
8. Area-bound tightness and sample-distribution correctness.
9. Equal-time area/product-sampling variance and update latency.
10. Combined visibility-plus-sampling memory/build/update Pareto.

### 4.2 Essential ablation table

Ray variants:

- fixed-$N$ or non-certified segmentation diagnostic;
- global certified segments + min/max;
- global certified segments + first-order slab;
- lazy first-order + componentwise projection;
- lazy first-order + direct directional projection; and
- final GPU traversal/DDA policy.

The first two paired comparisons isolate the surface hierarchy; the two lazy comparisons isolate the directional ray-side benefit.

### 4.3 External ray table

- Ogaki/upstream nonlinear traversal;
- TFDM;
- RMIP;
- PDM;
- dense triangle BLAS;
- DMM when available; and
- the proposed method at matched correctness/quality.

### 4.4 Sampling table

- uniform parameter sampling;
- specialized scalar $J$ hierarchy;
- specialized fused $EJ$ hierarchy;
- dense alias/CDF;
- relevant mesh-light tree;
- Ling et al. ray-cast uniform sampling; and
- shared first-order hierarchy.

### 4.5 Shared-system table

Report resident bytes, build scratch, preprocessing time, full-map update, localized update, instance reuse, and the combined visibility-plus-sampling workload for one shared hierarchy versus two specialized structures.

## 5. Claim ledger

| Candidate claim | Current status | Allowed language now |
|---|---|---|
| Analytic shell/microtriangle model, rational ray, first-order leaf/fold, and certified tube–slab formulation exist in a CPU reference. | Supported | “We develop and implement…” |
| P3's 896-ray procedural/mixed suite has zero candidate omissions/hit mismatches and agrees with its sampled independent high-precision cubic checks. | Preliminary supported | State exact legacy-suite scope and 13 exact fallbacks |
| Initial ramps reduce nodes and smooth maps reduce exact leaves; checkerboards do not benefit. P3 pools to 2.9% fewer nodes and 13.9% fewer exact leaf-interval tests. | Preliminary small corpus | “Initial CPU counts indicate…”; do not imply timing |
| Direct tube–slab rejection materially reduces coherent practical traversal work. | Rejected by P-D2.0c | `FO-CW` leaves `0.9674×`; hybrid `0.9622×`; both fail the frozen gate |
| Better plane fitting or a small affine support dictionary rescues coherent ray traversal. | Rejected by P-D2-R0 upper bound | Optimized hybrid leaves `0.9611×`; ideal support hull `0.9548×`; both gates and break-even tests fail |
| Directional certification broadly reduces ray knots or traversal work. | Rejected | Segmentation evidence was negligible; corrected P-D2 work is identical to componentwise clipping |
| The method is GPU competitive or faster than ray baselines. | Unsupported | Do not claim |
| The hierarchy supports correct and competitive area/product sampling. | Not implemented | Keep as planned application |
| One shared hierarchy gives a superior combined Pareto point. | Central unsupported hypothesis | Do not put in final abstract yet |

Current contribution verdict: the hierarchy construction and certified ray composition can be written as technical methods, with the CPU reference and exact-oracle validation stated in their precise scope. The present first-order ray-acceleration hypothesis has failed its full practical-corpus gate and should not proceed to GPU or external performance baselines. The unifying multi-query thesis is now contingent primarily on a strong area/metric application; otherwise the project should be framed as a narrower certified ray-query method rather than a shared first-order performance framework.

### Frozen Step 1 update — 2026-08-23

Replace the earlier small-corpus work-count sentence when drafting results. The 45-case frozen reference has zero active root-set mismatches and zero owner omissions. Across all cases, G-FO/G-MM is `0.937` for node tests and `0.757` for exact leaf/segment tests; across the coherent cohort it is `0.897` and `0.691`. Four analytic/procedural maps, the synthetic file-backed R00 test map, and three practical Poly Haven maps satisfy the per-map work gate. These are operation counts at shared segmentation, not timings and not comparisons with TFDM/Ogaki/RMIP/PDM/DMM.

The frozen report does **not** authorize a positive overall ray claim. Only `9/27` coherent cases meet the predeclared useful-level residual-width threshold, and ordinary denominator-root fallback is `2.61–2.93%`, above the frozen limits. Segment tails for active rays are good (`p99=4`, `max=6`). Describe the result as strong conditional evidence for orientation-aware tube–slab pruning plus a failed broad tightness hypothesis and an overconservative singularity adapter.

Author task A2 is now due for the Step 1 figures. For each of the case heatmap, paired-work plot, visited-tightness plot, and segment-tail plot, write four sentences: prediction, observation, mechanism, and allowed/forbidden claim. In particular, explain in your own words why a tilted slab can reject candidates even when its scalar residual interval is wider than min–max; this distinction is likely central to a credible revision of the ray story.

Safe current summary:

> We develop a conservative first-order displacement hierarchy and derive its composition with certified shell-ray tubes and displaced-surface metric bounds. Initial CPU-reference experiments validate the ray formulation and indicate reduced traversal work in smooth slope-dominated regimes.

### P-D1.5 theory checkpoint — 2026-08-24

The triangle-proxy ray method is now safe to draft through the leaf equation, but not through performance claims. The corrected CPU chain has independently validated rational shell inversion, analytic chord tubes, closed recursive coverage, exact-event supercoverage, and the represented shell-space leaf root on one full practical window. The key leaf derivation is

$$
G(h)=\alpha U(h)+\beta V(h)+\gamma hD(h)+\kappa D(h)=0,
$$

for microtriangle plane `αu+βv+γh+κ=0`; this is cubic or lower and is not the legacy planar world-microtriangle test. P-D1.5's event/recursive/exhaustive/Decimal roots agree on all 18 implementation cases, including six closed owners at a constructed grid-corner hit.

Author task A5 is now due. Write 400–600 raw words covering:

1. why the shell ray is curved in `(u,v,h)` even though the world ray is straight;
2. why a certified axis-aligned UV tube plus closed event ownership cannot omit a leaf visited by that curve;
3. how substituting the rational shell ray into one affine displacement microtriangle produces `G(h)` above; and
4. why exact leaf roots plus conservative fallbacks are different from intersecting an approximate world-space microtriangle.

Also write the geometric intuition for `T10`: enclosure gives candidate completeness, closed ownership preserves boundary hits, and the shared exact leaf equation recovers the exhaustive closest root. State explicitly that P-D1 is CPU correctness evidence; it does not show the first-order hierarchy or DDA is faster.

### Final P-D1 writing update — 2026-08-24

P-D1 now supports a stronger but still carefully scoped methods/correctness paragraph: the final 864-case practical run has zero event/recursive/exhaustive/Decimal root disagreement, retains every constructed hit, and reports no practical fallback. Use run `p-d1-6-e254f252e010`, not the superseded anchor IDs, for final P-D1 numbers.

Add one numerical-robustness paragraph to the supplemental draft. Explain that two initially agreeing float slab implementations shared cancellation and that residual-based root stopping truncated proxy-boundary intervals; independent exact-event and Decimal paths exposed both. The final CPU oracle uses exact rational event/coverage arithmetic plus position-terminated, Decimal-audited low-degree roots. This is valuable reproducibility evidence, but do not present exact rational CPU arithmetic as the final GPU implementation.

Allowed sentence now:

> Across 864 practical triangle-proxy shell rays, our closed event supercover and represented cubic leaf solver reproduce the exhaustive oracle's canonical root and owner sets, including all constructed edge/corner hits.

Still forbidden: any sentence claiming the first-order node accelerates traversal. P-D1 tests certified traversal correctness; the completed P-D2 decision below rejects the current acceleration hypothesis.

### P-D2.0c contribution checkpoint — 2026-08-24

Authoritative run `p-d2-0c-47849fef3202` passes every correctness and reproducibility check over 864 practical cases and 5,184 traversals. The primary coherent operation ratios are `0.9966×` nodes / `0.9674×` leaves for componentwise first order and `0.9906×` / `0.9622×` for the hybrid. Both fail the frozen `0.85` leaf-work threshold and both have negative break-even overhead budgets at the required cost scenarios.

This is a go/no-go result, not a disappointing preliminary number to optimize around. Stop the present P-D2.1/P-D3/P-D4 ray-speed path. The stress and near-miss cohorts show stronger conditional pruning and may be discussed as mechanism evidence, but they do not authorize a positive ray-performance claim. Drop the direct directional predicate.

Author task A6 is now due. Write 250–400 raw words with: the original prediction; the exact coherent observation; why the single-window anchor was misleading; why negative break-even before payload cost rules out a GPU rescue; and the scope change from “first-order ray acceleration” to “certified nonlinear ray application of a hierarchy whose performance value must be established by area sampling.”

Safe current ray summary:

> We derive and validate a tessellation-free triangle-proxy shell intersection method based on certified piecewise enclosures, closed hierarchy ownership, and exact cubic leaves. A frozen practical-corpus ablation shows that adding our current first-order residual clip preserves correctness but does not materially reduce coherent traversal work, so we do not claim ray-speed superiority from that representation.

### P-D2-R0 representation-oracle checkpoint — 2026-08-24

The final upper-bound audit removes “perhaps the average slope was poor” as the explanation for the negative coherent result. On 864 exact-replayed cases, an optimized single plane still retains `96.11%` of scalar coherent leaf work. An ideal recertified convex support hull—strictly stronger than any finite set of affine support strips over the same node domain and counted without its true facet cost—retains `95.48%`. Both have negative projected overhead margins and fail their frozen gates.

This result belongs in the representation ablation or limitations section, not the abstract contribution list. It is useful because it establishes a principled stopping point: do not spend the paper schedule on `K=2/4` slope dictionaries or a GPU implementation of affine first-order clipping. Preserve the separate question of whether the certified piecewise shell-ray plus scalar min/max-DDA architecture is competitive with Ogaki/TFDM.

Author task A7 is now due. Write 200–300 raw words answering: why a median static width reduction near 36–42% did not remove coherent traversal work; why the support hull is a meaningful upper bound; which regimes still benefited (grazing, boundary stress, near miss); and why those selected regimes do not reverse the frozen decision.

### Active scalar architecture experiment — planning only

Use [[Plan — Certified ray architecture comparison]] for the next experiment contract. The key causal sentence for the methods/experiments draft is: “We hold the exact triangle-shell surface, scalar min/max hierarchy, rays, and cubic leaf fixed, and vary only nonlinear node traversal versus certified tube-supercover DDA.” Keep TFDM in a separate quality-matched comparison because its native local surface is not presumed identical.

Do not reuse the historical Mode 1 timing table as a result. It is based on heuristic centerline segments and planar world leaves and has nonzero disagreement. A0 run `ray-a0-smoke-d9bbfd971b2f` now validates the experimental plumbing—20 warmups, 100 measured GPU-event launches, five randomized trials, content hashes, explicit ray counts, and TFDM GPU initialization—but it is still not a method comparison. Draft the same-surface ablation, TFDM quality normalization, ray workloads, and A0–A4 protocol, while leaving every performance claim blank until the certified GPU path passes the CPU differential oracle.

### Author task A8 — write now

Write 150–250 raw words explaining why the paper uses two separate comparisons: (1) a same-surface causal ablation against `NRT-QT`, and (2) an equal-quality Pareto comparison against native TFDM. Include one sentence explaining why the A0 M1-HEUR/NRT-QT smoke ratio is excluded: the two paths do not yet share the certified exact leaf contract.

### A2 same-surface GPU checkpoint — 2026-08-25

The certified GPU ray path is now complete enough for a methods, correctness, and limitations draft. Runs `ray-a2-5-hyb-stream-e8b2283abbb2` and `ray-a2-3-nrt-qt-ef2448723ee6` reproduce the same packed oracle on all 864 rays. The streaming candidate uses one certified segment per ray, zero fallback, p99 40 min/max nodes, and p99 26 exact microtriangle tests. Removing materialized correctness arrays reduces local memory from 10,144 to 880 bytes/thread.

The frozen performance call is negative/mixed. Candidate/`NRT-QT` median time is `0.798x` pooled and `0.383x` for held-out front rays, but `1.261x` for held-out ordinary rays, `1.279x` oblique, and `1.624x` grazing. The predeclared A2 gate fails, so do not write a general nonlinear-traversal speed claim and do not present A0 TFDM probes as a candidate comparison.

Author task A9 is due. Write 300–450 raw words with four parts: the predicted advantage from replacing repeated nonlinear node bounds; the exact packed-surface correctness result; the pooled/front wins and ordinary/grazing losses; and why the result triggers the planned stop before OptiX/TFDM timing. End by explaining that ray visibility remains a certified application, while practical first-order value must now come from area/product sampling. Use [[Report — A2 same-surface GPU comparison]] for exact numbers.

### Final S3 writing update — 2026-08-25

The two-application shared-performance thesis is now rejected by its frozen gate. Do not write the existing abstract/introduction claim that the first-order representation outperforms zero-order ray structures or provides a superior shared area sampler.

Safe to draft:

- the exact triangle-shell surface and its analytic tangents/Jacobian;
- the conservative Taylor height/gradient hierarchy and certified area-cap composition;
- proposal, explicit-null, and repeat-until-accepted probability semantics;
- the negative mechanism ablation: local child cap normalization does not telescope to descendant leaf mass;
- the repaired leaf-CDF result as an ablation, with its per-instance storage charged;
- the certified ray correctness result and mixed A2 regime behavior.

Required S3 numbers from `area-s3-full-f245aff67ea7`, on 69 nontrivial ordinary surfaces:

- `FO-PROP/UV`: geometric-mean variance `16.342x`, win fraction `5.8%`;
- `FO-PROP/J-PROP`: geometric mean `2.872x`, p90 `8.869x`;
- `FO-LEAFCDF/UV`: geometric mean `0.1377x`, win fraction `100%`;
- `FO-LEAFCDF/J-LEAFCDF`: geometric mean `1.4729x`, p90 `2.1785x`;
- `FO-LEAFCDF/DENSE-CELL`: geometric mean `2.4497x`, zero wins;
- logical one-instance storage: FO leaf-CDF `185928` B, J leaf-CDF `54864` B, dense-cell `60328` B under the combined-query accounting.

Author task A10: write 250–400 raw words explaining the distinction between (1) tight conservative leaf caps, (2) a good global sampling distribution, and (3) an advantageous shared representation. State why S1 passes while S3 fails, why the leaf-CDF repair improves variance but removes the shared-storage claim, and why the project stops before product sampling rather than selecting a new application after seeing the result.

### R0 hybrid writing update — 2026-08-25

Do not present a geometry-aware hybrid as future performance evidence. The zero-cost upper bound already fails: bootstrap median `0.9182x`, median saving `4.738 ns/ray`, and certified DDA selected only for front rays. Safe wording is that the front-facing regime admits a strong specialized fast path, but its balanced-workload contribution is too small to justify dispatch and scheduling machinery.

Author task A11: write 150–250 raw words explaining why a `0.383x` front-ray result does not imply a strong hybrid. Include the balanced family weights, the `0.9182x` zero-cost oracle, the missing dispatch/compaction/divergence costs, and the decision to stop before a new GPU implementation.

## 6. What the author should write

### Author task A1 — write now

Write **500–800 raw words**, without polishing, answering:

1. What concrete failure or inefficiency of min/max bounds motivated subtracting a local plane?
2. Why is supporting both ray visibility and surface-area sampling valuable in the workflows you care about?
3. Why did you freeze the analytic shell image of texture microtriangles as the common surface?
4. Which three content regimes do you expect to help most, and which three should fail or regress?
5. What result would convince you to abandon the ray-performance claim?
6. If GPU speed is mixed, what memory/build/update result would still make the shared framework worthwhile?

Write this in your own voice. It will supply the motivation, design-rationale, and limitations paragraphs of the eventual draft.

### Author task A2 — after the Step 1 report

For every major plot, write one paragraph with four sentences:

1. what you predicted;
2. what the experiment actually showed;
3. what mechanism explains it; and
4. what claim is now permitted or forbidden.

### Author task A3 — during theory consolidation

Write short intuition paragraphs for T2, T7, T8, and T10 before formalizing the algebra. These should explain why the theorem is true geometrically and where it can fail.

### Author task A4 — negative-result log

For every correctness bug, adverse asset, or losing baseline, record:

- the complete case ID;
- the initial expectation;
- the observed failure/regression;
- the violated assumption or dominant cost; and
- whether the method, claim, or stated limitation changed.

These notes become the ablation discussion and limitations section; they should not be discarded.

**Write now:** add the first A4 entry for run `p3-f29f4158c87f`. Record the prediction that direct directional projection would materially reduce lazy knots, the observation that it changed only one of 883 active rays and saved four linearizations/two node tests despite 5,300 bound evaluations, and the current decision: keep fixed first-order slab rejection, but require a decisive frozen-family G4 result before retaining directional lazy segmentation as a contribution.

## 7. Writing schedule coupled to implementation

| Project gate | Draft material safe to write |
|---|---|
| Now | Introduction motivation, surface contract, hierarchy definition, ray derivation, expected limitations |
| Persistence P1–P4 | Reproducibility/evaluation protocol and dataset manifest |
| Step 1 G0–G4 | CPU correctness, tightness, segmentation, and ablation results |
| P-D2.0c negative ray gate | Negative ablation, limitations, and the decision not to GPU-port the present predicate |
| Area oracle/sampler gate | Metric propagation, PDF proof, and sampling results |
| Combined-system gate | Final abstract, contribution bullets, and headline Pareto claim |

Do not wait for every experiment before writing the surface, hierarchy, and proof sections. Do not finalize the abstract or contribution bullets before both applications have evidence.

---

Related: [[Project — Conservative first-order queries on displacement maps]] · [[Plan — Step 1 ray reference experiment]] · [[Plan — Step 1 persistence and instrumentation]]
