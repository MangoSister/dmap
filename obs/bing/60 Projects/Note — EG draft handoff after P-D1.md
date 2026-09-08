---
title: Note — EG draft handoff after P-D1
tags: [paper, eurographics, handoff, ray-tracing, p-d1]
status: active
created: 2026-08-24
updated: 2026-08-24
evidence-cutoff: p-d2-r0-representation-oracle-complete
---

# EG draft handoff after P-D1

> [!abstract] How to use this note
> This is the safe handoff to a separate drafting session. It distinguishes material that can be written now from hypotheses that remain unsupported after the completed P-D2 representation oracle. Numerical values below come from the corrected authoritative P-D1 chain and the frozen P-D2/P-D2-R0 runs, not superseded exploratory runs.

## 1. Current paper frame

Working title:

> **A Conservative First-Order Displacement Hierarchy for Visibility and Surface Measure**

The intended umbrella is **conservative geometric queries over a triangle-proxy displacement map**. Ray intersection is the extrinsic visibility application; surface area/area sampling is the intrinsic metric application. Catmull–Clark surfaces and arbitrary-geometry-to-displacement conversion are outside the active scope.

The common representation is a hierarchy node with a local height plane and certified uncertainty,

$$
|d(u,v)-p_A(u,v)|\le r_A,
\qquad
p_A(u,v)=c_A+g_{u,A}(u-u_A)+g_{v,A}(v-v_A),
$$

plus conservative gradient uncertainty and folded scalar height bounds. The paper's intended novelty is the reuse of this retained first-order correlation by different conservative queries. It is not DDA, min/max boxes, or piecewise ray flattening by itself.

## 2. Sections safe to draft now

### 2.1 Motivation and problem statement

It is safe to explain that a zero-order min/max node mixes predictable variation from local slope with the remaining nonlinear variation. On a smooth tilted region, the height range can therefore remain large even when the residual around a local plane is small. A first-order node separates those terms and also retains derivative information needed by metric queries.

Use conditional wording for the practical consequence:

> We investigate whether retaining this correlation reduces conservative query work enough to offset its larger payload and arithmetic cost.

Do not yet say that it does.

### 2.2 Represented surface contract

The active ray surface is the triangle-proxy shell

$$
F(a,b,h)=P(a,b)+hN(a,b),
$$

where both $P$ and $N$ are affine over a proxy triangle. A fixed displacement texture is piecewise affine over its two microtriangles per texel cell. Ray hits, leaf equations, normals, and future area queries must all refer to this same represented surface. This avoids silently validating traversal against a different microtessellated surface.

State the regularity domain: the shell map must remain invertible over the admitted height interval; singular or numerically unresolved intervals require an explicit conservative fallback. State closed shared-boundary ownership because grid-edge and grid-corner hits can have multiple valid leaf owners.

### 2.3 Shell-space ray derivation

For a world ray $R(t)=O+tV$, inversion through the affine-normal triangle shell gives a rational curve parameterized by shell height:

$$
q(h)=\left(u(h),v(h),h\right)
=\left(\frac{U(h)}{D(h)},\frac{V(h)}{D(h)},h\right),
$$

where $U$, $V$, and $D$ are degree-at-most-two polynomials for this shell. In world space the ray is straight; in shell/texture space its $(u,v)$ trace is generally curved. The method partitions the valid height domain at denominator, proxy-boundary, and turning events.

### 2.4 Certified chords and traversal ownership

It is safe to describe the P-D1 construction:

1. partition each valid rational shell-ray interval;
2. approximate each subarc with a UV chord;
3. certify componentwise chord-error radii using polynomial/Bernstein bounds and a denominator lower bound;
4. traverse the resulting closed UV tube rather than the zero-width chord; and
5. retain all cells touched at grid edges/corners through closed event ownership.

The chord is a traversal aid, not the final geometric approximation. The tube is the certificate that makes the piecewise linear traversal conservative.

Avoid saying that first-order information currently creates fewer chords. The frozen P-D1.2 ablation found no material segmentation benefit: pooled split counts were 5 for the componentwise rule and 3 for the direct directional rule, while their medians were equal. The first-order performance hypothesis has therefore moved to **residual interval clipping with segmentation held fixed**.

