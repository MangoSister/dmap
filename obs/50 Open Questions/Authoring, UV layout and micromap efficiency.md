---
title: Authoring, UV layout and micromap efficiency
tags: [open-question, research-direction, authoring, parameterization]
priority: 9
rests-on: "Waldemarson & Doggett §6.3 — the entire literature on this"
---

# Authoring, UV layout and micromap efficiency

> [!note] Analysis, not a claim from any paper. See [[MOC — Open Questions]].

The complete published treatment of how to *author* for micromaps is **one section of one paper** — [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] §6.3. Its central observation deserves more attention than it got:

> micromaps are *not* just a specialised image texture — they are tied to both the texture **and** the (u,v) coordinates.

The consequence is severe and counter-intuitive. Overlapping or misaligned UVs make each triangle produce a **unique** micromap despite sharing a texture, so the deduplication that makes micromaps affordable silently fails, and acceleration-structure bandwidth *increases*. An asset can be made dramatically better or worse for ray tracing by a parameterisation decision made years earlier for unrelated reasons.

**The obvious missing tool: a UV parameterisation that optimises for micromap deduplication.** This is the direct analogue of what [[Maggiordomo et al. 2023 — Micro-Mesh Construction|Maggiordomo et al.]] did for displacement — their displacement-aware ARAP reformulates distortion energy over displaced micro-triangles and recovers 22–40% of the distortion displacement introduces. Nobody has done the equivalent for opacity, where the objective is not distortion but **how many triangles can share one micromap**. That objective is combinatorial rather than continuous, which is what makes it interesting.

Two more unstudied practical problems:

**Automatic budget allocation.** *Indiana Jones* hand-capped subdivision at level 6 to fit 128 MB, with a 0.5 MB per-OMM cap. [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] give a per-triangle criterion (sub-triangle edge below a pixel), but nobody solves the *global* allocation problem: given a memory budget and a scene, which assets deserve which levels? A principled answer needs a per-asset marginal-benefit curve, which nobody measures.

**Cross-LoD sharing.** [[Maggiordomo et al. 2023 — Micro-Mesh Construction|Maggiordomo et al.]] explicitly flag as open "how to share textures or UV-maps across different levels of detail", given that µ-meshes are intrinsically multi-resolution.

**Why this matters more than it looks.** These are the problems that decide whether the technology gets adopted, and they are invisible to the metrics the field publishes. Nothing in this vault reports authoring time, artist iteration count, or how often a bake produces a bad result — only milliseconds and megabytes. A paper that measured *those* would be unusual and useful.

Related: [[Micromap]] · [[Base mesh quality objectives]] · [[Dynamic opacity micromaps]]
