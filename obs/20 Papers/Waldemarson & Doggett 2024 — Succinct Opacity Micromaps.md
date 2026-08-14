---
title: Succinct Opacity Micromaps
authors: [Gustaf Waldemarson, Michael Doggett]
affiliation: Lund University; Arm
year: 2024
venue: PACMCGIT 7(3), Article 45 — HPG 2024
doi: 10.1145/3675385
project: https://gustafwaldemarson.com/pages/publications/succinct-opacity-micromaps/
tags: [paper, opacity, compression, succinct-data-structures, vulkan]
status: read
---

# Succinct Opacity Micromaps

Attacks the **memory footprint of the opacity micromap itself**, and along the way supplies the clearest exposition in this vault of how the shipped [[Micromap]] format actually works.

Unusually candid about its own negative result, which is what makes it useful.

## Problem

Any-hit sits in the innermost loop of the ray-tracing pipeline and is the major bottleneck for alpha-masked foliage. Opacity micromaps let hardware traversal consult per-sub-triangle opacity **without invoking any-hit** — but the micromap data grows as `4ⁿ` and becomes a memory and bandwidth problem at useful subdivision levels. Secondarily, the barycentric-indexing algorithm in the Vulkan spec is a mass of opaque bit-interleaving.

## Background it establishes

A micromap is "simply a linear array of values mapped into fixed sub-areas of a triangle specified by a space-filling curve". Only two official kinds exist — **opacity** and **displacement** — and only opacity is generally available in Vulkan/DirectX.

| 2-state (1 bit) | 4-state (2 bits) |
|---|---|
| `0` fully transparent | `00` fully transparent |
| `1` fully opaque | `01` fully opaque |
| | `10` unknown transparent |
| | `11` unknown opaque |

*Unknown* values must be resolved by an alpha lookup, or **converted down** to the corresponding 2-state value where the lookup isn't wanted — as shipping titles do for indirect rays.

**Highest useful subdivision level.** In 4-state mode the level trades runtime for memory; in 2-state mode it directly changes the **silhouette**, since no `unknown` means no any-hit and the shape is visibly quantised. Choosing `n` so a subtriangle's longest edge falls below a pixel gives `log₂ max(|e₀|,|e₁|,|e₂|) ≤ n`.

**Special indices.** Vulkan defines negative sentinel triangle indices — `FULLY_TRANSPARENT`, `FULLY_OPAQUE`, `FULLY_UNKNOWN_TRANSPARENT`, `FULLY_UNKNOWN_OPAQUE` — that encode a whole-triangle-uniform state with **zero stored micromap**. The paper stresses these are essential: its scenes have **millions** of special indices against tens of thousands of actual micromaps. The index buffer is also the deduplication mechanism, letting many triangles share one micromap.

## Core method

**Succinct encoding of a 4-way tree.** A succinct structure occupies memory within a constant factor of the information-theoretic minimum (Jacobson 1989); since the number of binary trees with `n` nodes is the Catalan number ≈ `4ⁿ`, `2n` bits suffice.

1. **Merge (bottom-up):** for each level, collapse any subtriangle whose four children hold the same value into a **leaf**; otherwise mark it internal. This yields a "perfect tree".
2. **Encode:** a DFS emitting **two bit-vectors** — `Tree` (1 = internal and push four children, 0 = leaf) and `Data` (the leaf values).
3. **Look up without decompressing:** walk the bit-vector with a `bitscan` helper that advances the tree pointer while accumulating the leaf count, skipping intervening subtrees.

**A better `uv2index`.** In place of the spec's bit-interleaving, an explicitly geometric, tail-recursive algorithm: compute `w = 1−u−v`, then repeatedly decide which of four subtriangles — **L**eft, **M**iddle, **R**ight, **T**op — is hit, emit `4·index + {L,M,R,T}`, and update the coordinates. Two boolean flags, **`mid-flip`** and **`top-flip`**, track winding and rounding changes: recursing into T flips the local indices of L and R; recursing into M makes ties round to the middle rather than the right subtriangle.

It is **proven equivalent to the Vulkan reference up to subdivision level 15 using the ACL2 theorem prover**. A discrepancy appears at level 16; the authors note the reference must be rewritten past 16 levels anyway since it cannot handle values above 16 bits — and a level-16 micromap would be 4 GiB, so the point is academic.

## Results

Five scenes up to 27.9M triangles, on an RTX 3080 (Ampere, **no** OMM engine) and an RTX 4080 (Ada, **with** the engine).

**Compression works, decisively.** Typically **45%–15% of original size**; New Sponza at 12 subdivision levels compresses to **under 1% — a 110× reduction**. San Miguel 50.9 MB → 2.3 MB; Landscape 17.2 → 1.4 MB.

**Lookup does not.** Tree lookup is only competitive at subdivision levels 3–4; beyond that it is "arguably too slow to be of practical use, at least in its current form" — 56.9 ms against 14.5 ms for native Vulkan micromaps on Landscape.

Official micromaps: **best case −29% frametime, worst case +2%**. Software emulation alone ranges from −16% to **+30%**, i.e. it can lose outright.

A nice piece of hardware inference: Vulkan's `fast-build` and `fast-trace` flags produce **identical memory footprints** but fast-trace is slightly quicker, from which they conclude the NVIDIA driver implements a single micromap type with a more optimised access path for fast-trace.

## Limitations

The honest core: like Huffman coding, a large amount of memory may have to be decoded before the sought value is found, and the cost **roughly quadruples per subdivision level** — violating the random-access criterion that texture compression is expected to meet. Reaching the right-most child requires scanning all intervening subtrees. Also: a degenerate micromap that always alternates state produces *more* internal nodes than the original; the encoding is lossless only; and Algorithm 3 "probably does not translate well into hardware".

**Authoring guidance worth remembering:** micromaps are *not* just a specialised texture — they are tied to both the texture **and** the uv coordinates. Overlapping or misaligned UVs make each triangle produce a **unique** micromap despite sharing a texture, *increasing* acceleration-structure bandwidth. Modern assets that model leaves as one texture on a tessellated grid work well at small subdivision levels but make special indices essential.

The authors conclude the use case for opacity micromaps is "rather limited at the time of writing" — a judgement the 2025–26 shipping evidence in [[Graphics API and hardware support timeline]] has since overtaken.

## Relation to other work

Cites [[Gruen et al. 2020 — Sub-Triangle Opacity Masks]] as the origin of the concept, and shows side-by-side that the two subdivision index orderings differ. Cites [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] but explicitly scopes it out as displacement rather than opacity — the single citation edge between this vault's opacity and displacement clusters. The only prior OMM-compression work is [[Fenney & Ozkan 2023 — Compressed Opacity Maps]].

Its listed future work — extending the tree encoding to **displacement** micromaps, and adding 1-bit unknown-only modes — is picked up by the newer papers in this vault.

![[Succinct_Opacity_Micromaps.pdf]]
