---
title: "Related Work — Application 3 — Tessellation-free closest-point queries"
tags: [related-work, application-3, closest-point, displacement-maps, tessellation-free]
status: working-note
created: 2026-09-07
updated: 2026-09-07
---

# Related Work — Application 3 — Tessellation-free closest-point queries

## Narrow finding

The literature located so far contains closest-point queries on triangle meshes, guaranteed closest-point queries on neural implicits, global optimization on polynomial patches, ray queries on tessellation-free displacement maps, and collision detection on hardware-tessellated displacement. It does **not** contain a located method that returns a certified global closest point directly on a conventional tessellation-free scalar displacement map using the map's native hierarchy. This is a focused search result, not proof that no such publication exists.

The useful paper position is therefore representation-specific:

> We derive a conservative world-space distance bound for a first-order scalar-displacement hierarchy and use it for certified global closest-point queries without displacement-resolution tessellation or a separate volumetric distance field.

The traversal itself is classical. Application 3 is strongest as the third interpretation of a shared representation: area sampling consumes a **measure bound**, ray tracing consumes an **intersection bound**, and closest point consumes a **distance bound**.

## Query contract

For a world-space point $x$ and the explicitly defined displaced surface $S$, the query returns a feasible surface point $y$, its base face and parameters, and an upper bound $U=\|x-y\|$. A hierarchy supplies lower bounds $L_n\leq \min_{z\in S_n}\|x-z\|$. Best-first branch-and-bound terminates when the best remaining lower bound proves the requested global error, for example $U-\min_n L_n\leq\varepsilon$. “Certified” should refer to this stated tolerance and represented surface, not to exact real arithmetic or uniqueness of the closest point.

## Exact overlap matrix

Here “scalar DM” means an ordinary sampled 2D scalar displacement over a coarse surface. “No fine tessellation” asks whether the published query operates without materializing displacement-resolution triangles. “Global CP” means that the method returns a globally closest surface point, not merely a collision, ray hit, or sampled distance value.

| Work | Scalar DM input | No fine tessellation in query | No auxiliary 3D distance/voxel field | Global CP output | Published guarantee/certificate | Uses native displacement hierarchy |
|---|---:|---:|---:|---:|---:|---:|
| Triangle BVH / FCPW | no | no | yes | yes | exact for the input triangle surface, modulo numerics | no |
| P2M | no | no | yes | yes | closest primitive on the input triangle mesh | no |
| Donnelly 2005 distance mapping | yes | yes | **no** | no | conservative ray-step intent, not a global CP certificate | no |
| Nießner et al. 2013 collision | yes | **no** | **no** | no | no global distance certificate | no |
| TFDM 2021 | yes | yes | yes | no | conservative displacement bounds for ray traversal | yes |
| RMIP 2023 | yes | yes | yes | no | conservative rectangular height bounds for ray traversal | yes |
| Spelunking 2022 | no: neural implicit | yes | yes | yes | concrete accuracy guarantees | no |
| Sum-of-Squares Geometry Processing 2021 | no: polynomial patches | yes | yes | yes | SOS lower bounds; exact-recovery certificate when obtained | no |
| SOS Collision 2023 | no: curved polynomial shapes/paths | yes | yes | no: collision/CCD | certified polynomial non-intersection/collision machinery | no |
| Huang 2025 | no: closed implicit surface | yes | yes | yes | interval branch-and-bound | no |
| Noma et al. 2026 | no: learned vector displacement | **no for downstream mesh processing** | yes | no | approximation evaluated against target geometry | no |
| Proposed Application 3 | yes | yes | yes | yes | lower/upper global $\varepsilon$ certificate | yes |

PWoS and modified PWoS are omitted from the matrix because they are consumers of a closest-point oracle rather than competing closest-point implementations.

## Closest point on explicit triangle meshes

