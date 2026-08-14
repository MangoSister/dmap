---
title: Inverse displacement mapping
authors: [John W. Patterson, S. G. Hoggar, J. R. Logie]
year: 1991
venue: Computer Graphics Forum 10(2)
tags: [reference, not-in-vault, displacement, inversion]
---

# Patterson et al. 1991 — Inverse Displacement Mapping

**Not in vault** — summarised from citations in [[Thonat et al. 2023 — RMIP]]. Not read directly.

The conceptual root of [[Thonat et al. 2023 — RMIP|RMIP]]: rather than pushing the surface out to meet the ray, **project the ray back into the displacement map's parameter domain** and search there. Limited to analytical base surfaces (extended by Logie & Patterson 1995); RMIP's contribution is carrying it to triangle meshes with interpolated displacement directions.

RMIP keeps the idea but replaces the splitting rule: Patterson projected the **3D interval midpoint** into texture space, which with range-arithmetic bounds lands anywhere along the projected curve and gives unbalanced splits. RMIP splits the **2D domain** directly along the longer side, which culls half the bound area and bounds the number of traversal steps.
