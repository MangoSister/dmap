---
title: Hybrid displacement and opacity micromaps
tags: [open-question, research-direction, micromap, lod, priority]
priority: 4
status: partially-occupied
rests-on: "The citation-graph hole; revised 2026-08-10 after reading van Antwerpen et al."
---

# Hybrid displacement and opacity micromaps

> [!warning] Substantially revised after reading [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage]]
> The core conceptual claim of the original version of this note — *geometry becoming coverage at coarse LOD* — **is taken.** It is that paper's headline contribution. What survives is narrower and more specific, and is set out below. The original framing is preserved at the bottom so the revision is auditable.

## What is now taken

van Antwerpen et al. replace chunks of irreducible fine geometry — twigs, needles, leaf clusters — with **three mutually perpendicular proxy triangles carrying opacity micromaps**. Geometry literally becomes coverage. Two things I had assumed were open are not:

- **The substitution itself**, done automatically with no semantic labels or artist authoring.
- **A derived crossover criterion.** Not distance, not screen size, but a *memory cost model*: `C_OMM < C_tri`, with coefficients "fitted empirically to measured OMM and acceleration structure sizes". Their contrast with Nanite is the sharpest statement of the design space — Nanite is **error-driven** and switches to voxels when geometric error would exceed voxel error; this is **memory-driven** and switches when OMMs are cheaper.

So "is the crossover derived rather than authored" is answered. Twice, differently.

## What survives, and is now better posed

**1. The crossover is one-shot, not continuous.** The representation has exactly three regimes: LOD 0 (pure geometry), LOD 1+ (one hybrid triangles+OMM mix, **reused verbatim by every coarser LOD**), and a final hard jump to three bounding-box-aligned OMMs for the whole plant. Coarser LODs differ only in *instance granularity*, never in the geometry-to-opacity ratio. There is no progressive shift with distance.

**2. The reason it is one-shot is stated, and it is a real tension.** From their limitations:

> Per-LOD OMM bakes could relax this, but at the cost of **forfeiting CLAS reuse**.

Sharing one cluster pool across all LODs is what buys the 46× acceleration-structure memory reduction that makes the whole system work. Per-LOD opacity re-baking would break it. **That is the actual open problem** — not "can geometry become coverage" but *can the crossover be made continuous without giving up cross-LOD geometry reuse?* A representation where coarser levels are derived from finer ones by filtering rather than re-baking would dissolve the tension, and that is exactly what a shared hierarchy over both payloads would give.

**3. The consequence they accept is a real artefact.** Because LOD 1+ geometry is reused over a shrinking screen footprint:

> screen-space geometric complexity can **increase** at coarser, more distant LODs… We rely on a denoiser to suppress the resulting high-frequency noise.

A system whose geometric complexity *rises* with distance has not solved LOD; it has bounded memory and delegated the rest to DLSS. That is an honest engineering trade, and it is also an opening.

**4. No unified structure exists.** There are three parallel structures — CLAS geometry, a separate OMM array, and a sparse shading buffer — tied by references, partly because the API keeps micromaps in a separate array. Nothing like a single node interpretable *either* as triangles *or* as a coverage value. Whether one is possible, and whether traversal would benefit, is untouched. Note [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] identify **dependent memory reads** as their binding constraint, which is exactly what two parallel hierarchies double.

**5. Displacement is still absent on both sides.** van Antwerpen et al. never mention micro-meshes or displacement at all. The substitution is *explicit triangles* → opacity, never *displacement* → opacity. Given displacement micromaps are withdrawn ([[Graphics API and hardware support timeline]]), the live version of this question is cluster geometry ↔ opacity, and the [[Maggiordomo et al. 2023 — Micro-Mesh Construction|µ-mesh]] hierarchy is only relevant as a design precedent.

## Also newly open, from their future work

They propose extending OMM approximation to **translucent** foliage building on dithering and stochastic opacity, **aggregate BSDFs** for distant layers, and **OMM impostors beyond foliage** — "fences, webs, and other cellular/lattice-like geometry where micromap opacity can stand in for dense geometric detail at distance". That last one is close to the application list I originally wrote (fabric, chain mail, perforated façades) and is now explicitly flagged by NVIDIA as unexplored.

## Honest assessment

This direction dropped from "structural hole nobody noticed" to "a well-defined tension inside a system that already works". That is a **less exciting but more tractable** research position: there is a strong baseline to beat, a named mechanism blocking the improvement (CLAS reuse), and an admitted artefact to fix (complexity rising with distance). It is no longer a blue-sky direction, and should not be pitched as one.

---

## Original framing, superseded

The two micromap kinds are the same structure — a hierarchical, uniformly subdivided barycentric grid over a triangle with values in space-filling-curve order — differing only in payload (11-bit heights vs 1–2-bit opacity). See [[Micromap]] and [[Micro-triangle and subdivision level]]. The vault's displacement and opacity literatures have **one citation edge between them**, which exists only so [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] can scope displacement out — yet foliage, the canonical asset for both, is displaced *and* alpha-masked.

Questions raised: could one traversal serve both payloads; does opacity prune displacement (a fully transparent sub-triangle's height is irrelevant, and [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]] proposed shrinking BVH leaf bounds with transparency bits in 2020 with no follow-up); does displacement change opacity under LoD; and the claim that **as micro-geometry shrinks below a pixel it stops being geometry and becomes coverage** — which is the part now occupied.

On encoding, [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] still list extending their succinct tree to displacement micromaps as unexplored future work.

Related: [[MOC — Opacity Micromaps]] · [[MOC — Micro-Meshes and Geometry Compression]] · [[Prefiltered coverage from opacity hierarchies]] · [[Tessellation-free vs pre-tessellation]]
