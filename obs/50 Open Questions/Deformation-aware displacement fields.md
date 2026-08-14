---
title: Deformation-aware displacement fields
tags: [open-question, research-direction, animation, deformation]
priority: 6
rests-on: "Gruen et al. 2024 and NeuBase, independently, same structural failure"
---

# Deformation-aware displacement fields

> [!note] Analysis, not a claim from any paper. See [[MOC — Open Questions]].

> [!success] Confirmed open — 2026-08-10, and now a third paper hits the same wall
> [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry|Gruen et al. 2026]] is architecturally the *strongest* version of the evasion: fine geometry is provably static in tetrahedron-local space, and sub-cage motion — *"cloth or facial animation with folds smaller than the cage resolution"* — is listed as an explicit non-use-case. [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|van Antwerpen et al.]] do the same with rigid bone motion and discard within-chunk deformation, bounding it as error. [[Zhang et al. 2026 — DJM|DJM]] has no time dimension at all.
>
> **Smith et al. 2000 is cited by neither.** Four papers now hold detail static and move the animation elsewhere; nobody has asked whether the offset field should itself be a function of deformation.

Two papers in this vault, from unrelated communities, hit the **same wall** — which is good evidence it is structural rather than incidental.

[[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al.]]: DMM displacements are baked in a fixed rigging pose and cannot change amplitude or sign, so a snake that switches from curving outward to inward is **inexpressible**. Their fix moves the animation into a per-micro-vertex interpolated matrix, leaving the displacements untouched.

[[Mendiratta et al. 2026 — NeuBase|NeuBase]]: the neural offset field cannot be subdivided or refined without retraining, because it uses learned rather than Catmull–Clark bases. Their fix keeps the offset fixed and moves editing into the control mesh.

Both are the same problem: **a precomputed offset field bound to a base configuration it no longer controls.** Both answers are the same evasion — hold the offset still and vary something else. Neither asks whether the offset field itself should be a function of the deformation.

**The open question.** What is the right representation for a displacement field that *co-deforms* — where fine detail responds to how the base bends? Wrinkles deepen when skin compresses; bark cracks open when a branch flexes; knit loops flatten under tension. All of this is currently faked with blend shapes on the dense mesh, which is exactly what a compressed representation was supposed to avoid.

Plausible formulations: displacement as a function of a local strain or curvature invariant of the base; a small basis of pose-dependent displacement maps blended by deformation state; or making the offset field differentiable with respect to base deformation, which [[Dou et al. 2024 — Differentiable Micro-Mesh Construction|Dou et al.]]'s machinery is already close to supporting.

**Beyond rendering.** Anatomical and surgical simulation (tissue detail under deformation), garment simulation at yarn scale, and materials modelling of textured surfaces under load — all want detail that responds to strain rather than riding along rigidly.

**Risks.** Anything pose-dependent trades away the compression that motivated the format. The honest version of this research has to report the storage cost per unit of expressiveness gained, and may conclude the trade is bad. [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al.]] also leave **watertightness under non-uniform subdivision** unsolved, which any adaptive deformation scheme would immediately run into.

Related: [[Base mesh quality objectives]] · [[Watertightness and cracks]] · [[Inverse and differentiable micro-geometry]]
