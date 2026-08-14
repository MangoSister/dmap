---
title: Real-time Path Tracing of Massive Dynamic Foliage
authors: [Dirk Gerrit van Antwerpen, Pascal Gautron, Edd Biddulph, Christoph Kubisch, Jan Schmid, Martin Stich]
affiliation: NVIDIA
year: 2026
venue: PACMCGIT 9(4), Article 51 — HPG 2026 track
doi: 10.1145/3820021
tags: [paper, opacity, foliage, lod, clusters, ptlas, production-system]
status: read
---

# Real-time Path Tracing of Massive Dynamic Foliage

The most consequential paper in the vault for anyone working on [[MOC — Open Questions|open directions]] — a complete production system that occupies a lot of territory. **60 million uniquely animated plants across 25 km², path traced at 60 fps on an RTX 4090.**

> [!note] Venue
> The PDF itself never says "HPG"; PACMCGIT 9(4) is the HPG 2026 track but is not stated in the paper. No project page, no supplemental video.

## Problem

Rasterisation animates foliage in the vertex shader, where deformed triangles live in transient on-chip storage. Ray tracing has no equivalent — *"all bottom-level acceleration structures must already reside in device memory and be valid before ray dispatch begins."* Replicating uniquely skinned geometry per plant is prohibitive in memory and build cost.

## Core method

Everything, including leaves and needles, is **explicit geometry — no alpha textures in the source assets.** Plants are skeletons with rigid meshes on bones; animation is rigid per-bone rotation driven by a global wind field, chosen specifically *"to avoid building and storing unique acceleration structures per plant for each frame."*

The acceleration hierarchy is `PTLAS partition → instance → CBLAS → CLAS → OMM`, built on three API features ([[Graphics API and hardware support timeline]]): opacity micromaps, **cluster acceleration structures**, and the **partitioned TLAS**.

**Skeleton simplification.** Bones are merged with animation error bounded by a first-order motion model (explicitly *not* a conservative bound, but "accurate enough"). Simplified skeletons have fewer degrees of freedom, so rather than bounding cumulative error — "impractically loose… particularly higher up in trees" — they **refit the wind coefficients by matching statistical moments**, the mean and variance of mesh motion, via non-negative least squares. LOD pose discontinuities are crossfaded.

**Conservative edge contraction.** Quadric error "does not penalize loss of aggregate surface area", so standard contraction collapses leaves and needles to degenerate lines and visually thins foliage. Their fix reads the **square root of the quadric error as a proxy for local feature scale**: run an unconstrained collapse sequence to record when each triangle degenerates, propagate those thresholds back, then re-run refusing any contraction within 95% of a feature's collapse error. Result: "shallow bumps wash out into the surrounding surface, while isolated leaves and long, thin needles and twigs remain as distinct geometry."

**Opacity micromap replacement** — the headline contribution, and the mechanism that recovers the simplification the above step gives up. A PCA-aligned OBB BVH chunks the residual triangle soup; each chunk is replaced by **three mutually perpendicular equilateral proxy triangles** carrying unique OMMs — three being "the minimum needed to capture geometry from all directions". Whether to replace is decided by a **memory cost model**: *"A chunk is approximated with OMMs only if C_OMM < C_tri."*

The contrast they draw with Nanite is the crux:

> Nanite foliage is **error-driven**: it simplifies aggressively and switches to a voxel representation when the error of the geometric simplification would exceed that of the voxel grid. In contrast, our approach is guided by **memory consumption**: we simplify conservatively and switch to OMMs only when these are found to be more memory-efficient.

**OMM dithering.** They deliberately refuse partial opacity:

> We explicitly avoid representing partial opacity using the unknown OMM state, thereby avoiding any-hit overhead. Instead, we encode opacity in OMMs using only the two discrete states fully opaque or fully transparent. We use OMM dithering to approximate partial opacity, yielding comparable Monte Carlo noise to stochastic hit rejection but without any-hit overhead.

Coverage is preserved by softmax-weighting each source triangle's projected area across the three proxies, renormalising so total assigned coverage matches the original area, then **probabilistically discarding opaque micro-triangles**. Without it, LOD 1+ screen coverage is **11% too high** and trees look too opaque and too dark. See [[Any-hit shader cost]] and [[Micromap]].

