---
title: Non-ray queries — slicing, volume and mass properties
tags: [open-question, research-direction, manufacturing, geometry]
priority: 5
---

# Non-ray queries — slicing, volume and mass properties

> [!note] Analysis, not a claim from any paper. See [[MOC — Open Questions]].

> [!success] Outside check — 2026-08-10 — **still open, and now with a named victim**
> The only direction of the three that came out of the literature check *stronger*. Direct slicing without tessellation is mature for CSG, implicits and NURBS (Lefebvre's IceSL, the AM literature) — so "nobody slices without tessellating" is false in general, **but nobody does it for base mesh + tangent-space displacement**. And the manufacturing side has now published the exact tessellation blow-up this note predicts, with a number attached. See [[#Prior art outside this vault]].
> This direction rested on absence, which was its weakness. It no longer does. **Recommend re-ranking above [[Surface measure and sampling on implicit displaced surfaces|direction 2]].**

Additive manufacturing does not cast rays. It intersects geometry with a **plane**, thousands of times, once per layer. Nobody in this vault has sliced a displaced surface without tessellating it first — and tessellating defeats the entire purpose of the representation for the exact assets (scanned, multi-million-triangle) where it helps most.

The related unanswered quantities: **enclosed volume**, **centre of mass**, and the **inertia tensor** of a displaced solid. These drive material cost estimation, print-time prediction, buoyancy and rigid-body dynamics.

**Why it should work.** A plane–surface query is a *lower-dimensional* problem than a ray query, not a harder one: the same [[Min-max mipmap and conservative bounds|min-max bounds]] cull texel regions the plane cannot reach, and the output is a curve in texture space rather than a point — structurally similar to the interval curve [[Thonat et al. 2023 — RMIP|RMIP]] already tracks, and to the projected-ray quadric it already solves. Volume follows from the divergence theorem over the [[Shell, prism and prismoid|prismoid]] decomposition; [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] already note a prismoid's volume is a sum of three tetrahedra, but use it only as an optimisation target, never evaluate it as a physical quantity.

**What it unlocks.** Printing scanned assets directly from a compressed representation; CAD/CAM on micro-detailed surfaces; and slice-based fabrication generally. Also engineering analysis — a displaced [[Shell, prism and prismoid|shell]] is a natural prismatic boundary-layer mesh for CFD, a connection nobody here has made.

## Prior art outside this vault

Checked 2026-08-10. Absent from all fifteen bibliographies here.

**The named victim — the most useful single find for this direction.** *Displacement Mapping as a Highly Flexible Surface Texturing Tool for Additively Photopolymerized Components* (Micromachines 15(5), 2024) uses displacement mapping to author 3D-printed surface texture, and reports that the mesh had to be refined to **10% of the smallest feature size**, with the textured layer heavily overexposed, to hit the target accuracy. That is precisely the tessellation blow-up predicted above, measured and published by the manufacturing side. It converts this note's argument from *rests on absence* — its stated weakness in [[MOC — Open Questions]] — into a documented cost. Cite it first.

**Direct slicing already exists, for other representations.** Sylvain Lefebvre's **IceSL** is a GPU-accelerated modeller/slicer that slices a CSG tree over meshes, voxels, implicit surfaces and shaders **straight to G-code with no intermediate STL**. The CAD/AM literature adds direct slicing of implicits, of NURBS, and of dilated/eroded models; slicing an analytic primitive yields analytic contours with no loss of precision. So the *concept* is established and shipped — what is missing is doing it for a **base mesh plus tangent-space displacement plus compressed micro-geometry**, where the surface is neither a global implicit nor a CSG tree. Frame the contribution as the representation, never as the idea.

**Machinery for the volume half that this note does not currently cite.**

- **Sharp & Jacobson 2022**, *Spelunking the Deep* (SIGGRAPH) — range analysis over general implicits, listing **bulk properties** among its supported queries alongside closest point and ray casting. The volume/mass-properties half in generic form. *Unverified: exactly which bulk properties (volume, mass, area, centre of mass) and by what construction — the PDF resisted extraction. Check this directly before writing anything that claims volume on implicits is unaddressed.*
- **Generalized winding numbers** — Jacobson, Kavan & Sorkine 2013; Barill et al. 2018 (fast winding numbers for soups and clouds); *One-Shot Method for Computing Generalized Winding Numbers* 2024. This is the robust inside/outside machinery that makes volume well-posed on imperfect solids, and it speaks directly to the watertightness caveat below. Currently uncited here and it should not be.
- **Sellán, Aigerman & Jacobson 2021**, *Swept Volumes via Spacetime Numerical Continuation* (SIGGRAPH) — implicit in, implicit out, via 4D spacetime continuation. Relevant here, and to step 4 of [[Proximity and contact queries against micro-geometry]].
- Classical polyhedral mass properties (Mirtich 1996) remains the ground truth to validate against, alongside the prismoid-as-three-tetrahedra decomposition [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]] already note.
- The **curved / high-order meshing** literature — Bézier guarding and its 3D tetrahedral successor, curvilinear CFD mesh generation, exact-conservation high-order methods — is the field the "shell as prismatic boundary layer" claim would land in. Large, active, and entirely unengaged by this vault. Read before repeating that nobody has made the connection.

**Risks.** Needs a closed, watertight, non-self-intersecting solid to be well-posed — so it inherits every caveat in [[Watertightness and cracks]], and self-intersecting displacement makes volume ambiguous; generalized winding numbers are the standard answer and should be tried before inventing one. Manufacturing tolerances are absolute, not perceptual, so conservative bounds must be provably one-sided. And the fabrication community solves its own problems: the risk is not that this is taken, but that it is taken *elsewhere*, in a venue this vault does not read.

Related: [[Proximity and contact queries against micro-geometry]] · [[Surface measure and sampling on implicit displaced surfaces]]
