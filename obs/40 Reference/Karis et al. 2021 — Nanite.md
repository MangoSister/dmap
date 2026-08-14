---
title: "Nanite: A Deep Dive"
authors: [Brian Karis, Rune Stubbe, Graham Wihlidal]
affiliation: Epic Games
year: 2021
venue: SIGGRAPH 2021 Advances in Real-Time Rendering course
tags: [reference, not-in-vault, virtualized-geometry, clusters, lod]
---

# Karis et al. 2021 — Nanite: A Deep Dive

**Not in vault** — summarised from citations in [[Barczak et al. 2024 — DGF]], [[Maggiordomo et al. 2023 — Micro-Mesh Construction]], [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]], [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]] and [[Dou et al. 2024 — Differentiable Micro-Mesh Construction]]. Not read directly.

Unreal Engine's virtualised geometry system: meshes are decomposed into **~128-triangle clusters** with a hierarchical LOD DAG, compressed, and streamed — for **rasterisation**.

It appears throughout this vault as the standard everyone measures against, and it is the reason the whole micro-geometry problem exists in ray tracing: rasterisers could already render this complexity, ray tracers could not, because RT APIs demand opaque acceleration structures built from uncompressed input.

Two concrete inheritances in [[Barczak et al. 2024 — DGF|DGF]]: the **24-bit integer vertex grid**, and the comparison point of **~9 bytes/triangle in memory, 5.6 on disk** including LOD hierarchy and attributes.

The cluster idea is also the through-line to what replaced displaced micro-meshes — NVIDIA's cluster acceleration structures brought Nanite-style clustering into hardware ray tracing. See [[Graphics API and hardware support timeline]].
