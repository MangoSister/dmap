---
title: MOC — Ray Traced Displacement Mapping
tags: [moc, displacement, ray-tracing]
---

# MOC — Ray Traced Displacement Mapping

**The question:** how do you intersect a ray with a displacement-mapped surface **without** first building the displaced geometry?

Four papers, four answers, spanning 2021–2025. All share a premise — pre-tessellation's memory cost is unacceptable for tiled, dynamic or editable displacement — and all concede that when content is static and modest, [[Tessellation-free vs pre-tessellation|pre-tessellation still wins]].

## The papers

| | Year | Core idea | Space traversed |
|---|---|---|---|
| [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)\|TFDM]] | 2021 | min-max mipmap quadtree, affine-arithmetic bounds | texture space (2D) → world boxes |
| [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping\|Ogaki]] | 2023 | ray as a degree-2 rational curve in height | **entirely** texture space |
| [[Thonat et al. 2023 — RMIP\|RMIP]] | 2023 | rectangular range-minimum queries, 2D↔3D ping-pong | both, alternating |
| [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)\|PDM]] | 2025 | no hierarchy — direct marching in a rectified prism | world space, projected |

## The lineage

TFDM is the common ancestor; **all three later papers cite it as the baseline**.

- **RMIP** supersedes it directly, by the same lead author, fixing three named weaknesses: a tree structure that ignores the data, square nodes bounding anisotropic footprints, and binary yes/no pruning.
- **Ogaki** is a *parallel* answer from a different angle — same conference as RMIP, **and the two do not cite each other**. Read neither as "the" 2023 state of the art.
- **PDM** cites all three and benchmarks against RMIP and the NVIDIA Micro-Mesh API.

## How to read the numbers

Each speedup claim chains through **different hardware and different scenes**. Laid out honestly:

| Claim | Hardware | Against |
|---|---|---|
| TFDM: ~100× slower, 10–90× less memory, ~10,000× faster edits | RTX 2080 Max-Q | uniform pre-tessellation |
| Ogaki: 20–30% of pre-tessellated speed, ~1/60 memory | Apple M1 Ultra (CPU) | uniform pre-tessellation |
| RMIP: **×5 average, ×11 best**, 3× less memory | RTX 3080 | TFDM |
| PDM: 40–60% faster primary, **2×–13× beauty**, 108 B/triangle | RTX 4090 | RMIP |

There is no single ranking here. RMIP → PDM is the only directly comparable pair (identical hardware, measurements supplied by RMIP's author), and even that compares a general renderer against one tuned for interactive editing on low-poly base meshes.

## What differentiates them

**Robustness.** [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] alone renders base triangles that are **degenerate in uv space or world space**, or whose vertex normals cause self-intersections — cases TFDM cannot render at all. This is his headline result, not speed.

**Editability.** [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] alone needs **zero acceleration rebuild** on a displacement edit — 16.7 fps full-screen against RMIP's 2.9 on the same scene. RMIP rebuilds in ~5 ms for a 4k map, TFDM in ~0.1 ms for the BLAS.

**Generality.** [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki]] alone does **shell mapping** — arbitrary instanced geometry in the shell, not just a height field — via a zero-memory implicit quadtree. That is what makes woven textiles, feathers and fur practical, and it is what he doubts micro-meshes can extend to.

## Shared machinery

Nearly all of it recurs across the four:

- [[Shell, prism and prismoid]] — the per-triangle container, in three variants
- [[Min-max mipmap and conservative bounds]] — the acceleration idea, and RMIP's anisotropic replacement
- [[Watertightness and cracks]] — nobody solves UV seams
- [[Reshetov 2019 — Cool Patches]] — used by both Ogaki and PDM for prism sides
- [[Hardware ray tracing and micro-geometry]] — the intersection-shader tax

## Where this thread went

Toward hardware, then away from it. TFDM explicitly asked for hardware displacement support; [[NVIDIA 2022 — Ada Lovelace Architecture|Ada]] delivered it; it was **withdrawn in 2025** ([[Graphics API and hardware support timeline]]). Meanwhile displacement moved into cluster-based adaptive tessellation.

The irony worth noting: these software methods **outlived the hardware they were competing against**.

Adjacent reading: [[MOC — Micro-Meshes and Geometry Compression]] for the format side, and [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes]], which borrows TFDM's implicit-quadtree traversal for skinned micro-meshes.

Open directions from this cluster — all of them ask these structures a question other than *what does this ray hit*: [[Proximity and contact queries against micro-geometry]] · [[Surface measure and sampling on implicit displaced surfaces]] · [[Non-ray queries — slicing, volume and mass properties]]
