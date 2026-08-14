---
title: Proximity and contact queries against micro-geometry
tags: [open-question, research-direction, physics, collision, priority]
priority: 1
rests-on: "RMIP's stated future work; the absence of any non-ray query in the vault"
---

# Proximity and contact queries against micro-geometry

> [!note] This is analysis, not a claim from any paper
> Written from the gaps across this vault. See [[MOC — Open Questions]] for how these were derived and what has *not* been checked.

> [!success] Confirmed open — 2026-08-10
> Checked against all three 2026 papers ([[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|foliage]], [[Zhang et al. 2026 — DJM|DJM]], [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry|animated geometry]]). **None performs any non-ray query.** The words "collision", "distance field", "closest point", "proximity" and "contact" appear nowhere in the foliage paper; its wind is a one-way stateless field with no readback, and it describes interactive/stateful foliage only as something *rasterisation* practitioners do. DJM uses closest-point iteration purely as a construction subroutine. The tetrahedral-cage paper cites collision-detection work only as background for why cages exist.
> Still the strongest lane in this folder.

> [!warning] Outside check — 2026-08-10 — **narrowed, not closed**
> Searched geometry processing and simulation (SIGGRAPH/Asia, ToG, EGSR, EG, SGP, SCA, HPG). **Three direct precedents exist**, none of them anywhere in this vault's citation graph. The *query* is no longer novel; the *representation* still is. See [[#Prior art outside this vault]] before writing any framing that claims nobody has done contact against displaced geometry — that claim is false.

**The gap.** Every structure in this vault indexes sub-triangle detail, and every paper uses it to answer one question: *what does this ray hit?* Nobody **in this literature** asks *where is the nearest surface point*, *how deep is this penetration*, or *what is the contact normal*. Geometry processing asks exactly these questions — of other representations.

## Evidence it is open

[[Thonat et al. 2023 — RMIP|RMIP]] flags it and drops it in a single closing sentence:

> RMIP could be instrumental in a number of other level-of-detail problems, including **approximation models for physics simulation** and high-resolution geo mapping.

No follow-up exists in any paper here. [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]] is the only paper that even touches interaction, and only as sculpting brush edits — a texture-space operation, not a spatial query.

## Prior art outside this vault

Checked 2026-08-10. Absent from all fifteen bibliographies here, and load-bearing for how this direction must now be pitched.

**Direct precedents — these do the thing, or its methodological core.**

- **Nießner, Siegl, Schäfer & Loop 2013**, *Real-time Collision Detection for Dynamic Hardware Tessellated Objects* (Eurographics Short Papers). Collision detection against **patch-based, displacement-mapped objects**, entirely on GPU: OBB overlap → patch inclusion test → 1-bit voxelisation of candidate patches → collision, with GPU-side offset updates giving fine-scale impact deformation. Sub-millisecond on models with thousands of patches. It **tessellates** via the hardware tessellator and reduces contact to binary voxel occupancy — no min-max hierarchy, no closest point, no signed distance, no inversion. The proposal below is still distinct. The *motivating claim* is not.
- **Sharp & Jacobson 2022**, *Spelunking the Deep: Guaranteed Queries on General Neural Implicit Surfaces via Range Analysis* (SIGGRAPH, ToG 41(4)). Range analysis — with affine-arithmetic variants they identify as most effective — over general implicit functions, yielding ray casting, intersection testing, spatial-hierarchy construction, mesh extraction, **closest-point evaluation** and bulk properties, with concrete accuracy guarantees. This is precisely the argument made below — *point the conservative-bounding machinery at non-ray queries* — already published, on a different substrate. The idea is established; only the substrate is new.
- **Zhang, Marschner, Solomon & Tamstorf 2023**, *Sum-of-Squares Collision Detection for Curved Shapes and Paths* (SIGGRAPH), building on **Marschner, Zhang, Palmer & Solomon 2021**, *Sum-of-Squares Geometry Processing* (SIGGRAPH Asia, ToG 40(6)). Exact collision detection on Bézier triangles **without tessellating**, up to 300× faster than prior SOSP, plus an algebraic rigid-motion formulation giving curved geometry *and* curved trajectories simultaneously — i.e. continuous collision detection. The 2021 paper covers closest-point and distance queries on higher-order surfaces. This is a live competing route to every item in *Concrete first steps* below, step 4 included, from Solomon's group.

**Adjacent work that erodes the supporting arguments.**

- **Otaduy & Lin**, *Sensation Preserving Simplification for Haptic Rendering* (SIGGRAPH 2003) and *CLODs: Dual Hierarchies for Multiresolution Collision Detection* (SGP 2003), plus the 6-DoF haptic line. Haptic textures are built from measured **height-displacement profiles** and forces rendered from them. Penalty-based force model, not a geometric query on a displaced surface — but haptics as an untouched downstream application is two decades stale.
- **Macklin, Erleben, Müller, Chentanez, Jeschke & Corse 2020**, *Local Optimization for Robust Signed Distance Field Collision* (PACMCGIT 3(1)). Per-element local optimisation for closest points between an SDF isosurface and mesh elements, giving accurate point-face and edge-edge contact. This is the baseline any displacement-native method must beat, and it is strong.
- ***Coupling Friction with Visual Appearance*** (PACMCGIT 2021, from the Andrews/Erleben contact group). Derives anisotropic, asymmetric friction coefficients from surface facet orientations, GPU-rasterised per contact point at the scale where roughness is also a visual feature. Micro-geometry already feeds simulation — as friction *parameters*, not contact geometry.
- ***Contact detection between curved fibres: high order makes a difference*** (ToG 2024). Shows low-order collision proxies produce **spurious contact forces** relative to high-order geometry. This is the argument this note wants to make, already made empirically for fibres — the best available supporting citation, and evidence that someone is circling the same thesis from the simulation side.
- The **IPC** family (Li et al. 2020; C-IPC 2021) and the SIGGRAPH *Contact and Friction Simulation* course are mesh-based throughout: consumers of such a query, not competitors.

