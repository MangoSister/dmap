---
title: Dynamic opacity micromaps
tags: [open-question, research-direction, opacity, animation]
priority: 7
---

# Dynamic opacity micromaps

> [!note] Analysis, not a claim from any paper. See [[MOC — Open Questions]].

> [!success] Confirmed open — 2026-08-10
> [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|van Antwerpen et al.]] bake all 230 plant models once, in 70 s CPU + 2 s GPU, and **never touch an OMM again**. When a leaf moves in wind, only a PTLAS instance transform is written; the CLAS geometry, opacity bits and shading samples are bitwise unchanged. Sharing is what forbids mutation — one OMM is referenced by hundreds of twig instances across LODs and even across plant *families*, and OMMs are acceleration-structure build inputs, so changing one implies a rebuild. Nothing in the paper discusses runtime OMM updates, incremental rebuilds, or deformable OMMs.
> The strongest production system to date reinforces the assumption rather than relaxing it.

Every opacity paper here **bakes offline and assumes the mask never changes**. [[Gruen et al. 2020 — Sub-Triangle Opacity Masks|Gruen et al.]] rasterise conservatively at level load; [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] state construction is deliberately out of scope; [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] build bottom-up on the CPU in an offline pass.

Games do not behave this way. Dissolve, burn, decay, growth, damage, bullet holes, melting and procedural wind are ordinary effects, and every one of them invalidates a baked micromap. The current answer is presumably to fall back to alpha testing — losing the [[Any-hit shader cost|entire benefit]] exactly when the effect is on screen and cost matters most.

**The open questions.** What does *incremental* micromap update cost, given that the value must stay conservative? Can a compressed form be updated locally at all — Chernaik et al.'s scene-wide subtree merging makes any local edit potentially non-local, since a shared node may back thousands of triangles. Is there a cheap conservative envelope for a *parameterised* family of masks (a dissolve threshold sweeping 0→1) that can be baked once and indexed at runtime? That last framing looks the most promising: a dissolve is a monotone family, so a single micromap storing the *threshold at which each sub-triangle flips* would serve every frame of the effect from one bake.

Note the API history is not encouraging: `VK_EXT_opacity_micromap` had a `DATA_UPDATE` build flag and discardable micromaps, and the 2026 `VK_KHR` redesign **removed both** ([[Graphics API and hardware support timeline]]). Whether that reflects hardware reality or lack of demand is worth understanding before building on it.

Related: [[Micromap]] · [[Prefiltered coverage from opacity hierarchies]] · [[Authoring, UV layout and micromap efficiency]]
