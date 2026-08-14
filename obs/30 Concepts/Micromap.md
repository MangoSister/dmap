---
title: Micromap
tags: [concept, primitive, opacity, displacement, api]
---

# Micromap

The unifying primitive of this vault, and the bridge between its two main clusters.

A micromap is, in [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett's]] definition, *"simply a linear array of values mapped into fixed sub-areas of a triangle specified by a space-filling curve."*

Three properties do all the work:

1. **Uniform recursive subdivision.** A triangle splits into 4 by midpoint edge splits, recursively, so at level `n` there are `4ⁿ` sub-triangles on a `2ⁿ × 2ⁿ` barycentric grid.
2. **Implicit position.** Because subdivision is uniform, no sub-triangle geometry is stored — position follows from index.
3. **Space-filling-curve order.** Values are laid out in Morton/Z-order, chosen for memory locality. Conveniently, that order equals depth-first traversal order, which [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] exploit to make each group of four index bits select one level of their DAG.

## Two official kinds

| | Opacity Micromap (OMM) | Displacement Micromap (DMM) |
|---|---|---|
| Stores | per-sub-triangle opacity state | per-µ-vertex scalar displacement |
| Purpose | skip the [[Any-hit shader cost\|any-hit shader]] | compress geometry, shrink the BVH |
| Status 2026 | ratified `VK_KHR_opacity_micromap`, DXR 1.2 | **withdrawn**, replaced by cluster acceleration structures |

Only opacity is generally available. See [[Graphics API and hardware support timeline]] for how they diverged.

## Opacity states

| 2-state (1 bit) | 4-state (2 bits) |
|---|---|
| `0` fully transparent | `00` fully transparent |
| `1` fully opaque | `01` fully opaque |
| | `10` unknown transparent |
| | `11` unknown opaque |

*Unknown* means "resolve this some other way" — in practice an alpha-texture lookup. The states can be **converted down** to 2-state where the lookup isn't wanted, and shipping titles use this asymmetrically: *Indiana Jones* runs indirect rays with a 2-state approximation while keeping shadow rays fully conservative at 4-state.

The choice of subdivision level means different things in the two modes. In 4-state it trades runtime against memory. In **2-state it changes the silhouette** — no `unknown` means no any-hit, so the shape is visibly quantised.

## Special indices and reuse

Micromaps are attached to triangles through an **index buffer**, which is also the deduplication mechanism: many triangles can point at one micromap. Vulkan additionally defines negative sentinel indices — `FULLY_TRANSPARENT`, `FULLY_OPAQUE`, `FULLY_UNKNOWN_TRANSPARENT`, `FULLY_UNKNOWN_OPAQUE` — that encode a whole-triangle-uniform state with **zero stored data**.

These are not an optimisation but a necessity: [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett's]] scenes contain **millions** of special indices against tens of thousands of real micromaps.

## The `4ⁿ` problem

Micromap data quadruples per level, which is what every compression paper here attacks:

- [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Succinct]] — merge identical siblings into a 4-way tree, encode succinctly. Up to **110×**, but lookup cost roughly quadruples per level, so it is unusable past level 3–4.
- [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Common subtree merging]] — merge identical subtrees across the whole scene into a DAG with 16× fan-out. Less compression per-micromap than succinct, but **≤ 2 reads per level and no backtracking**, so it stays fast at level 10 (**351×** on Namaqualand).
- [[Fenney & Ozkan 2023 — Compressed Opacity Maps]] — lossy vector quantisation, but quad-based and not exposed through any API.

## An authoring trap

Micromaps are **not** just a specialised texture. They are tied to the texture *and* to the uv coordinates. Overlapping or misaligned UVs make each triangle produce a **unique** micromap despite sharing a texture — *increasing* acceleration-structure bandwidth rather than reducing it.

## Origin

The concept is [[Gruen et al. 2020 — Sub-Triangle Opacity Masks]], whose STOC bits encoded 3 states in 2 bits and whose future-work section specified what hardware later shipped. One detail survives as a footnote: **its subdivision index ordering differs from the Vulkan/DXR one**, including rounding at edges and corners.
