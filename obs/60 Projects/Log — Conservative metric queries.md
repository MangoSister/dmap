---
title: Log — Conservative metric queries
tags: [project, log]
created: 2026-08-25
---

# Log — Conservative metric queries

Append-only running log for [[Project — Conservative metric queries without tessellation]]. Newest entry first. Each entry records what was done, what was measured, and what was decided, with reasoning. Plan changes themselves are made in the master plan, in place.

---

## 2026-08-26 — Phase 2 order changed: sampling MVP first

- Decision: start Phase 2 with the A-MVP (product sampling) instead of D (diagnostics). D was first only because it is the cheapest; it stays a guaranteed section and does not gate anything, so its position carries no risk. A is a headline candidate and carries the project's cheapest kill-test: the Ling et al. baseline, which decides whether the sampling story needs the pyramid at all. Front-loading it gets that evidence to the gate earliest. Master plan §7 Phase 2 list and the Status block updated in place. The rest of the order (D, then B1-MVP, then the B2 spike) is unchanged.

## 2026-08-26 — Kill criterion revised after a per-application review; Phase 1 passes

- Question raised: does tightness at coarse cells (32–64 texels per side) matter for correctness anywhere, or only for efficiency? Reviewed each application:
  - **A, product sampling: variance only.** Every sample descends to a leaf, and the density is exact because √det G at the drawn sample is evaluated pointwise, never from a node. Coarse-level weights only split probability between subtrees; a loose weight misallocates samples, which is variance, never bias. The default weights use the node's midpoint model, not the certified interval, so interval width barely enters. The one correctness rule — positive weight wherever emission is positive — is the ε floor, independent of tightness.
  - **B1, texture-space operator: refinement depth.** A cell whose certificate does not close gets refined, so loose coarse bounds cost degrees of freedom, never wrong answers. Measured widths suggest certificates with ~10% tolerance close at 2–4-texel cells. The exposure is to the claim, not the correctness: if certificates only close near the leaf, "certified a-priori adaptivity" degenerates toward uniform fine resolution. Measure in Phase 2.
  - **B2, walk on spheres: efficiency, most sensitive.** Conservativeness keeps the estimator unbiased at any looseness, but the feature-size lower bound controls walk length (the PWoS paper's own figure shows 31 → 1819 steps as the bound shrinks). Our normal cones saturate at π above ~8–16-texel cells on rough content, so the cone localization only helps at fine cells. Worst case an order of magnitude in walk length, still never a wrong answer.
  - **C, LoD error: the one direct consumer of coarse cells.** The distortion certificate for mip level k is read per level-k cell. But LoD decisions usually compare levels 1–3 apart, and per coarse texel the relevant fine-side node covers 2–8 fine texels, where tightness is 1.5–2.2. Only certifying drops deeper than about 3 levels reads the loose 32–64-texel bounds.
  - **D, diagnostics: unaffected** (pointwise, no pyramid).
- Conclusion: correctness depends on conservativeness alone, which holds at every level; tightness at coarse cells buys only efficiency, and the applications read bounds for their *answers* at cells of roughly 1–8 texels. The old blanket criterion ("within 3× at the levels applications traverse") gated on cells no application depends on.
- **Criterion revised in the master plan §7, in place:** conservativeness is absolute (any violation kills); tightness must be within 3× of the true range at cells up to 8 texels per side on typical content; coarser cells are reported and priced per consumer in Phase 2. Flagged as sensitive there: the normal-cone localization of B2's feature-size bound, and LoD drops deeper than about 3 levels.
- **Under the revised criterion Phase 1 passes:** 0 violations, worst median ratio 2.77 (the cone on spot + disp_rock at 8-texel cells). Experiment 5's verdict logic updated to match and rerun; [[Result — Phase 1 Taylor pyramid]] updated. Phase 2 begins: the three MVPs, D first, and the C++ core.

## 2026-08-26 — Phase 1: Taylor pyramid built, kill-test run

- Implemented the pyramid in the numpy reference, per the session decision to keep Phase 1 in Python and start the C++ core with Phase 2 (master plan §4 updated in place). New modules: `affine.py`, `pyramid.py`, `node_bounds.py`, `dense_reference.py`; experiments 5–7; 24 tests pass. Full numbers in [[Result — Phase 1 Taylor pyramid]].
- **Kill-test outcome is mixed, left open for the gate.** On typical content (0.05×edge), median √det G tightness is 1.2× at texel cells, 1.6–1.9× at 4-texel, 2.7–4.1× at 32–64-texel cells. The criterion (more than 3× at traversed levels → rethink) is met at fine traversed levels and missed at the coarsest ones. Reasons recorded: the residual gap at coarse cells is inherent to the node — its six numbers bound h and the gradient by independent ranges, which also cover value combinations that never occur together on the surface — and not slack the propagation left (zeroing the base-form enclosure changes widths by under 5%; the ablations are never tighter). The Phase 2 consumers weigh in favor of proceeding: sampling weights (A) turn looseness into variance only; the certificate consumers (B1, C) refine until tight, so looseness costs refinement depth, not correctness.
- Three implementation findings changed the concept note, rewritten in place:
  - **Fold through interval hulls**: the parent gradient interval is the interval hull of the child intervals, with the slope at its midpoint; the parent offset and remainder are the midpoint and half-width of the child plane bounds evaluated at the quadrant corners. This replaced the child-mean plane and cut widths by about a third at mid levels.
  - **Propagation is intersected**: the affine route (squares anchored at the midpoint of the quadratic term's range) ∩ plain interval evaluation of the same node. Interval squares beat affine forms wherever a deviation straddles zero, which on rough content at coarse cells is everywhere; the affine route wins the correlated products. Eigenvalues need Weyl ∩ the affine discriminant ∩ the determinant division; the naive interval discriminant is useless.
  - **The §1 failure argument sharpened**: giving the same gradient two inconsistent worst-case values is a failure of interval *evaluation*, not channel *storage*. Min-max channels consumed as centred affine forms match the joint node's metric bounds (measured, within 1%); interval consumption costs 1.6–1.9× and twice the false alarms at fine levels. The joint node keeps the exact leaf and the O(s²) slab bound.
- Negative results, now in the notes: λmin has no meaningful per-node range certificate (rank-one structure pins it; width relative to value is order 1) — limits per-node anisotropy and conditioning certificates. Certified normal cones saturate at π above ~8–16-texel cells on textured content. Certificates of det G > 0 fail at coarse cells for every bound family tried. Min-max recovery is 5–29× looser than the exact channel near the root, so ray traversal keeps the height channel.
- Two implementation bugs caught by the conservativeness harness, worth remembering: sampling gradients exactly on cell edges attributes the neighbor cell's one-sided gradient to the wrong cell (truth grid now insets by 1e-9), and the cone cosine bound is invalid once the deviation radius reaches the centre length (must open to π).
- Cost as designed: 3.0× the two-channel min-max memory, one vectorized mipmap-style build pass (50 ms for a 512² tile in numpy), ~1.5 μs per node for a batched bound query.

## 2026-08-26 — Check-first literature sweep done; Phase 0 complete

- Ran the §3 check-first sweep as four parallel web searches (bounding structures; shells/CAD/normal fields; PDE operators and certified queries; sampling and LoD). Full findings in [[Result — Check-first literature sweep]]; master plan §3 rewritten in place; positioning sentences added to §5A and §5 B2.
- Verdict: no claim died; every claim narrowed. Five items found nowhere in the literature: the joint (h, ∇h) Taylor node with certified metric/area/anisotropy/normal-cone propagation; certified area/measure queries on displaced surfaces at all; the certified local-feature-size lower bound; emissive displaced surfaces as unmeshed light-tree clusters targeting E·dA; the integrability defect and three-determinant separation as displacement diagnostics.
- Sharpest collisions found: Hasselgren/Munkberg 2009–2010 (Taylor-arithmetic bounds of displaced patches, min-max mips + normal cones, patent); pbrt-v4's textured bilinear-patch emitter (exact area PDF by pointwise Jacobian — application A's central mechanism, in the textbook); Huang, SIGGRAPH Asia 2025 TC (certified branch-and-bound queries for walk on stars on implicits — application B2's elevator pitch); Spira–Kimmel 2004 / Weber 2008 (geodesics on parameter-lattice grids from the metric); Maggiordomo's visibility (existing pre-bake tilt diagnostic). Ling et al. 2025 confirmed uniform-only, no rendering use — weaker rival than assumed, still the mandatory baseline.
- The field is active (Huang 2025, Hui 2026 modified PWoS, Noma 2026 neural displacement fields, DJM 2026): keep the schedule tight.
- Six papers flagged read-in-full before submission (Pottmann 1997, Chen 2014, Bán & Valasek 2025, Dodziuk 1982, Hoetzlein 2025, Moule & McCool 2002).
- Phase 0 is complete. Next: Phase 1, the Taylor pyramid and its tightness kill-test.

## 2026-08-26 — Phase 0 numpy reference built and run

- Created the conda env `dmap` and the reference implementation in `C:\dmap\code` (`dmapref` package, 16 unit tests, four experiments). Coding-style rule added to `CLAUDE.md`: clarity over optimization, functions map to note formulas.
- Experiment 1: the metric formula matches finite differences of S at order 2.00, floor 1.2e-10. The formula is verified.
- Experiment 2: tessellated (rendered) surface converges monotonically to the formula. Leaf-scale area discrepancy (limitation 2): ~0.3–1.3% mean at 0.05×edge amplitude, 2–7% at 0.2×edge. Priced, as the plan required.
- Experiment 3: **interpolant decided — bilinear.** B-spline's C¹ smoothness bought no measurable operator accuracy on the toy heat-method solve (errors within 5% of each other everywhere, zero invalid cells for both). Master plan §4 updated in place. Also observed: per-face constant metric sampling cannot violate the triangle inequality (lengths come from an actual embedding); the hazard belongs to per-edge sampling. Worth remembering for Phase 1+.
- Experiment 4: obliquity diagnostics show the predicted coarseness dependence in real data: cc_torus max sin²θ = 2.3e-4 (~1°), spot max 0.32 (~35°) with a heavy tail. Application D has signal.
- Full numbers in [[Result — Phase 0 numpy reference]].

## 2026-08-25 — Style pass on the master plan

- Rewrote the master plan's prose to follow the new writing-style rule: plain vocabulary, short sentences, standard terms, fewer em-dashes. All formulas, numbers, tables, code blocks, and links are unchanged.
- Fixed two stale cross-references while at it: the contribution statement pointed to §3.2 for the Taylor pyramid (now §2.2) and to a nonexistent §8 for the classical-citations discussion (now §3).
- Retitled §3 from "the paper's honesty ledger" to "Believed new vs classical". Content unchanged.

## 2026-08-25 — Plan cleanup and document structure

- Removed all phase durations and calendar dates from the master plan. They were mock-ups, not estimates anyone had committed to. The phase order, the gate, and the kill criteria stay. The only external anchor left is the SIGGRAPH 2027 deadline (late January 2027).
- Adopted the two-document structure: the master plan stays stateless and always current, with a short Status block at the top; this log is append-only and holds history and reasoning. Experiment results will go into their own notes.
- Added `CLAUDE.md` at the repository root with the document-organization rules and a writing-style rule (plain vocabulary, standard technical terms, short sentences, no invented shorthand).

## 2026-08-11 — Outside review and note hygiene (retroactive entry)

- Outside review of the metric thread. Derivations verified independently. Two corrections applied to the concept notes: micro-mesh metrics come from vertex positions, not the formula; the metric and DJM's shell Jacobian are complementary, not one subsuming the other.
- Projected walk on spheres promoted to a first-class application (B2) after reading the PWoS paper. Its heuristic local-feature-size pipeline is the opening for certified bounds. The B2 feasibility spike in Phase 2 is unconditional.
- Decisions settled: joint Taylor-model bound node (not independent min-max gradient channels); per-face Ptex-style parameterization; CPU-first prototype.
- Master plan written (`Project — Conservative metric queries without tessellation`).