Closest-point search over explicit triangles is mature. A BVH prunes a node when its bounding-volume distance cannot improve the current feasible point. [FCPW](https://github.com/rohan-sawhney/fcpw) is a practical CPU/GPU library for closest-point and ray queries on triangle and segment meshes. [P2M](https://doi.org/10.1145/3592439) instead uses a vertex k-d tree plus a precomputed interception table to recover the closest edge or face.

These works establish both the branch-and-bound pattern and strong materialized-mesh baselines. They do not query a scalar displacement map directly: the displaced surface must first be sampled into triangles, after which memory, construction, refit, edit, tiling, and instancing costs belong to the fine mesh. Application 3 must compare query time as well as those representation costs; it should not imply that ordinary nearest-BVH traversal is new.

## Displacement-specific rendering and collision

### Donnelly 2005: a nearby name, a different query

[Per-Pixel Displacement Mapping with Distance Functions](https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-8-pixel-displacement-mapping-distance-functions) precomputes a 3D texture around a height field. Each voxel stores a sampled distance to the displaced surface, and the shader uses it to take safe steps while marching a viewing ray. This is the closest terminological near-miss: it contains closest-surface distances, but it does not accept an arbitrary world-space point and return the corresponding surface point, parameters, and global lower/upper certificate. It also requires a separate volumetric distance-map bake rather than querying the native 2D displacement hierarchy.

### Nießner et al. 2013: displacement-aware proximity, but tessellated

[Real-time Collision Detection for Dynamic Hardware Tessellated Objects](https://doi.org/10.2312/conf/EG2013/short/033-036) detects overlap between patch-based displacement-mapped objects. It culls patches against the overlap of object bounds, hardware-tessellates candidate patches, voxelizes them into one-bit grids, and tests voxel overlap. An extended pass can report patch IDs and parameters near collision locations. It is direct prior art against any claim that displacement-aware collision or interaction is new. It does not return global closest distance, uses the generated tessellation and an auxiliary voxel grid, and even notes possible loss near the visual hull from non-conservative rasterization.

### TFDM and RMIP: native structures, ray-only outputs

[TFDM](https://doi.org/10.1145/3478513.3480535) decouples a min-max displacement acceleration structure from the coarse base mesh and composes bounds at query time for tessellation-free ray tracing. [RMIP](https://doi.org/10.1145/3610548.3618146) replaces square-only mip queries with conservative bounds over oblong texture-space regions and combines them with inverse displacement mapping. Both establish the compact, editable, tileable displacement-native architecture that this project extends. Their published query is ray-surface intersection, not arbitrary-point global closest point.

The obvious Application 3 baseline is consequently not only a dense triangle BVH. It is also the same best-first closest-point traversal driven by TFDM-style scalar min-max node enclosures. The first-order hierarchy earns its role only if its tilted world-space enclosure is conservative and its tighter pruning repays its arithmetic and storage.

## Guaranteed queries on other continuous representations

### Neural implicit range analysis

[Sharp and Jacobson 2022, *Spelunking the Deep*](https://doi.org/10.1145/3528223.3530155) applies interval and affine range analysis directly to neural implicit functions. It supports ray casting, intersection, hierarchy construction, mesh extraction, bulk properties, and closest-point evaluation with concrete accuracy guarantees. It establishes the methodological template “conservative range bounds unlock non-rendering queries on a compact continuous representation.” Our novelty cannot be branch-and-bound, guaranteed closest point in general, or redirecting rendering bounds toward geometry queries. The distinction is the structured scalar-displacement substrate, its plane-plus-residual hierarchy, composition with the coarse shell, and reuse across measure, visibility, and proximity.

### Polynomial patches and sum-of-squares optimization

[Marschner et al. 2021, *Sum-of-Squares Geometry Processing*](https://doi.org/10.1145/3478513.3480551) formulates closest point and other operations on higher-order polynomial surfaces as polynomial optimization. SOS relaxations provide global lower bounds and can produce exact-recovery certificates. [Zhang et al. 2023, *Sum-of-Squares Collision Detection for Curved Shapes and Paths*](https://doi.org/10.1145/3588432.3591507) accelerates this machinery for collision and continuous collision detection on curved shapes and paths without tessellation.

These papers block broad claims about the first tessellation-free closest point on curved geometry or the first certified collision query on continuous patches. They do not exploit a conventional sampled displacement texture or its mip hierarchy. A polynomial reconstruction of an individual displacement cell might make SOS a leaf solver or comparison point, but it does not supply the displacement-native coarse hierarchy, editing, tiling, or cross-application reuse claim.

## Closest point as a PDE service

[Sugimoto et al. 2024, *Projected Walk on Spheres*](https://doi.org/10.1145/3680528.3687599) solves surface PDEs pointwise by taking ambient-space Monte Carlo steps and projecting each step back to the nearest surface point. It assumes closest-point and normal queries and additionally estimates a valid tubular neighborhood/local feature size and distance to normal-extended Dirichlet boundaries. Thus a closest-point oracle is necessary but not sufficient for a PWoS application.

[Huang 2025, *Geometric Queries on Closed Implicit Surfaces for Walk on Stars*](https://doi.org/10.1145/3757376.3771378) formulates the geometric services needed by a WoS-family solver on closed implicit boundaries as constrained global optimization and solves them with interval branch-and-bound. It includes closest-point, closest-silhouette, and Robin-radius queries. This work blocks claims that certified branch-and-bound geometry services for mesh-free Monte Carlo PDE solvers are new; the remaining distinction is the surface-PDE setting and the displacement-native representation, with local-feature-size certification a separate potential contribution.

[Hui et al. 2026, *A modified projected Walk on Spheres method for elliptic equations on high-dimensional embedded manifolds*](https://arxiv.org/abs/2606.21883) adds a compensation term, adaptive radii, and mean-square error estimates while retaining closest-point projection as an input service. It reinforces the need to state separately the accuracy of the geometry oracle and the convergence/error claim of the downstream stochastic solver.

## Neural displacement fields

[Noma et al. 2026, *Mesh Processing Non-Meshes via Neural Displacement Fields*](https://doi.org/10.1111/cgf.70354) learns a compact vector-valued map from a coarse mesh to a target non-mesh surface. It enables geometry-processing workflows by efficiently extracting manifold/Delaunay meshes and by compressing precomputed scalar fields. The title makes it mandatory related work for the unified paper, but its representation and execution path differ: it is a learned vector displacement fitted to general input, and its downstream mesh analysis extracts a mesh rather than evaluating a certified global closest point through a native scalar-displacement hierarchy.

## What is established and what remains open

Established:

- closest-point traversal and bounding-volume pruning on explicit meshes;
- closest-point queries with guarantees on neural implicit functions;
- globally optimized closest points on polynomial patches;
- collision/CCD on curved polynomial geometry without tessellation;
- tessellation-free ray intersection on conventional scalar displacement maps;
- collision detection on displacement-mapped patches through hardware tessellation and voxelization;
- closest point as an oracle for mesh-free surface and boundary PDE solvers.

Not located in this search:

- a certified global closest-point result evaluated directly on a conventional sampled scalar displacement map;
- a conservative world-space distance lower bound that preserves a displacement node's first-order plane/residual correlation through the base-surface shell;
- a closest-point implementation that reuses the same displacement hierarchy used for area/measure and ray intersection;
- an evaluation isolating first-order versus scalar min-max distance pruning on the same displacement hierarchy and surface;
- the combination of closest-point query time with displacement-native memory, edit, tiling, and instancing costs.

## Claim ledger

Safe draft language, subject to a final full-text audit:

> We introduce a certified global closest-point query for conventional tessellation-free scalar displacement maps.

> To our knowledge, prior displacement-native hierarchies support ray intersection but not global closest-point queries; prior guaranteed closest-point methods target triangle meshes, neural implicits, or polynomial patches.

> Our contribution is the displacement-specific conservative distance bound and its reuse within a hierarchy that also supports measure and intersection queries; best-first branch-and-bound is standard.

Avoid:

- “the first closest-point query on a non-mesh, implicit, curved, or tessellation-free surface”;
- “the first guaranteed/certified closest-point query”;
- “the first use of conservative hierarchy bounds for closest point”;
- “the first tessellation-free collision method for curved geometry”;
- “the first geometry-processing method for displacement fields”;
- “no previous proximity work exists for displacement maps”;
- “closest point alone makes PWoS certified.”

Before submission, prefer “to our knowledge” over an unqualified “first,” and specify **conventional scalar displacement map**, **global closest point**, **represented surface**, **no displacement-resolution tessellation**, and **certificate tolerance** in the same paragraph.

## Bibliography checklist for the three-application draft

- [x] Rohan Sawhney. *FCPW: Fastest Closest Points in the West*. Software, 2021. [Repository](https://github.com/rohan-sawhney/fcpw).
- [x] Chen Zong et al. *P2M: A Fast Solver for Querying Distance from Point to Mesh Surface*. ACM TOG 42(4), 2023. [DOI 10.1145/3592439](https://doi.org/10.1145/3592439).
- [x] William Donnelly. *Per-Pixel Displacement Mapping with Distance Functions*. GPU Gems 2, Chapter 8, 2005. [NVIDIA chapter](https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-8-pixel-displacement-mapping-distance-functions).
- [x] Matthias Nießner, Christian Siegl, Henry Schäfer, and Charles Loop. *Real-time Collision Detection for Dynamic Hardware Tessellated Objects*. Eurographics Short Papers, 2013. [DOI 10.2312/conf/EG2013/short/033-036](https://doi.org/10.2312/conf/EG2013/short/033-036).
- [x] Théo Thonat et al. *Tessellation-Free Displacement Mapping for Ray Tracing*. ACM TOG 40(6), 2021. [DOI 10.1145/3478513.3480535](https://doi.org/10.1145/3478513.3480535).
- [x] Théo Thonat et al. *RMIP: Displacement Ray-Tracing via Inversion and Oblong Bounding*. SIGGRAPH Asia Conference Papers, 2023. [DOI 10.1145/3610548.3618146](https://doi.org/10.1145/3610548.3618146).
- [x] Nicholas Sharp and Alec Jacobson. *Spelunking the Deep: Guaranteed Queries on General Neural Implicit Surfaces via Range Analysis*. ACM TOG 41(4), 2022. [DOI 10.1145/3528223.3530155](https://doi.org/10.1145/3528223.3530155).
- [x] Zoë Marschner et al. *Sum-of-Squares Geometry Processing*. ACM TOG 40(6), 2021. [DOI 10.1145/3478513.3480551](https://doi.org/10.1145/3478513.3480551).
- [x] Paul Zhang et al. *Sum-of-Squares Collision Detection for Curved Shapes and Paths*. SIGGRAPH Conference Papers, 2023. [DOI 10.1145/3588432.3591507](https://doi.org/10.1145/3588432.3591507).
- [x] Ryusuke Sugimoto et al. *Projected Walk on Spheres: A Monte Carlo Closest Point Method for Surface PDEs*. SIGGRAPH Asia Conference Papers, 2024. [DOI 10.1145/3680528.3687599](https://doi.org/10.1145/3680528.3687599).
- [x] Tianyu Huang. *Geometric Queries on Closed Implicit Surfaces for Walk on Stars*. SIGGRAPH Asia Technical Communications, 2025. [DOI 10.1145/3757376.3771378](https://doi.org/10.1145/3757376.3771378).
- [x] Yuta Noma et al. *Mesh Processing Non-Meshes via Neural Displacement Fields*. Computer Graphics Forum 45(2), 2026. [DOI 10.1111/cgf.70354](https://doi.org/10.1111/cgf.70354).
- [x] Zhiyuan Hui et al. *A modified projected Walk on Spheres method for elliptic equations on high-dimensional embedded manifolds: algorithm and error estimates*. arXiv:2606.21883, 2026. [Preprint](https://arxiv.org/abs/2606.21883).
- [ ] Re-run focused title/abstract/full-text searches immediately before submission, including citations to and from TFDM, RMIP, Spelunking, SOS Geometry Processing, Nießner et al., and Noma et al.
- [ ] Confirm the final surface contract and certificate definition before converting this note into paper prose; related-work distinctions depend on what the leaf reconstruction represents.

Related: [[Project — Conservative metric queries without tessellation]] · [[Proximity and contact queries against micro-geometry]] · [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] · [[Thonat et al. 2023 — RMIP]] · [[Sugimoto et al. 2024 — Projected Walk on Spheres]]