**What survives.** Tessellation-free signed distance and penetration depth on a displacement-mapped surface, via branch-and-bound over the min-max hierarchy plus [[Thonat et al. 2023 — RMIP|RMIP]]'s inversion for sidedness, with temporally warm-started contact caching. Nobody does that. Everything found is either **representation-agnostic** (Spelunking, SOS) or **representation-specific but tessellating** (Nießner et al.). Nobody exploits *this* structure — min-max mipmap, prism, inversion, micromap compression — for a non-ray query. That is the claim that survives, and it is narrower than the one this note originally made.

## Why it matters beyond rendering

Production today ships **two** geometries: micro-detailed geometry for rendering, and a hand-authored low-poly proxy for collision. That split costs artist time, causes visual/physical mismatch (characters floating above visibly displaced ground), and scales badly as render geometry gets denser. No *practical, tessellation-free* method does contact against micro-geometry — Nießner et al. 2013 tessellates and voxelises, SDF collision needs a separate baked field — so the split remains forced in practice, though not for want of anyone trying.

Downstream, the same primitive is what robotics grasp planning, CAD interference checking and cloth-on-surface simulation need. Haptics needs it too, but has been served approximately since Otaduy & Lin 2003 — cite them rather than claiming the application is open.

## The technical shape of it

Most of the machinery already exists and is pointed the wrong way.

**Branch-and-bound over the implicit hierarchy.** [[Min-max mipmap and conservative bounds|Min-max bounds]] give a conservative box per texel region; distance from a query point to that box *lower-bounds* distance to the enclosed surface. That is a textbook closest-point BVH search — except the hierarchy is implicit and generated per query, exactly as in [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]] traversal. Substituting a distance-to-box priority queue for a ray-interval test is a small change to a solved traversal. **Sharp & Jacobson 2022 already did this on neural implicits** — so this step is now a port with a citation, not a contribution. Budget the novelty elsewhere.

**Inversion gives sidedness almost for free.** [[Thonat et al. 2023 — RMIP|RMIP]]'s Newton inversion maps a 3D point to the `(u,v)` whose displacement line passes through it. Once you have that, the *signed* distance along the displacement direction is `h_query − h(u,v)` — so signed distance, and hence penetration depth and a contact normal, fall out of a primitive that already exists and whose convergence is already guaranteed inside a [[Shell, prism and prismoid|prism]].

**The workload shape argues in its favour.** Contact queries are far more spatially coherent than incoherent secondary rays, and they exhibit strong frame-to-frame temporal coherence — a contact point moves a little per step. Caching the last resolved `(u,v)` per contact and warm-starting the Newton iteration should make the amortised cost much lower than a ray query. That is the opposite of the usual "displacement is expensive" story.

**Watertightness suddenly becomes a feature.** For rendering, [[Watertightness and cracks|near-watertightness]] is a quality issue. For collision it is a correctness issue — leaks mean objects tunnel through. [[Maggiordomo et al. 2023 — Micro-Mesh Construction|µ-meshes]] are **bit-exact watertight by construction** under uniform LoD reduction, which arguably makes them a *better* collision representation than an arbitrary triangle mesh, not a worse one. This is an argument nobody in the vault has made.

## Concrete first steps

1. Closest-point and unsigned distance to a displaced surface, by branch-and-bound over the min-max hierarchy. Validate against brute-force pre-tessellated ground truth.
2. Signed distance and penetration depth via inversion; characterise where the sign is ill-defined (overhangs, self-intersecting displacement, [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping|Ogaki's]] degenerate cases).
3. Warm-started, temporally coherent contact caching — the result that would decide whether this is practical.
4. Swept / continuous collision detection. Hardest, and the one that matters most for fast-moving bodies.

## Risks

- **Conservativeness vs accuracy.** Physics wants guaranteed no-tunnelling; a conservative bound that reports contact slightly early is usually acceptable, and slightly late is not. The existing bounds are conservative in the right direction, but this needs proving rather than assuming.
- **Overhangs break the height-field sign.** Scalar displacement over a base triangle is genus-zero ([[Barczak et al. 2024 — DGF|DGF]], [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes|Gruen et al.]]), so signed distance is only well-defined where the surface really is a height field over the base. Cluster-based representations do not have this structure at all.
- ~~**Concurrent work unchecked.**~~ **Checked 2026-08-10 — and it was there.** Displacement-aware collision does exist (Nießner et al. 2013), conservative-bounds-to-closest-point does exist (Sharp & Jacobson 2022), and tessellation-free contact on curved primitives does exist (Zhang et al. 2023). None uses this representation, but the framing risk is now the dominant risk on this direction: a reviewer who knows geometry processing will find all three immediately. Lead with the representation, never with the query.
- **Solomon's group is the one to watch.** The SOS line has both the queries and curved-trajectory CCD, and is actively extending. Displacement maps are a natural next substrate for it.
- **Step 3 is the whole contribution.** With the bounding argument conceded to Spelunking and the query conceded to SOS, warm-started temporal contact caching — the claim that this is *cheaper* than a ray query rather than merely possible — is what would make the paper. Design the evaluation around it from the start.

Related: [[Non-ray queries — slicing, volume and mass properties]] · [[Hardware ray tracing and micro-geometry]] · [[Shell, prism and prismoid]]
