---
title: Home
tags: [moc, home]
---

# Micro-Geometry for Ray Tracing

This vault covers one research thread: **how to render, construct and compress sub-triangle geometry for hardware ray tracing.** Twelve papers spanning 2020–2026, plus the API and silicon story that runs underneath them.

The thread starts from a gap. Rasterisers could already render film-scale geometric detail — [[Karis et al. 2021 — Nanite|Nanite]] shipped in 2021 — but ray tracers could not, because ray-tracing APIs demand opaque acceleration structures built from **uncompressed** geometry. Everything here is an attempt to close that gap, from three directions:

- **Render displacement without building it** → [[MOC — Ray Traced Displacement Mapping]]
- **Compress geometry into a form hardware can trace** → [[MOC — Micro-Meshes and Geometry Compression]]
- **Skip the shader work that transparency forces** → [[MOC — Opacity Micromaps]]

And a fourth map that is not a summary of the papers but an argument about what they left out → [[MOC — Open Questions]].

## Start here

1. [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] — the ancestor of the displacement cluster, and a clear statement of the problem
2. [[Thonat et al. 2023 — RMIP]] — how it was made five times faster
3. [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] — the hub node; nearly everything else cites it
4. [[Graphics API and hardware support timeline]] — what actually shipped, and what was withdrawn

## The twist worth knowing early

[[NVIDIA 2022 — Ada Lovelace Architecture|Ada]] added two fixed-function units in 2022: an **Opacity Micromap Engine** and a **Displaced Micro-Mesh Engine**. Half the papers here orient themselves around the second one.

They diverged completely:

- **Opacity micromaps** were promoted to `VK_KHR_opacity_micromap` (May 2026), shipped in DXR 1.2, and cut any-hit cost by more than half in real games.
- **Displacement micromaps** were **withdrawn in February 2025**, their SDK archived, replaced by cluster acceleration structures — [[Karis et al. 2021 — Nanite|Nanite's]] idea, arriving in hardware ray tracing.

So the software displacement methods here outlived the hardware they were benchmarked against, and the compressed-cluster bet ([[Barczak et al. 2024 — DGF|DGF]]) beat the displaced-height-field bet. Read [[Graphics API and hardware support timeline]] before trusting any paper's framing of "current hardware".

## Timeline

| | |
|---|---|
| **2020** | [[Gruen et al. 2020 — Sub-Triangle Opacity Masks\|Sub-triangle opacity masks]] — the micromap is invented, in software |
| **2021** | [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)\|TFDM]] — displacement ray traced from a texture-space hierarchy |
| **2022** | [[NVIDIA 2022 — Ada Lovelace Architecture\|Ada]] ships both micromap engines; `VK_EXT_opacity_micromap` finalised |
| **2023** | [[Maggiordomo et al. 2023 — Micro-Mesh Construction\|Micro-Mesh Construction]]; and two rival answers to TFDM — [[Thonat et al. 2023 — RMIP\|RMIP]] and [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping\|Ogaki]] — at the same conference, not citing each other |
| **2024** | [[Dou et al. 2024 — Differentiable Micro-Mesh Construction\|Differentiable construction]] · [[Barczak et al. 2024 — DGF\|DGF]] · [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps\|Succinct OMM]] · [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes\|Animated DMM]] |
| **2025** | DMM withdrawn; RTX Mega Geometry; DXR 1.2 ships OMM; [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)\|PDM]] |
| **2026** | `VK_KHR_opacity_micromap` ratified · [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps\|CSM opacity micromaps]] · [[Mendiratta et al. 2026 — NeuBase\|NeuBase]] · [[Zhang et al. 2026 — DJM\|DJM]] · [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage\|NVIDIA foliage]] · [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry\|tetrahedral cages]] |

## All papers

**Displacement ray tracing** — [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] · [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]] · [[Thonat et al. 2023 — RMIP]] · [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]]

**Construction and compression** — [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] · [[Dou et al. 2024 — Differentiable Micro-Mesh Construction]] · [[Barczak et al. 2024 — DGF]] · [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes]] · [[Zhang et al. 2026 — DJM]] · [[Mendiratta et al. 2026 — NeuBase]]

**Opacity** — [[Gruen et al. 2020 — Sub-Triangle Opacity Masks]] · [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps]] · [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps]] · [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage]]

**Animation at scale** — [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry]]

## Concepts

[[Micromap]] · [[Shell, prism and prismoid]] · [[Micro-triangle and subdivision level]] · [[Min-max mipmap and conservative bounds]] · [[Base mesh quality objectives]] · [[Watertightness and cracks]] · [[Tessellation-free vs pre-tessellation]] · [[Any-hit shader cost]] · [[Hardware ray tracing and micro-geometry]] · [[Graphics API and hardware support timeline]]

**Differential geometry of displaced surfaces** — a five-note thread, derived rather than summarised from papers: [[The induced metric of a displaced surface]] (what the metric is and what it costs) · [[Obliquity and the integrability defect]] (what interpolated vertex normals break) · [[Laplace–Beltrami on displaced surfaces]] (turning the metric into a solvable operator) · [[Taylor-model bound pyramid]] (the joint bound structure that makes the metric conservative) · [[Anisotropic and spectral LoD error]] (what a coarser mip destroys, certified)

## Background and successors

[[Prior art index]] — coverage map of what all twelve papers cite, organised by territory, with the fields none of them touch. Start here when checking whether a gap is real.

Not in this vault — short stubs summarised from citations: [[Porumbescu et al. 2005 — Shell Maps]] · [[Smits et al. 2000 — Direct Ray Tracing of Displacement Mapped Triangles]] · [[Patterson et al. 1991 — Inverse Displacement Mapping]] · [[Tevs et al. 2008 — Maximum Mipmaps]] · [[Jeschke et al. 2007 — Smooth and Curved Shell Mapping]] · [[Reshetov 2019 — Cool Patches]] · [[Karis et al. 2021 — Nanite]] · [[NVIDIA 2022 — Ada Lovelace Architecture]] · [[Fenney & Ozkan 2023 — Compressed Opacity Maps]] · [[2026 successors — DJM and NVIDIA foliage]] · [[Nießner & Loop 2013 — Analytic Displacement Mapping]] (read from PDF — the smooth-base escape from the obliquity trade-off) · [[Crane et al. 2013 — Geodesics in Heat]] · [[Sugimoto et al. 2024 — Projected Walk on Spheres]] (both read from PDF — the two PDE backbones of the metric-queries project)

---

## Open questions

Underexplored directions, performance deliberately excluded — see [[MOC — Open Questions]] for the ranking and the provenance caveat. The four with the most detail:

[[Proximity and contact queries against micro-geometry]] · [[Surface measure and sampling on implicit displaced surfaces]] · [[Prefiltered coverage from opacity hierarchies]] · [[Hybrid displacement and opacity micromaps]]

## Projects

[[Project — Conservative metric queries without tessellation]] — execution plan (2026-08-11) for turning the metric thread into a SIGGRAPH submission and prototype: story, bound-structure design, four scoped applications with baselines, and a gated 22-week schedule.

## Notes on this vault

- Paper notes carry the papers' **own numbers** with the table or figure they came from. Where papers measure on different hardware or scenes, the notes say so rather than presenting a single ranking.
- Hardware and API facts were researched **August 2026** and are dated inline. Claims I could not confirm against a primary source are marked with `> [!warning]` callouts rather than stated flatly.
- `40 Reference/` notes are for work **not in this vault**, summarised from how the local papers cite it. Each says so at the top.
- Source PDFs are in `90 Attachments/` and embedded at the bottom of each paper note.