### 2.5 Exact represented leaf equation

For one affine displacement microtriangle,

$$
d(u,v)=\alpha u+\beta v+\gamma,
$$

substitution of the rational shell ray yields

$$
G(h)=\alpha U(h)+\beta V(h)+(\gamma-h)D(h)=0,
$$

up to the equivalent sign/constant convention used by the implementation. Since $U$, $V$, and $D$ are quadratic at most, $G$ is cubic at most. The reference isolates all real roots in the admitted closed height interval, validates texture-microtriangle ownership, groups coincident roots, and applies deterministic closest-hit ownership.

The important contrast is that the final test solves the represented displaced surface. It does not intersect planar world-space microtriangles merely selected by an approximate chord.

### 2.6 Correctness argument structure

The visibility proof can already be organized as:

1. valid shell intervals cover every regular ray/shell solution;
2. every true shell arc lies inside its certified UV tube;
3. the closed event supercover admits every leaf containing a point of that tube;
4. every represented leaf intersection is a root of the cubic equation above; and
5. canonical root grouping and closed ownership return the exhaustive oracle's closest root and owner set.

Floating-point qualification matters: the corrected reference uses position-terminated root isolation, Decimal-audited conservative root brackets, outward widening, and explicit precision fallbacks. Earlier exploratory artifact IDs are not evidence.

### 2.7 Limitations and expected regimes

Safe limitations to write now include shell singularity/poor conditioning, grazing rays, many tube events at high texture resolution, high-frequency displacement with little empty space, increased first-order payload, GPU divergence, and static workloads favorable to dense triangles or hardware micromeshes. Universal fastest tracing is neither expected nor claimed.

## 3. Quantitative evidence safe to cite

Authoritative full-reference artifact:

`experiments/p_d1_full_reference/p-d1-6-e254f252e010`

Across 864 frozen practical triangle-proxy shell-ray cases:

- event, recursive, exhaustive, and Decimal-audited root groups agree;
- every constructed target is retained, including 144/144 proxy-edge cases;
- no practical fallback was required;
- 6,431 event-supercover candidates were tested versus 1,851,552 leaf candidates in the exhaustive oracle;
- 936 canonical root groups were recovered;
- maximum event/exhaustive root-coordinate difference was $2.731\times10^{-14}$;
- maximum floating/Decimal root difference was $1.643\times10^{-14}$;
- maximum height-polynomial residual was $1.148\times10^{-13}$; and
- maximum world-space ray/surface residual was $9.080\times10^{-16}$.

Safe result sentence:

> Across 864 practical triangle-proxy shell rays, our closed event supercover and represented cubic leaf solver reproduce the exhaustive oracle's canonical root and owner sets, including all constructed edge and corner hits.

The 6,431-versus-1,851,552 count is only an **oracle-work diagnostic** (about 288× fewer enumerated leaf candidates). It is not a speedup over TFDM, Ogaki, RMIP, PDM, DMM, or a GPU baseline.

The full corrected artifact chain is:

- P-D0 dataset: `p-d0-71627ba7b07f`;
- P-D1.2 shell intervals/tubes and corrected hierarchy records: `p-d1-2-ec0dd6a036f5`;
- P-D1.3 recursive coverage: `p-d1-3-62f0f7416547`;
- P-D1.4 closed event coverage: `p-d1-4-dd1251769c51`;
- P-D1.5 cubic leaves: `p-d1-5-b7bcc9a005a6`; and
- P-D1.6 full reference: `p-d1-6-e254f252e010`.

### P-D2.0b drafting update

The 18-case integration anchor now passes all 108 `(case,variant)` traversals with identical grouped roots, closest roots, and owner sets. It is safe to write the **experimental design**: all variants replay the same certified UV events, propagate one-dimensional clipped intervals, and call the same cubic leaf solver.

Do not promote the anchor counts to the abstract or contribution bullets. As an internal diagnostic, `FO-CW-clip` used `0.907×` the node predicates and `0.787×` the leaf cubics of `MM-clip`; the hybrid used `0.884×` nodes and `0.745×` leaves. These numbers justify the frozen 864-case P-D2.0c run, but one window is not enough for a paper performance result.

