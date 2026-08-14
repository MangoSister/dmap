---
title: MOC — Micro-Meshes and Geometry Compression
tags: [moc, micro-mesh, compression, construction]
---

# MOC — Micro-Meshes and Geometry Compression

**The question:** what representation lets a ray tracer render rasteriser-scale geometric detail?

The premise all these papers share is that [[Karis et al. 2021 — Nanite|Nanite]] already solved this for rasterisation, and ray tracing could not follow — because RT APIs demand opaque acceleration structures built from **uncompressed** input.

Two answers compete here, and one of them lost.

## The two bets

**Displaced micro-meshes (NVIDIA).** A coarse base mesh plus scalar displacements on a barycentric grid, self-bounding by a prismoid, so the BVH covers only base triangles. Extremely dense — under 1 byte per triangle — with built-in LoD.

**Compressed clusters of arbitrary triangles (AMD, Epic).** Block-compress ordinary triangles with any topology. ~3× less dense, but no representational restriction.

By 2026 the second bet won. The DMM extension was withdrawn in favour of cluster acceleration structures; DGF is a live SDK with hardware support planned. See [[Graphics API and hardware support timeline]].

## The papers

| | Year | Contribution |
|---|---|---|
| [[Maggiordomo et al. 2023 — Micro-Mesh Construction\|Micro-Mesh Construction]] | 2023 | The reference construction pipeline. 15:1 average compression, under 5 min/model |
| [[Dou et al. 2024 — Differentiable Micro-Mesh Construction\|Dou et al.]] | 2024 | Replaces the staged pipeline with end-to-end differentiable optimisation. +1.3–2.3 dB |
| [[Barczak et al. 2024 — DGF\|DGF]] | 2024 | AMD's 128-byte block format for arbitrary topology. The argument against DMM |
| [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes\|Animated DMM]] | 2024 | Makes DMMs skinnable. An animated DMM is two quadratic triangular Bézier patches |
| [[Zhang et al. 2026 — DJM\|DJM]] | 2026 | Closed-form Jacobian makes distortion and bijectivity exactly computable. Current state of the art |
| [[Mendiratta et al. 2026 — NeuBase\|NeuBase]] | 2026 | The same coarse-base-plus-offset idea for Catmull–Clark surfaces, with learned bases |
| [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry\|Tetrahedral cages]] | 2026 | The same team abandons displacement for cages. 584M animated triangles at 60 fps |

[[Maggiordomo et al. 2023 — Micro-Mesh Construction|Micro-Mesh Construction]] is the hub — **every other paper here cites it**, and none of the others cite each other except through it.

## The argument, in the participants' own words

**DGF against DMM:**

> the approach **cannot represent arbitrary geometry topologies** … Models with complex topology, sharp features, or high depth complexity would need to be represented using a large number of base triangles, which eliminates the advantage.

Demonstrated on Sponza: 16 MB as DMM with visible artefacts at sharp corners, **1.23 MB as DGF** with none and no manual tuning.

**The measured trade:**

| | DMM | DGF |
|---|---|---|
| Bytes/triangle | 0.81–2.38 | 2.87–3.29 |
| Encode time | 317–1661 s | 6–49 s |
| Error predictability | varies up to **10×** | max ≈ 1.8× average, always |
| Topology | height field over a base triangle, genus 0 | arbitrary |

So DMM is ~3× denser; DGF is ~30× faster to encode, more predictable, and unrestricted.

**The concession from the DMM side.** [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al.]] — the same AMD team as DGF — agree the topological limit is real, and note DGF "is particularly geared at being consumed by future ray tracing hardware".

**And the verdict two years later,** from the same first author in [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry|Gruen et al. 2026]]:

> **DMMs have not been widely adopted**, as their representation is tied to piecewise manifold objects, which limits their applicability when dealing with real-world content.

Note the reason given is **topological**, not the API withdrawal — the objection DGF raised in 2024, restated as settled fact by the author of the paper that tried to save DMMs.

## The construction problem

Independent of format, converting an asset into a base mesh plus displacements is hard because the objectives conflict: coarseness, reprojectability, isotropy, minimal shell volume. See [[Base mesh quality objectives]].

Three generations of answer: **hand-designed collapse costs** (Maggiordomo), **end-to-end gradients** (Dou), and **analytic Jacobian distortion** (DJM 2026, [[2026 successors — DJM and NVIDIA foliage]]).

The sharpest critique is Dou et al.'s: the shell volume is essentially fixed the moment the base mesh exists, yet its true value is only known **after** baking — so any staged pipeline optimises the wrong thing at the wrong time.

## Where the value actually came from

Not fewer bytes for geometry — a **cheaper acceleration structure**. µ-meshes cut BVH build time ÷4 and size ÷6; DGF cuts leaf data 6–10× and total BVH 50%. Both pay for it in tracing time. See [[Hardware ray tracing and micro-geometry]].

## Key concepts

[[Shell, prism and prismoid]] · [[Micro-triangle and subdivision level]] · [[Base mesh quality objectives]] · [[Watertightness and cracks]] · [[Micromap]] · [[Tessellation-free vs pre-tessellation]]

Adjacent: [[MOC — Ray Traced Displacement Mapping]] for rendering displacement without a format, and [[MOC — Opacity Micromaps]] for the sibling micromap kind that survived.

Open directions from this cluster: [[Deformation-aware displacement fields]] · [[Inverse and differentiable micro-geometry]] · [[Hybrid displacement and opacity micromaps]] · [[Base mesh quality objectives]] as an unsettled question rather than a solved one.
