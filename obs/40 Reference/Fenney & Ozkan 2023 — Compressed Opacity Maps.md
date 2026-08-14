---
title: Compressed Opacity Maps for Ray Tracing
authors: [Simon Fenney, Adnan Ozkan]
affiliation: Imagination Technologies
year: 2023
venue: HPG 2023, pp. 23–31
doi: 10.2312/hpg.20231133
tags: [reference, not-in-vault, opacity, compression]
---

# Fenney & Ozkan 2023 — Compressed Opacity Maps for Ray Tracing

**Not in vault** — summarised from citations in [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps]]. Not read directly.

The **only prior work on compressing opacity micromaps**, and therefore the immediate point of comparison for [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Succinct Opacity Micromaps]].

As characterised there: it compresses to **50–25% of original size**, but is **quad-based** rather than triangle-based and so is not usable through any current graphics API. It also raises two schemes left unexplored — a 3-level quadtree, and a `wavelet mod 3` encoding.

For context on the vendor angle, Fenney is also cited in [[Barczak et al. 2024 — DGF|DGF]] for Imagination's architecture natively intersecting **pairs of edge-adjacent triangles**, which is why DGF measures its quad rate.