**Baked shading.** Each opaque micro-triangle stores normal, UVs and a material *reference*, front and back, at 128 bits per sample (octahedral normals, fixed-point UVs) against 1 opacity bit per micro-triangle — opacity is 16× supersampled, so dithering is effectively 4-bit. Micro-triangles are grouped in blocks of 32 with a **32-bit residence mask** described as *"a compact, 16× sub-sampled mipmap of the higher-resolution opacity information within the micromap"* — but used only to index a sparse shading buffer, never as a coverage value.

A projection-induced normal bias (projection amplifies grazing-angle coverage) is corrected at shading time by nudging the normal toward the view direction with a quadratic fitted to a **randomly oriented triangle soup**.

**Distant vegetation.** Beyond an animation cutoff, small plants are stamped into the terrain via **XOR delta maps** — chosen because XOR is commutative, associative and self-inverse, so layer order is irrelevant, removal equals addition, and updates run with atomics. Larger plants get one aggregate BLAS per layer.

**PTLAS management.** No BLAS or CBLAS is ever rebuilt at runtime; all motion is instance transforms. A fixed instance budget, lock-free allocation from a free list, garbage collection decoupled from allocation, and re-partitioning every frame via a quadtree cut at ≤1024 instances per partition.

## Results

25 km², 60M plants from 230 models, RTX 4090, 1 spp with two indirect bounces, 1440p → 4K via DLSS, **60 fps**.

- **PTLAS instead of TLAS reduces frame time by 40%.**
- **3M instances sustained** versus the ~4 billion needed for full detail — a ~1300× reduction.
- Clustering plus OMM replacement **reduces LOD 1+ acceleration-structure memory by 46×**, "which is what makes instance merging feasible".
- Offline preprocessing for all 230 models: **70 s CPU + 2 s GPU**.
- Negative results worth knowing: intersecting Nanite's voxel brick format in an intersection shader was **~4× slower** than hardware primitives, and sparse linear swept spheres needed **~4× more memory**.

Validated with 𝗟LIP on converged undenoised images; error is "mainly interior shading, not silhouette mismatch". No comparison against a shipping production system — explicitly out of scope.

## Limitations

- *"Our pipeline optimizes for memory usage rather than progressive geometric simplification with camera distance."*
- Screen-space geometric complexity can **increase** at coarser LODs, because LOD 1+ geometry is reused over a shrinking footprint — *"We rely on a denoiser to suppress the resulting high-frequency noise."*
- *"Per-LOD OMM bakes could relax this, but at the cost of forfeiting CLAS reuse."*
- LOD choices stay near-optimal only while wind strength, planting density and resolution match the offline estimates.

Future work: translucent foliage via OMM dithering, aggregate BSDFs for distant layers, and **OMM impostors beyond foliage** — "fences, webs, and other cellular/lattice-like geometry".

## Relation to other work

> [!warning] Disjoint from the micromap research literature
> This paper cites **none** of: [[Gruen et al. 2020 — Sub-Triangle Opacity Masks]], [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps]], [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps]], [[Maggiordomo et al. 2023 — Micro-Mesh Construction]], [[Barczak et al. 2024 — DGF]], [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]], [[Thonat et al. 2023 — RMIP|RMIP]], or any prefiltered-occlusion work. It treats OMMs purely as an **API feature**, citing vendor documentation, not as a research topic with prior art. Micro-meshes are never mentioned at all.

Nanite is discussed substantively but cited only as Unreal documentation; micro-poly ray tracing is credited to Benthin & Peters 2023. The closest acknowledged prior art for the OMM proxies is Décoret et al.'s billboard clouds, from which they "take the opposite tack".

## What this means for the open questions

Assessed in detail in [[MOC — Open Questions]]. In brief: it **partially occupies** [[Hybrid displacement and opacity micromaps|geometry-becoming-coverage]] — that substitution is its core contribution — but the crossover is memory-driven, offline, one-shot, and reused verbatim across all coarser LODs, with per-LOD opacity re-baking explicitly rejected. It **does not touch** [[Prefiltered coverage from opacity hierarchies|prefiltered coverage]] (it actively chooses dithering over filtering), [[Proximity and contact queries against micro-geometry|proximity queries]], [[Surface measure and sampling on implicit displaced surfaces|surface measure]], or [[Dynamic opacity micromaps|dynamic opacity]].

![[Real-time_Path_Tracing_of_Massive_Dynamic_Foliage.pdf]]