P-D2.0b also produced a useful implementation lesson for the numerical section: a single-ULP widening after a rounded bottom-up fold was insufficient. The corrected reference constructs centered node envelopes in exact binary64-rational arithmetic and rounds outward only at storage. Phrase this as a numerical design requirement, not as a claimed production preprocessing algorithm; the GPU builder still needs a validated directed-rounding implementation.

### P-D2.0c drafting update — final ray-side usefulness decision

Use authoritative run `p-d2-0c-47849fef3202`. It contains 48 exact-audited practical hierarchies, 864 ray cases, and 5,184 variant traversals. Every variant matches the exhaustive grouped roots, closest roots, closed owner sets, and constructed targets; all interval, hierarchy, fallback, and deterministic-replay gates pass.

The predeclared first-order acceleration hypothesis is negative on the primary coherent cohort. Relative to scalar `MM-clip`, `FO-CW-clip` uses `0.9966×` node predicates and `0.9674×` leaf cubics, while `HYB-clip` uses `0.9906×` nodes and `0.9622×` leaves. Both are far above the required `0.85` leaf ratio, and both have negative projected overhead budgets at leaf/node cost ratios 4 and 8. This means the predicted `10%` internal win is unavailable even before charging first-order payload and arithmetic cost.

It is safe to report the regime observation as a secondary diagnostic: over the boundary-stress cohort, the hybrid uses `0.8813×` nodes and `0.7460×` leaves; over near misses, `0.9049×` and `0.8091×`. Do not use these selected cohorts as a headline or imply they reverse the frozen decision. Front-hit work is exactly unchanged, and the coherent practical corpus is the primary gate.

The direct directional predicate should not appear as a retained contribution. After adding the correctness-required numerical owner guard, it performs identically to componentwise clipping in pooled work and triggers the guard 5,851 times. It is useful only as a negative ablation about the interaction between a tight real-curve certificate and floating-point closed-boundary ownership.

The paper story must now separate two ray results:

1. **Positive method/correctness result:** certified triangle-proxy shell inversion, piecewise tube coverage, closed hierarchy ownership, and exact represented cubic leaves recover the exhaustive practical oracle without displacement-resolution tessellation.
2. **Negative performance result:** adding the present first-order residual clip does not materially reduce ordinary coherent traversal work, so no CUDA/OptiX port or TFDM/Ogaki/RMIP speed comparison is authorized for this formulation.

Consequently, the first-order hierarchy can remain the paper's unifying representation only if the area/metric application shows a clear and application-relevant advantage. The ray application may remain a technically substantial consumer, but its novelty should be the certified nonlinear shell-ray/query construction rather than a claim that first-order nodes accelerate it.

Planning update: [[Plan — First-order ray representation oracle]] is complete and negative. The weak P-D2 result is not specific to its arithmetic-average single plane. The prohibition on a first-order ray-speed claim, GPU port, and external speed comparisons for this representation is now final on the fixed hierarchy topology.

### P-D2-R0 drafting update — optimized-plane and support-hull upper bound

Use full run `p-d2-r0-05a8b7b3f3c4` and report `p-d2-r0-report-af2d251f38cb`. The run replays 864 cases through eight variants and passes grouped-root, closest-root, owner-set, target-retention, interval-containment, exact-model-enclosure, no-fallback, determinism, and previous-count-reproduction checks. The 30-test P-D2 regression suite passes.

The optimizer reveals a real static-bound improvement: the ray-independent optimal strip has median residual-width ratio `0.6360` relative to the current plane, and the tube-aware objective has median effective-width ratio `0.5783`. Do not convert these width reductions into a traversal or speed claim. In the coherent cohort, the realizable optimized hybrid still uses `0.9841×` scalar nodes and `0.9611×` scalar leaf cubics. It fails the frozen `0.85` leaf gate and has negative `delta_max` at leaf/node costs 4 and 8.

The strongest evidence is the recertified convex support-hull oracle. It encloses every node microtriangle, applies all convex affine support directions simultaneously, and is counted unrealistically as one node predicate. Even this upper bound uses `0.9818×` coherent nodes and `0.9548×` coherent leaves, wins only `7/12` assets, and has negative break-even margin. Therefore a better single slope or a finite `K=2/4` slope dictionary cannot plausibly supply the missing broad coherent acceleration on the same topology.

