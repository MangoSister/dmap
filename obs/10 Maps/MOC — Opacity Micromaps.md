---
title: MOC — Opacity Micromaps
tags: [moc, opacity, any-hit, compression]
---

# MOC — Opacity Micromaps

**The question:** how do you ray trace alpha-masked foliage without paying for an any-hit shader at every leaf?

This is the cleanest success story in the vault: a 2020 research prototype that became fixed-function silicon, then a ratified cross-vendor standard, then a measured win in shipping games — with the research since moving on to compressing the data it created.

## The arc

| Year | Event |
|---|---|
| 2020 | [[Gruen et al. 2020 — Sub-Triangle Opacity Masks\|Gruen et al.]] propose STOC bits and prototype in software. Up to 86% of tests skipped, 40% faster |
| 2022 | [[NVIDIA 2022 — Ada Lovelace Architecture\|Ada]] ships the Opacity Micromap Engine; `VK_EXT_opacity_micromap` finalised |
| 2024 | [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps\|Succinct OMM]] compresses the data up to 110× — but lookup is too slow past level 3–4 |
| 2024–25 | *Indiana Jones* and *Alan Wake 2* ship it. Any-hit samples fall 17% → 3% |
| 2025 | DXR 1.2 brings OMM to DirectX |
| 2026 | `VK_KHR_opacity_micromap` ratified. [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps\|CSM]] compresses 351× *and* stays fast |
| 2026 | [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage\|NVIDIA]] uses OMMs to **replace geometry**, not just accelerate it — 60M plants at 60 fps |

Details and sources: [[Graphics API and hardware support timeline]].

## Why it works

An [[Any-hit shader cost|any-hit shader]] interrupts fixed-function traversal. Classifying sub-triangle regions as provably opaque or transparent ahead of time lets traversal accept or reject in hardware.

[[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]] wrote the specification for what shipped, two years early:

> Future ray tracing hardware implementations could store STOC bits **directly in the BVH** … The STOC bits would allow the hardware to skip all `anyHit` shader calls for all intersections corresponding to fully transparent or fully opaque sub-triangles.

## The compression contest

Micromap data grows `4ⁿ`. The two entrants make **opposite trades**, and both are right about the other:

| | [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps\|Succinct]] | [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps\|Common subtree merging]] |
|---|---|---|
| Structure | succinct 4-way tree, two bit-vectors | sparse DAG, 16× fan-out, scene-wide merging |
| Best ratio | 110× (New Sponza, level 12) | **351×** (Namaqualand, level 10); 517× at 4-state |
| Per-micromap density | **higher** — 3 KiB vs 9 KiB on the same tree | lower |
| Lookup | scans intervening subtrees; cost ×4 per level | **≤2 reads per level, no backtracking** |
| Usable to | level 3–4 | level 10, at alpha-texture speed |

Chernaik et al. concede the density point generously — "This supports the authors' claims that the succinct trees are extremely efficient" — and then name the reason it doesn't matter: a compression scheme for a **random-access** structure must stay random-access.

Both are still slower than hardware OMMs. The realistic bar for a software method is matching an alpha-texture lookup, which CSM clears at ten subdivision levels.

## Things the papers teach that the specs don't

**Skip rate is the wrong metric.** [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]] hit a 93% skip rate and gained 1–2%, because the texture had almost no *fully opaque* sub-triangles so rays never terminated early.

**Special indices are essential, not an optimisation.** Real scenes have millions of uniform-opacity triangles against tens of thousands of real micromaps.

**UV layout determines whether OMMs help or hurt.** Overlapping or misaligned UVs make each triangle generate a *unique* micromap despite sharing a texture — raising acceleration-structure bandwidth.

**2-state changes the silhouette**, it doesn't just save memory: with no `unknown` state there is no any-hit call, so the shape is quantised. Shipping practice splits the difference — *Indiana Jones* uses 2-state for indirect rays and conservative 4-state for shadows.

## The third use of an OMM

The cluster's arc has three stages, and the third arrived in 2026. An opacity micromap started as a way to **skip any-hit work** ([[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]]), became a thing to **compress** ([[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]], [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]]), and is now a way to **replace geometry outright** ([[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|van Antwerpen et al.]]) — twigs and needles discarded in favour of three proxy triangles carrying opacity masks, with the swap decided by a memory cost model.

Worth noting how disconnected that third stage is: NVIDIA's paper cites **none** of the first two stages, treating OMMs as an API feature documented by vendors rather than a research topic. The compression literature and the production literature are not talking to each other.

## An open speculation

[[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] observe that the fraction of `unknown` states falls sharply as subdivision rises, and suggest that at high enough levels micromaps "include sufficient detail to describe underlying shapes directly" — so 2-state would suffice and **alpha textures could be dropped entirely**. Their binary-alpha-texture compression result (0.046–0.30 bits/texel) points the same way.

## Key concepts

[[Micromap]] · [[Any-hit shader cost]] · [[Micro-triangle and subdivision level]] · [[Graphics API and hardware support timeline]]

Prior art: [[Fenney & Ozkan 2023 — Compressed Opacity Maps]]. Successor: the foliage paper in [[2026 successors — DJM and NVIDIA foliage]], which combines OMMs with clusters.

Open directions from this cluster: [[Prefiltered coverage from opacity hierarchies]] · [[Dynamic opacity micromaps]] · [[Hybrid displacement and opacity micromaps]] · [[Authoring, UV layout and micromap efficiency]]

Adjacent: [[MOC — Micro-Meshes and Geometry Compression]] — the sibling micromap kind, which did not survive.
