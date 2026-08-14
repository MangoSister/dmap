---
title: Common Subtree Merging Compressed Opacity Micromaps
authors: [Thomas Chernaik, Jaina Modisett, Markus Billeter]
affiliation: University of Leeds
year: 2026
venue: PACMCGIT 9(4), Article 50 — HPG 2026
doi: 10.1145/3820017
tags: [paper, opacity, compression, dag, lod, vulkan]
status: read
---

# Common Subtree Merging Compressed Opacity Micromaps

The direct answer to [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Succinct Opacity Micromaps]], and the newest paper in this vault. It trades some compression ratio for **random access that stays fast at high subdivision levels** — precisely the weakness its predecessor admitted.

## Problem

Opacity micromaps cost memory *on top of* the alpha textures they accelerate, and the data grows ×4 per subdivision level. The paper's motivating datum is from shipping practice: *Indiana Jones and the Great Circle* capped at **six subdivisions to stay inside a 128 MiB budget**. The goal is **lossless** compression supporting **random-access sampling directly in compressed form** at real-time rates.

## Core method

**Merge identical subtrees into a DAG.** A micromap's recursive subdivision is a quadtree; build it *sparsely* (omit fully transparent subtrees) and merge identical subtrees into a directed acyclic graph — the same idea as sparse voxel DAGs, applied to the triangular domain. Each triangle indexes a **micromap descriptor** holding a subdivision count and a root pointer. Crucially, **root nodes may sit deeper in the graph**, so triangles with fewer subdivisions share nodes with deeper ones.

**16× fan-out.** Rather than a 4-ary DAG, two standard subdivision levels are collapsed into one DAG level. Two reasons: it **halves graph depth**, so fewer *dependent* memory reads during traversal (depth 5 covers ten subdivision levels — over a million sub-triangles); and it packs neatly into 32-bit words. Odd subdivision counts are handled by zero-padding the root, which is nearly free given sparsity.

**Node layout.** An interior node is a 16-bit **child mask** plus an optional 16-bit **aggregate opacity** in the same word, followed by one 32-bit pointer per extant child. A leaf node is a child mask plus sixteen 16-bit (2-state) or 32-bit (4-state) leaf masks. Worst case — nothing mergeable, no sparsity — is only **1.27× larger** than an uncompressed OMM.

**Hierarchical opacity.** The otherwise-free 16 bits per interior node store the **fraction of non-empty leaf sub-triangles within that subtree** — conceptually a mipmap of the alpha texture, or a 2D simplification of prefiltered occlusion. It can be read as a hit probability, as an alpha value for blending, or as input to stochastic transparency. Desired traversal depth follows from the triangle's screen-space area as `⌊log₁₆ A⌋`.

**Traversal without backtracking.** Barycentrics are converted to a sub-triangle index using the **unmodified `VK_EXT_opacity_micromap` spec function** — because the Z-order space-filling curve order equals depth-first traversal order, **each group of four bits of the index selects one DAG level**. Per level: extract four bits, test that bit in the child mask, and if set follow the pointer at index `bitCount(mask & ((1<<c)−1))`. At most **two memory reads per level**, no backtracking. Contrast [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|the succinct encoding]], where reaching the right-most child means scanning every intervening subtree.

**Construction** is an offline CPU bake: generate uncompressed micromaps with the NVIDIA OMM SDK, then build bottom-up, hashing each constructed node against previously seen ones. **The hash table and buffer are reused across all micromaps of a mesh and optionally the entire scene** — which is what drives the headline ratios.

## Results

RTX 5070 Ti, 1920×1080, Vulkan ray queries, four foliage-heavy scenes.

**Compression at 10 subdivisions (2-state), micromaps only:**

| Scene | Uncompressed | Compressed | Ratio |
|---|---|---|---|
| Namaqualand | 3294 MiB | **9.37 MiB** | **351×** |
| Forest | 173.1 MiB | 2.78 MiB | 62.3× |
| Bistro | 579.8 MiB | 15.66 MiB | 37.0× |
| Intel Sponza | 633.9 MiB | 68.73 MiB | 9.22× |

4-state reaches **517×** on Namaqualand. Whole-scene merging clearly beats per-mesh merging, though per-mesh remains necessary for on-demand loading.

**Headline (Forest):** 1.2M unique alpha-masked triangles, each subdivided into ~a million sub-triangles, in **under 6 MiB** versus 175 MiB uncompressed — with primary visibility ≤ 2 ms and one shadow ray ≤ 1 ms, a **worst-case overhead of 8% and 14%** against uncompressed micromaps.

**Performance:** at 4 subdivisions compressed ≈ uncompressed; at higher levels compressed is slightly more expensive but stays **similar to or better than an alpha texture lookup**. Hardware OMMs still outperform all three software methods.

**The comparison that matters**, stated generously:

> we find that their method achieves higher compression rates … This supports the authors' claims that the succinct trees are extremely efficient. Nevertheless, the authors state that the succinct encoding becomes impractical at higher subdivision levels due to the decoding cost. Here, the common subtree merging has a clear advantage: even with ten subdivisions, we demonstrate performance comparable to alpha texture lookups.

A bonus result: the same 16× fan-out DAG applied to **thresholded binary alpha textures** reaches **0.046–0.30 bits per texel**.

## Limitations

Benefits are small at low subdivision levels, especially per-mesh. Integrating hierarchical opacity into a full renderer "is still a challenge". Lossy variants, variable-length pointers, and subtree transformations (reflections, translations) are all listed as unexplored gains. A tried-and-failed experiment is reported honestly: giving fully-opaque subtrees a special early-abort value showed **no clear improvement**.

One forward-looking belief worth noting: at very high subdivision levels micromaps "include sufficient detail to describe underlying shapes directly", so 2-state suffices and **alpha textures may be omitted entirely** — the percentage of `unknown` states falls sharply as subdivision rises.

## Relation to other work

Cites [[Gruen et al. 2020 — Sub-Triangle Opacity Masks]] as the origin of the concept, [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps]] as the nearest competitor and quantitative baseline, [[Fenney & Ozkan 2023 — Compressed Opacity Maps]] as the lossy alternative, and [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] once — for the displacement analogue.

That last citation is the vault's **best-sourced statement of the DMM deprecation**, from a peer-reviewed 2026 paper rather than a repository notice:

> While initially available through the provisional Vulkan vendor extension `VK_NV_displacement_micromap`, the extension is now deprecated by the more general **cluster acceleration structure** approach from `VK_NV_cluster_acceleration_structure`.

See [[Graphics API and hardware support timeline]] and [[Any-hit shader cost]].

![[Common_Subtree_Merging_Compressed_Opacity_Micromaps.pdf]]
