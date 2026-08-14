---
title: MOC — Open Questions
tags: [moc, open-question, research-direction]
created: 2026-08-10
---

# MOC — Open Questions

Underexplored directions in ray-traced displacement and opacity micromaps, deliberately **excluding rendering performance** — which is where essentially all published effort goes.

> [!warning] Provenance — read this first
> These notes are **analysis derived from this vault**, not claims made by any paper. Each cites the specific stated limitation or structural absence it rests on, and separates *what a paper says* from *what I am inferring*.
>
> ~~They have **not** been checked against the wider literature.~~ **Directions 1, 2 and 5 now have been** (2026-08-10, geometry processing and simulation — see [[Prior art index#The outside check — geometry processing and simulation (2026-08-10)|the outside check]]). Directions 3, 4, 6, 7, 8, 9 and 10 have not: for those I can still only say a direction is absent from these fifteen papers and their reference lists, not that nobody has published it. Concurrent-work risk is highest for anything touching foliage, and for anything the vision community would call reconstruction.

## The two observations behind all of it

**The field has optimised exactly one query.** Every structure here — min-max mipmaps, the RMIP, prismoids, micromaps, subtree DAGs — is a spatial index over sub-triangle detail. All twelve papers use it to answer *what does this ray hit*. These structures answer many other questions, and almost nobody asks them.

> [!danger] Necessary qualification, added 2026-08-10
> "The field" here means the **rendering displacement literature**. The observation is false of **geometry processing**, which has been asking these questions of other representations for years: Sharp & Jacobson's *Spelunking the Deep* (2022) turns conservative range analysis into closest-point and bulk-property queries on implicits, and Ling et al.'s *Uniform Sampling of Surfaces by Casting Rays* (2025) gets area and uniform samples from a ray subroutine alone. Directions 1 and 2 already exist in representation-agnostic form. The surviving thesis is narrower and must be stated as such: **nobody exploits *this* structure — min-max mipmap, prism, inversion, micromap compression — for any non-ray query.** Written without that qualification, this folder reads as unaware of SGP.

**The two clusters barely touch.** There is **one** citation edge between the displacement and opacity literatures in this vault, and it exists only so [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] can scope displacement out. Yet the canonical asset for both is foliage, which is displaced *and* alpha-masked.

## Ranked

| # | Direction | Rests on | 2026 paper check | Outside check (2026-08-10) | Reaches beyond rendering |
|---|---|---|---|---|---|
| 1 | [[Proximity and contact queries against micro-geometry]] | [[Thonat et al. 2023 — RMIP\|RMIP]] future work, one sentence | **open** | **narrowed** — query taken, representation free | physics, robotics, CAD (*not* haptics) |
| 2 | [[Surface measure and sampling on implicit displaced surfaces]] | [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping\|Ogaki]], stated limitation | **open** (cite DJM's det J) | **substantially occupied** — demote | manufacturing, physical estimation |
| 3 | [[Prefiltered coverage from opacity hierarchies]] | [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps\|Chernaik et al.]], stated open challenge | **open, sharpened** | not checked — but Wu et al. 2019 surfaced and is relevant | agronomy, remote sensing, solar |
| 4 | [[Hybrid displacement and opacity micromaps]] | the citation-graph hole | **partly occupied** | not checked | fabric, medical mesh, façades |
| 5 | [[Non-ray queries — slicing, volume and mass properties]] | ~~absence~~ → Micromachines 2024, a measured tessellation cost | **open** | **open, strengthened** — promote | 3D printing, CAD/CAM, CFD meshing |
| 6 | [[Deformation-aware displacement fields]] | now *four* papers, same wall | **open** | not checked | surgical sim, garment sim |
| 7 | [[Dynamic opacity micromaps]] | absence | **open** | not checked | — |
| 8 | [[Inverse and differentiable micro-geometry]] | [[Dou et al. 2024 — Differentiable Micro-Mesh Construction\|Dou et al.]] extended | **open** | not checked — **highest remaining priority** | 3D capture, photogrammetry |
| 9 | [[Authoring, UV layout and micromap efficiency]] | [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps\|Waldemarson & Doggett]] §6.3 | unchecked | not checked | adoption, tooling |
| 10 | [[Smaller open threads]] | assorted future-work items | unchecked | not checked | geospatial, verification |

Ranking is by **impact per unit of risk**, not by ambition. The top three each have a named victim in the published literature, existing machinery pointed the wrong way, and cheap ground truth to validate against.

> [!warning] The ranking is stale as of 2026-08-10 and deliberately not renumbered
> The outside check inverts the top of this table. **Direction 2 should drop below 5** — its two headline deliverables (total area, uniform sampling) were published in 2025 by Ling et al., and its proposed mechanism (a displacement-gradient mipmap) has existed since LEADR in 2013; only *conservativeness* and *localisation* survive. **Direction 5 should rise** — it was ranked last of the three purely because it rested on absence, and it now has a measured cost from the manufacturing literature. **Direction 1 stays top but on a narrower claim**: the bounding argument is conceded to Spelunking, the query to Sum-of-Squares, and the haptics application to Otaduy & Lin 2003 — what remains is the representation, and warm-started temporal coherence.
> Numbering is left alone because the whole vault cross-references these directions by number. Re-rank deliberately, in one pass, or not at all.

## The 2026 check (2026-08-10)

Three papers arrived after these notes were written and were read specifically against them: [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage]], [[Zhang et al. 2026 — DJM]], and [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry]]. Outcome:

- **Direction 4 lost ground.** Geometry-becoming-coverage is the foliage paper's headline contribution, with an automatic memory-driven crossover. What survives is the narrower question of making that crossover *continuous* without forfeiting cross-LOD geometry reuse — a tension the paper names in its own limitations. Less exciting, more tractable, and now with a strong baseline.
- **Direction 3 gained ground.** I had ranked it highest-risk because the foliage paper sat on top of it. It does not do this: it explicitly refuses partial opacity, dithers instead, rejects prefiltering distributions, and delegates minification aliasing to a denoiser. The question is now a concrete comparison — *prefiltering versus dithering-plus-denoiser* — against a published production system.
- **Directions 1, 2, 5, 6, 7, 8 confirmed untouched** by all three. Direction 6 is now four papers deep in the same evasion. Direction 2 must cite DJM's closed-form Jacobian as prior art on *local* area distortion. — *Note added 2026-08-10: "untouched" here means untouched **by these papers**. For 1, 2 and 5 that is no longer the same as untouched; see below.*
- **[[Base mesh quality objectives]] is now substantially settled** by DJM, which replaces two of the four objectives with an exact computable quantity — though it sidesteps shell volume, and no head-to-head against Dou et al. exists. — *Qualified 2026-08-10: DJM's `Det(J)` is a scalar, and the metric it is the determinant of carries an eigenstructure plus two normal-field defects nobody measures. See [[The induced metric of a displaced surface]]. "Settled" is too strong.*

The general lesson: the risk was real but not where I guessed. Do the literature check per direction rather than trusting a global sense of crowdedness.

## Why performance is the wrong emphasis here

The vault's own history argues it. The most heavily optimised object in it — the displaced micro-mesh — **was withdrawn** ([[Graphics API and hardware support timeline]]), while the software methods that lost to it on every benchmark are still usable. [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Succinct Opacity Micromaps]] achieved 110× compression and was superseded within two years by a method with *worse* compression and better access time. And [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] showed that a shading-normal artefact had been sitting unremarked in Micro-Mesh and RMIP output all along, because nobody was measuring for it.

Speed rankings here have proven fragile. Representational and query-capability questions have not been asked at all.

## Before you dig: the prior art index

[[Prior art index]] maps what these twelve papers **collectively already survey**, by territory rather than by paper, with a per-direction verdict and a list of fields none of them cite at all. Read it first — it is the cheapest way to tell a real gap from an unread one. It flags that direction 3 has the densest surrounding literature *within graphics rendering*; its "literally none" verdicts for directions 1 and 5 describe **these bibliographies**, and both are now known to be wrong about the world. The index's new **outside check** section carries the corrections.

## The outside check (2026-08-10)

Directions 1, 2 and 5 searched against geometry processing and simulation — SIGGRAPH/Asia, ToG, EGSR, Eurographics, SGP, SCA, HPG. Full detail in [[Prior art index]]; per-direction consequences in each note.

- **Direction 1 — narrowed, not closed.** Nießner et al. 2013 already do collision against displacement-mapped patches (by tessellating and voxelising). Sharp & Jacobson 2022 already turn conservative bounds into closest-point queries (on neural implicits). Zhang et al. 2023 already do tessellation-free collision and CCD on curved primitives (by convex relaxation). Otaduy & Lin have served haptics from height-displacement profiles since 2003. The representation-specific claim survives; three of the four supporting arguments do not.
- **Direction 2 — substantially occupied.** Ling et al. 2025 compute area *and* uniform samples from a ray subroutine alone, which every displacement traverser already has. LEADR has mipmapped displacement gradients since 2013. What survives is conservative, localised, hierarchical bounds — and the concurrent-work risk sits with Boubekeur's own group, which owns both TFDM/RMIP and the prefiltering line.
- **Direction 5 — open and strengthened.** Direct slicing without tessellation is mature for CSG and implicits (IceSL), but not for tangent-space displacement over a base mesh; and the manufacturing literature has now published the tessellation blow-up this direction predicted, refining meshes to 10% of the smallest feature size to print displacement-mapped texture. Absence became evidence.

**The transferable lesson**, which is not the one from the 2026 paper check: the risk was not concurrent work in *this* field, it was **mature work in a neighbouring one**, arrived at from the other side and phrased in different vocabulary. Searching for "displacement" finds nothing; searching for the *query* finds everything.

## The remaining sanity check

Foliage: **done**. Outside graphics for directions 1, 2 and 5: **done**. What stands:

**Direction 8 is now the highest-priority unchecked lane.** It lives next to computer vision and differentiable rendering, the field least likely to cite this literature and most likely to have solved it — and the one where "fitting micro-geometry to images" is a well-populated phrase. Directions 3, 4, 6, 7, 9 and 10 remain unchecked outside these bibliographies too.

The failure mode this guards against is unchanged and now doubly evidenced: [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] went uncited by [[Barczak et al. 2024 — DGF|DGF]] three months later; [[Zhang et al. 2026 — DJM|DJM]] could not compare against Dou et al. two years after that; and this vault spent its top two directions on questions SGP had already answered in general form. The [[Prior art index]] blind-spot list is the map of where to look — **search by query, not by representation.**

Back to [[Home]].
