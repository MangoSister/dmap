---
title: 2026 successors — DJM and NVIDIA foliage
year: 2026
tags: [reference, successor, reading-list, resolved]
updated: 2026-08-10
---

# 2026 successors — DJM and NVIDIA foliage

> [!success] Resolved — these are now in the vault
> This note began as a stub for two papers identified by web search but not held locally. Both have since been obtained and read in full, along with a third. It is kept as a redirect because several notes link here.

| Was a stub | Now |
|---|---|
| DJM: Compact Base Meshes for Displacement Mapping using Triangle Jacobians | [[Zhang et al. 2026 — DJM]] |
| Real-time Path Tracing of Massive Dynamic Foliage | [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage]] |
| Ray Tracing Massive Amounts of Animated Geometry | [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry]] |

Two corrections to what the stub asserted from search results:

- **DJM's venue is unconfirmed.** The PDF is a preprint with placeholder venue fields and no DOI, and never names SIGGRAPH 2026.
- **The foliage paper does not use displacement at all.** The stub described it as combining "partitioned TLAS, clustered LOD and opacity micromaps", which is right — but it never mentions micro-meshes or displacement mapping, and its geometry is explicit triangles throughout.

## Still not in the vault

- **Memory-Efficient Bounding Volume Hierarchies with Merged Nodes** — Jacob Haydel, Andrew Kensler, Cem Yuksel, Erik Brunvand, HPG 2026 (second place, Wolfgang Straßer Award).
- **Luton & Tricard 2025**, *Real-time rendering of animated meshless representations*, HPG 2025 — the tetrahedral-cage rasterisation work that [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry|Gruen et al. 2026]] extends to ray tracing, and its closest prior art.
- **Smith et al. 2000**, *Layered animation using displacement maps* — cited by [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al. 2024]] as "very similar to DMMs… even discusses animating displaced meshes", and by **nothing else in this vault**. The single most relevant uncited work for [[Deformation-aware displacement fields]].
- **Kraemer, Lacewell & Kanani 2025**, *Real Time Path-Tracing with NVIDIA RTX MegaGeometry* — cited by the foliage paper for close-range hero-tree techniques.
- The full HPG 2026 accepted-paper list was never retrieved; this is not exhaustive.

See [[Prior art index]] for what the vault's papers collectively survey.
