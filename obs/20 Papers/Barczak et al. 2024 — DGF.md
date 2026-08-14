---
title: "DGF: A Dense, Hardware-Friendly Geometry Format for Lossily Compressing Meshlets with Arbitrary Topologies"
authors: [Joshua Barczak, Carsten Benthin, David McAllister]
affiliation: AMD
year: 2024
venue: PACMCGIT 7(3), Article 1 — HPG 2024
doi: 10.1145/3675383
tags: [paper, compression, geometry-format, ray-tracing, amd]
status: read
---

# DGF: A Dense, Hardware-Friendly Geometry Format

AMD's answer to the same problem NVIDIA solved with displaced micro-meshes — and an **explicit argument that the micro-mesh answer is wrong for general content**.

## Problem

Rasterisation systems (Nanite) already use lossy compressed meshlets decoded on the fly, but **ray tracing APIs require opaque acceleration structures built from uncompressed input**, and RT hardware uses low-density triangle representations designed for lossless float storage. So ray-traced applications cannot render the geometric complexity rasterisers can. DGF is a block-compressed, lossy geometry format for **arbitrary topologies**, designed for direct consumption by future fixed-function hardware.

## Core method

**Block format.** An array of **128-byte blocks**, each holding at most 64 triangles and 64 vertices, sized against the target hardware's cache line so a block fetch costs minimal cache-line transfers. All data for a given triangle lives in **one** block. Layout: a 20-byte header, then vertex offsets, an optional geometry-ID palette, a re-use buffer, and first-index and control bits packed back-to-front from the end.

**Vertex positions.** Vertices live on a **24-bit signed integer grid** (inspired by [[Karis et al. 2021 — Nanite]]), because floats have excessive precision near the origin and too little far from it. Per block: a 24-bit signed **anchor** per axis, an unsigned **1–16-bit offset** per vertex, and an **8-bit power-of-two exponent**. Decode is `V = (A + O)·2^(E−127)` — a 25-bit signed integer converted exactly to float, then losslessly scaled. The sum of the x/y/z bit widths must be a multiple of 4 to allow a hardware muxing decoder. A **single global quantisation factor prevents cracks** across blocks. See [[Watertightness and cracks]].

**Topology: generalised backtracking triangle strips.** A simplification of Deering's [1995] scheme with four 2-bit control codes — RESTART, EDGE1, EDGE2 and **BACKTRACK**, the last re-using the opposite edge of the predecessor so isolated triangles don't force a strip restart. Vertices of re-used edges are swapped to preserve winding; mixed winding restarts the strip. The index buffer is compressed with a **first-index bit** per index and an indirect **re-use buffer** whose index width is chosen per block (3–6 bits).

**API compliance.** DXR and Vulkan need a 29-bit primitive ID, 24-bit geometry ID and 1-bit opacity per triangle. Primitive IDs are **implicit** (a 29-bit block base plus in-block index). Geometry ID is concatenated with the opacity flag into a 25-bit value, stored either in *constant mode* (one 10-bit header value) or *palette mode* (shared MSBs, varying LSBs, 2-bit per-triangle selectors).

**Generation.** SAH-based clustering into ≤128-triangle clusters (following Benthin & Peters [2023]), global quantisation, then greedy block packing that repeatedly adds the unused triangle sharing the most vertices, ties broken by Morton code. Strip construction is a greedy walk of the adjacency graph specialised for valence 3, starting from a minimum-valence node with backtracking — a combination that "causes the strip to **walk block boundaries in a spiral pattern**, rather than venturing into the interior and becoming trapped". Most steps are incremental, but strip construction must be redone whenever a triangle is added, and it dominates encoder runtime.

## The argument against DMM

This is the vault's clearest statement of the case against displaced micro-meshes:

> This approach offers a very compact geometry representation and level of detail support. However, the approach **cannot represent arbitrary geometry topologies** due to the restriction of applying scalar adjustments to modify the interpolated displacement vectors. Models with complex topology, sharp features, or high depth complexity would need to be represented using a large number of base triangles, which eliminates the advantage. … Additionally, the **DMM base triangles are the coarsest available simplification** of geometry.

The concrete demonstration: Sponza as DMM — built with [[Maggiordomo et al. 2023 — Micro-Mesh Construction|Maggiordomo's]] toolchain — is 40K base triangles, 16M µ-triangles, **16 MB** and struggles with sharp corners and small high-frequency features; the same mesh as DGF at b=16 is 262K triangles and **1.23 MB**. "In this case, DGF needs no manual intervention to produce an acceptable result."

## Results

Encoding on an i9-13900KF; rendering on a Radeon 7900 XT via DXR intersection shaders.

- **Density vs DMM:** DGF is ~**3× larger** (2.87–3.29 vs 0.81–2.38 bytes/triangle) but **~30× faster to encode** (6–49 s vs 317–1661 s) and handles any topology. Bit width b=14 roughly matches DMM's average error.
- **Error stability:** "DGF error is more stable and predictable. The maximum error is always roughly 1.8× the average, and scales linearly with quantization level, while for **DMM the error can vary by as much as 10×**."
- **vs meshlets** [Kuth et al. 2024]: 2.87–3.29 vs ~4.0 bytes/triangle, at higher vertex duplication (1.53–1.58 vs ~1.23).
- **BVH:** reduces triangle leaf data **6–10×** and total BVH size by **50%**.
- **Traversal cost:** decoding costs up to **2.4× more time**, but reduces L2 misses (memory bandwidth) by **40–50%** — the ALU cost of scanning the strip is the price, the shrunken working set is the payoff. A DMM proxy reduces L2 misses further still, since its BVH covers only coarse base triangles.
- **Strips and quads:** average strip length 14.8–19.3 (vs 8–13.5 for meshoptimizer and tunnelling), and a **>90% quad rate**, which matters because several architectures natively intersect pairs of edge-adjacent triangles.

**Failure case, honestly reported:** San Miguel mixes huge backdrop triangles with high-polygon tableware, so the global quantisation factor starves the small geometry of precision. Fixes measured include b=24 (+18% bytes/triangle, artefacts eliminated).

## Limitations

Lower compression density than displacement-based approaches; software intersection is currently slower than uncompressed leaves (offset only by future hardware decode); the encoder is slow relative to Draco; non-manifold meshes with many triangles per edge cause frequent strip restarts; the locked-offset mode enabling animation costs 2–4× density.

Sec. 6 proposes both a **hardware strip-scan unit** and an **API extension** taking pre-computed DGF blocks in acceleration-structure builds, "just as they currently accept pre-compressed textures".

## Relation to other work

Cites [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] four times over — for all DMM error and size figures, the eight-model test set, the encoding-time baseline, and the toolchain behind its failure case. It does **not** cite [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] (same year), nor any of the displacement ray-tracing or opacity papers here; opacity appears only as a 1-bit field.

DGF is the surviving branch of this contest. As of 2026 the format is an actively developed open-source SDK with hardware support planned — see [[Graphics API and hardware support timeline]].

![[DGF.pdf]]