Safe result sentence:

> Although optimal plane fitting substantially tightens median static residual bounds, neither the optimized hybrid nor an idealized convex support hull materially reduces coherent hierarchy work; we therefore rule out affine first-order support clipping as the ray-acceleration mechanism on this fixed topology.

This is a scoped upper-bound no-go, not a claim about every possible displaced-surface accelerator. It does not test the certified piecewise shell-ray plus scalar min/max-DDA architecture against Ogaki or TFDM, nor adaptive/anisotropic topology, nonconvex mixtures, or a grazing-specialized accelerator.

### Active planning update — scalar ray architecture

[[Plan — Certified ray architecture comparison]] now freezes the next ray experiment. Its internal causal comparison holds the shell surface, scalar min/max hierarchy, exact cubic leaf, rays, and OptiX semantics fixed while replacing nonlinear per-node ray–box tests with certified chord-tube DDA. Its external TFDM comparison uses both equal-input throughput and a common-oracle quality–time–memory Pareto curve, because TFDM's native local surface is not assumed identical.

The plan itself is not performance evidence, and A0 has now passed only its infrastructure gate. Run `ray-a0-smoke-d9bbfd971b2f` verifies the shared timing schema, discarded warmups, five randomized trials, explicit primary-ray counts, TFDM `TwoTriangle`/`Bilinear` GPU initialization, and stable trial medians. Do not write that the certified DDA is faster: it has not been implemented. The existing Mode 1 path still uses heuristic zero-width segments and planar world-microtriangle leaves, so even its stable A0 smoke ratio cannot appear as a method result. The next action is the detailed A1 differential-port plan, followed by device-primitive correctness testing—not performance optimization.

## 4. Exact claims that remain prohibited

Do not write any of the following as results:

- the first-order hierarchy materially accelerates ordinary coherent ray traversal;
- the first-order ray method is faster than scalar min/max;
- the method is faster than TFDM, Ogaki, RMIP, PDM, DMM, or dense triangles;
- direct directional certification materially improves segmentation;
- the method is GPU efficient;
- one shared hierarchy is better than two specialized structures; or
- the area sampler is correct, unbiased, or competitive.

P-D2 has decided not to advance the first-order ray predicate to P-D3/P-D4. The separately frozen scalar architecture hypothesis may advance only through A0–A4 of [[Plan — Certified ray architecture comparison]]. RMIP/PDM/DMM work remains deferred until the Ogaki-style and TFDM first-tier gates pass. Area sampling is temporarily deferred while the user finishes this ray gate.

## 5. Draft placeholders to leave explicit

Use placeholders rather than guessed values for:

- `[P-D2 coherent result: FO-CW 0.9966× nodes / 0.9674× leaves; HYB 0.9906× / 0.9622×; both fail]`;
- `[GPU ns/ray and memory]`;
- `[TFDM/Ogaki/RMIP/PDM/DMM equal-quality comparisons]`;
- `[area-bound tightness and sampling efficiency]`;
- `[combined hierarchy memory/build/update Pareto]`; and
- `[focused novelty audit of centered-form/gradient hierarchies]`.

The abstract and final contribution bullets should not be frozen until the ray-architecture and area-query gates are known. A scalar certified ray GPU gate is active; the rejected first-order payload is not.

## 6. Immediate writing tasks for the author

1. Write the surface contract and assumptions in precise notation.
2. Write the rational shell-ray derivation, including polynomial degrees and singularity conditions.
3. Turn the five-step correctness argument above into lemma statements, keeping real-arithmetic and floating-point obligations separate.
4. Write the exact cubic leaf derivation and explain why it targets the represented surface.
5. Draft the limitations paragraph now; it will help prevent the introduction from overclaiming.
6. Fill the P-D2 row with the authoritative negative result; leave GPU baselines, area sampling, and shared-structure rows empty rather than inserting predicted numbers.
7. Add a negative-result paragraph for P-D2.0c: state the frozen hypothesis, the coherent ratios, the negative break-even margins, and the decision not to GPU-port this predicate.

Related: [[Guide — Eurographics paper draft]] · [[Plan — P-D1 certified tube-supercover DDA reference]] · [[Plan — P-D2 residual clipping ablation]]
