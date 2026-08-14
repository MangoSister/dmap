---
title: Prior art index
tags: [reference, index, bibliography, gap-check]
created: 2026-08-10
purpose: "Coverage map of what the twelve vault papers cite — for checking whether an open question is genuinely open"
---

# Prior art index

What the twelve papers in this vault **collectively already survey**, organised by territory rather than by paper. Its job is to answer one question quickly: *when I claim direction X is underexplored, what did this literature already look at nearby?*

> [!warning] Scope and limits — read before relying on this
> - **Updated 2026-08-10** with three further papers ([[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|foliage]], [[Zhang et al. 2026 — DJM|DJM]], [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry|animated geometry]]), taking the vault to fifteen. Their additions are listed under **The 2026 additions** below; the territory sections above them reflect the original twelve.
> - These are the **most-cited and most load-bearing references** from each paper's bibliography, not complete reference lists. A work's absence here means it was not prominent, not that it was never cited.
> - Coverage is as of each paper's publication date: 2020 (Gruen) through 2026 (Chernaik, NeuBase).
> - This maps what *these twelve papers* know about. It says nothing about literatures they never engaged — which is precisely what the [[MOC — Open Questions|open questions]] depend on, so see **Blind spots** below.
> - **2026-08-10: that limit has been partly lifted.** Directions 1, 2 and 5 were checked against geometry processing and simulation directly — see **The outside check** below. Do not cite a "nothing" verdict in the table without reading it; two of those verdicts are now wrong about the world, and one direction lost its headline result.

## Coverage map

### Displacement foundations
Cook 1984 (shade trees, the origin) · Blinn 1978 (bump mapping) · [[Smits et al. 2000 — Direct Ray Tracing of Displacement Mapped Triangles|Smits, Shirley & Stark 2000]] · [[Patterson et al. 1991 — Inverse Displacement Mapping|Patterson, Hoggar & Logie 1991]] + Logie & Patterson 1995 · Lee, Moreton & Hoppe 2000 (displaced subdivision surfaces) · Guskov et al. 2000 (normal meshes) · Szirmay-Kalos & Umenhoffer 2008 (STAR).
*Cited across the whole displacement cluster. Well-trodden; assume any purely-rendering displacement idea has been considered here.*

### Shell mapping and prisms
[[Porumbescu et al. 2005 — Shell Maps|Porumbescu et al. 2005]] · [[Jeschke et al. 2007 — Smooth and Curved Shell Mapping|Jeschke, Mantler & Wimmer 2007]] · Ritsche 2006 · Ye et al. 2007 (low-distortion shell maps) · Dachsbacher & Tatarchuk 2007 (prism POM) · Zirr & Ritschel 2019 (distortion-free displacement).
*Surveyed by Ogaki, PDM, RMIP, TFDM. [[Shell, prism and prismoid]].*

### Height-field and relief rendering
Policarpo et al. 2005 (relief mapping) · Oliveira, Bishop & McAllister 2000 · [[Tevs et al. 2008 — Maximum Mipmaps|Tevs et al. 2008]] · Oh, Ki & Lee 2006 (pyramidal displacement) · Lee, Tseng & Tai 2009 · Kaneko et al. 2001 (parallax) · Donnelly 2005 · Dummer 2006 (cone step) · Policarpo & Oliveira 2007 (relaxed cone) · Baboud et al. 2011 · Hart 1996 (sphere tracing) · Wang et al. 2003/2004 (view-dependent displacement, generalized displacement maps).
*Thoroughly covered. The rasterisation-era answers are all known to this literature.*

### Bounding: interval and affine arithmetic
de Figueiredo & Stolfi 2004 · Heidrich & Seidel 1998 · Knoll et al. 2009 · Munkberg et al. 2010 (bounding displaced Bézier patches, OBBs) · Snyder & Barr 1987 · Paiva et al. 2012.
*The bounding toolkit is well explored. [[Min-max mipmap and conservative bounds]].*

### Patch and curve intersection
[[Reshetov 2019 — Cool Patches|Reshetov 2019]] · Reshetov 2022 (ray/ribbon) · Ramsey, Potter & Hansen 2004 · Martin et al. 2000 (trimmed NURBS) · Yuksel 2022 (polynomial root finding) · Vlachos et al. 2001 (PN triangles) · Boubekeur & Alexa 2008 (Phong tessellation) · Loop et al. 2009 · Farin 2002 (CAGD, blossoming).

### Hardware tessellation
Nießner & Loop 2013 (analytic displacement) · Nießner et al. 2016 · Microsoft 2009 (DX11) · Haydel, Yuksel & Seiler 2023 (locally-adaptive LOD) · Reich et al. 2015.

### Simplification, remeshing, base-mesh construction
Garland & Heckbert 1997 (QEM) · Hoppe 1993/1996/1999 · Botsch & Kobbelt 2004 · Field 2000 (aspect ratio) · Cignoni et al. 2008 (MeshLab) · Microsoft 2022 (Simplygon) · Jiang et al. 2020 (bijective shells) · Welzl 1991 · Puppo 1998 · Dey et al. 1999 · Alliez et al. 2003 · Liu et al. 2009 (CVT) · Liu et al. 2012 (half-edge collapse) · DeCoro & Rusinkiewicz 2005 (pose-independent simplification).
*Dense coverage — [[Base mesh quality objectives]] sits on top of a mature literature.*

### Parameterisation and texture mapping
Liu et al. 2008 (ARAP) · Rabinovich et al. 2017 (SLIM) · Huang et al. 2018 (QuadriFlow) · Yuksel, Keyser & House 2010 (Mesh Colors) · Yuksel, Lefebvre & Tarini 2019 (Rethinking Texture Mapping) · Sederberg et al. 2003 (T-splines) · Liu et al. 2017 (seam erasure) · Maggiordomo et al. 2020 (Real-World Textured Things).
*Relevant to [[Authoring, UV layout and micromap efficiency]] — but note all of it is about **distortion**, none about micromap deduplication.*

### Geometry compression
Deering 1995 · Meyer 2012 · Rossignac 1999 (EdgeBreaker) · Google 2017 (Draco) · Kuth et al. 2024 (meshlet compression) · Segovia & Ernst 2010 · Lee, Choe & Lee 2010 · Jie et al. 2011 · Nystad et al. 2012 (ASTC) · Waveren 2006 (DXT) · Lier et al. 2018 · Selgrad et al. 2016 · Benthin et al. 2021 (lossy grid primitives) · Evans et al. 1996 / Stewart 2001 / Kapoulkine 2024 (strips).

### Clusters, LOD, virtualised geometry
[[Karis et al. 2021 — Nanite|Karis, Stubbe & Wihlidal 2021]] + Karis 2022 · Benthin & Peters 2023 (micro-poly hierarchical LOD) · Djeu et al. 2011 (Razor) · Christensen et al. 2003 · Hanika et al. 2010 · Hunt et al. 2007 · Kushwaha et al. 2024 (`VK_NV_cluster_acceleration_structure`).

### BVH construction and traversal
MacDonald & Booth 1990 · Goldsmith & Salmon 1987 · Wald et al. 2008 (binned SAH) · Wald et al. 2014 (Embree) · Meister et al. 2021 (survey) · Woop et al. 2013 (watertight ray/triangle) · Ize 2013 · Karras & Aila 2013 (TRBVH) · Domingues & Pedrini 2015 · Parker et al. 2010 (OptiX) · Williams et al. 2005 · Möller & Trumbore 1997 · Woop et al. 2017 (STBVH) · Lee et al. 2019 (traversal shaders) · Ylitie et al. 2017 / Liktor & Vaidyanathan 2016 (compressed wide BVH) · Vitsas et al. 2023 (OBBs).

### Alpha, transparency, stochastic methods
Wyman & McGuire 2017 (hashed alpha testing) · Enderton et al. 2010 (stochastic transparency) · McGuire & Mara 2017 (phenomenological transparency) · Persson 2012 · Lefebvre & Hoppe 2007 · Barré-Brisebois 2019 (texture LOD for RT).
*Relevant to [[Prefiltered coverage from opacity hierarchies]] — this is the closest existing territory, and it is **cited but not built on**.*

### Occlusion culling and prefiltering
Hasselgren & Akenine-Möller 2007 (PCU) · Hey et al. 2001 · Chen & Lee 2002 · Poon & Wang 1999 (opacity map) · Greene et al. 1993 (hierarchical Z) · Shopf et al. 2008 · Munkberg et al. 2016 (texture-space caching, conservative coverage) · **Lacewell et al. 2008 (prefiltered occlusion for aggregate geometry)** · Pharr & Fernando 2005.
*Lacewell is the single most important entry for direction 3 — it is the directional prefiltered-occlusion precedent Chernaik et al. explicitly simplify away from.*

### Succinct and sparse hierarchical structures
Jacobson 1989 · Gog et al. 2014 · Kämpe et al. 2013 (SVDAG) + 2016 · Webber & Dillencourt 1989 (CSM quadtrees) · Villanueva et al. 2016/2017 (symmetry-aware DAGs) · Molenaar & Eisemann 2025 · Assarsson et al. 2018 · van der Laan et al. 2020 (lossy voxel compression) · Scandolo et al. 2016/2021 · Beers et al. 1996 (rendering from compressed textures) · Huffman 1952 · Kaufmann & Moore 2004 (ACL2).

### Range queries
Amir, Fischer & Lewenstein 2007 (2D RMQ) · Fischer & Heun 2011 · Brodal et al. 2012 · Yuan & Atallah 2010 · Wang et al. 2020 (example-based microstructure, constant storage).

### Differentiable rendering and neural representations
Nicolet, Jacobson & Jakob 2021 (large steps — the load-bearing one) · Laine et al. 2020 (nvdiffrast) · Kingma & Ba 2015 · Bengio et al. 2009 (curriculum) · Ramamoorthi & Hanrahan 2001 · Kuznetsov et al. 2021/2022 (NeuMIP, neural materials on curved surfaces) · Takikawa et al. 2021 (NGLOD) · Pentapati et al. 2025 (QNDF) · Edavamadathil Sivaram et al. 2024 (NGF) · Yang et al. 2025 (NeuPPS) · Morreale et al. 2021/2022 · Mildenhall et al. 2021 · Gropp et al. 2020 · Vaidyanathan et al. 2023 (neural texture compression).

### Subdivision surfaces, skinning, deformation
Catmull & Clark 1978 · Stam 1998 · Loop 1987 · Zorin et al. 1997 · Kobbelt et al. 1998 · Kavan et al. 2007 (dual quaternion skinning) · Gruenvogel 2024 (LBS) · **Smith et al. 2000 (layered animation using displacement maps)** · Kiyavash Kandar 2024 (GPU skinning, *Alan Wake 2*) · Lipman et al. 2008 (Green coordinates) · Sorkine & Alexa 2007 / Chao et al. 2010 (ARAP) · Sorkine 2005 · Nealen et al. 2006 · Dodik et al. 2023.
*Smith et al. 2000 matters for [[Deformation-aware displacement fields]] — Gruen et al. note it "even discusses animating displaced meshes" but does not describe the geometric artefacts.*

### APIs, hardware, production
Microsoft 2018/2020 (DXR) · Khronos 2020 (Vulkan RT) · Kubisch et al. 2022 (`VK_EXT_opacity_micromap`) · Werness 2022/2023 · Kubisch & Werness 2023 (`VK_NV_displacement_micromap`) · Patel & Miles 2025 (D3D12 OMM) · [[NVIDIA 2022 — Ada Lovelace Architecture|NVIDIA 2022/2023]] · Kilgariff et al. 2018 (Turing) · Bickford & Moreton 2023 · Sanzharov et al. 2020 · Sjöholm 2022 · Bavoil 2025 (*Indiana Jones* OMM) · AMD 2024 (GPURT) · Fenney 2024 (Imagination *Hot3D*) · Apple 2023 (Metal) · Christensen et al. 2018 (RenderMan) / Burley et al. 2018 (Hyperion) / Fascione et al. 2018 (Manuka) / Georgiev et al. 2018 (Arnold).

## Read against the open questions

| Direction | Nearest surveyed territory | Verdict from these bibliographies |
|---|---|---|
| [[Proximity and contact queries against micro-geometry\|1 — Proximity]] | nothing | **No collision or distance-query reference appears in any of the twelve.** RMIP names physics simulation in future work and cites nobody for it. → **but see the outside check: three direct precedents exist.** |
| [[Surface measure and sampling on implicit displaced surfaces\|2 — Measure]] | Wang et al. 2020; sampling refs in Ray Tracing Gems | Ogaki states the limitation and cites **no** prior attempt. No area/Jacobian-bounding reference anywhere. → **outside check: area and uniform sampling are solved (Ling et al. 2025).** |
| [[Prefiltered coverage from opacity hierarchies\|3 — Prefiltered coverage]] | **densest** — Lacewell 2008, Wyman & McGuire 2017, Enderton 2010, Munkberg 2016 | The precedents exist and are cited. The gap is that nobody connected them to a micromap hierarchy. Highest concurrent-work risk. |
| [[Hybrid displacement and opacity micromaps\|4 — Hybrid]] | both micromap lineages, separately | Both sides cite their own ancestry fully; **no reference bridges them**. |
| [[Non-ray queries — slicing, volume and mass properties\|5 — Non-ray queries]] | nothing | No fabrication, slicing, or mass-properties reference in any of the twelve. → **outside check: direct slicing is mature elsewhere, but not for this representation.** |
| [[Deformation-aware displacement fields\|6 — Deformation]] | Smith et al. 2000; skinning refs; NeuBase's deformation baselines | Partially surveyed from two sides that don't meet. Check Smith 2000 first. |
| [[Dynamic opacity micromaps\|7 — Dynamic opacity]] | DGF's locked-offset mode (animated *geometry*, not opacity) | No dynamic-opacity reference. |
| [[Inverse and differentiable micro-geometry\|8 — Inverse]] | Nicolet 2021, nvdiffrast, the neural-surface cluster | Well surveyed for *fitting to a mesh*; **nothing on fitting to images**. |
| [[Authoring, UV layout and micromap efficiency\|9 — Authoring]] | the parameterisation cluster | All about distortion; none about deduplication or budget allocation. |

## The 2026 additions

The three newest papers bring **new territory that the original twelve never surveyed**, and one striking absence.

**Foliage, vegetation and aggregate detail** (new, from [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage|van Antwerpen et al.]]): Cook et al. 2007 (stochastic simplification of aggregate detail) · Remolar et al. 2002 (geometric simplification of foliage) · Décoret et al. 2003 (billboard clouds) · Zioma 2007 (GPU-generated procedural wind) · Harris 2004 (fluid dynamics on GPU) · Sousa 2007 (Crysis vegetation) · SpeedTree 2024 · Remedy 2024 (*Alan Wake 2* GPU skinning) · Kuth et al. 2025 (real-time GPU tree generation) · Stadter et al. 2024 (neural volumetric LOD) · Bruneton & Neyret 2012 · Jakulin 2000 · Hasselgren et al. 2021 (appearance-driven simplification) · Andersson et al. 2020 (𝗟LIP) · Gottschalk et al. 1996 (OBBTree) · Lawson & Hanson 1995 (NNLS) · Meyer et al. 2010 (octahedral normals) · Hanika 2021 (shadow terminator) · Enderton et al. 2010 (stochastic transparency).

**Successive self-parameterisation and correspondence** (new, from [[Zhang et al. 2026 — DJM|DJM]]): Lee et al. 1998 (MAPS) · Liu et al. 2020 (neural subdivision), 2021 (intrinsic prolongation), 2023 (intrinsic error metrics) · Sharp et al. 2019 (intrinsic triangulations) · Kraevoy & Sheffer 2004 · Sander et al. 2001 · Maggiordomo et al. 2024 (**the inverse barycentric displacement problem** — the closed-form cubic DJM rejects) · Surazhsky & Gotsman 2003 · Pentapati et al. 2025 · Sivaram et al. 2024 (NGF) · Zhang et al. 2023 (progressive shell quasistatics for FEM) · Krishnamurthy & Levoy 1996.

**Tetrahedral cages and deformation** (new, from [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry|Gruen et al. 2026]]): Luton & Tricard 2025 (the direct predecessor) · Morrical et al. 2023 (RTX volumetric tetrahedral rendering) · Aman et al. 2022 · Lagae & Dutré 2008 · Káčerik & Bittner 2025 (UBVH) · Freudenthal 1942 / Domiter & Žalik 2008 (six-tet voxel split) · Si 2006 (TetGen) · Alliez et al. 2005 · Jacobson et al. 2011 (bounded biharmonic weights) · Joshi et al. 2007 (harmonic coordinates) · Mezger et al. 2003 (**collision detection for cloth**) · Müller et al. 2005 (**real time physics**) · Nießner 2013 (min-sum heuristic) · Kensler 2008 / Lee & Liktor 2020 (incremental and lazy BVH builds).

> [!note] The blind spots are narrowing, but not where it matters
> Physics and collision references finally appear — Mezger et al. 2003 and Müller et al. 2005 — but **only as background for why tetrahedral cages exist**, never as a query the system performs. The vault now cites collision literature without any paper doing collision. That strengthens rather than weakens [[Proximity and contact queries against micro-geometry]].

**Still absent after fifteen papers:** computational fabrication, slicing and mass properties · robotics · radiative transfer and canopy light interception (despite one paper being entirely about foliage) · remote sensing · geospatial and DEM analysis · simulation meshing · medical modelling. Formal methods remains a single entry.

## The outside check — geometry processing and simulation (2026-08-10)

The sanity check [[MOC — Open Questions]] called for, run against **directions 1, 2 and 5** across SIGGRAPH/Asia, ToG, EGSR, Eurographics, SGP, SCA and HPG. Everything below is **absent from all fifteen bibliographies above** and materially changes how those three directions must be pitched.

> [!danger] The one-line correction
> *"The field has optimised exactly one query"* is true of the **rendering displacement** literature and **false of geometry processing**. Spelunking the Deep (2022) is conservative-bounds-to-non-ray-queries; Uniform Sampling by Casting Rays (2025) is area-and-sampling from a ray subroutine. Directions 1 and 2 exist in representation-agnostic form already. State this distinction wherever the observation appears, or the whole folder reads as unaware of SGP.

### Queries on implicit and higher-order surfaces
Sharp & Jacobson 2022 (**Spelunking the Deep** — range analysis with affine-arithmetic variants over general implicit functions: ray casting, intersection testing, spatial hierarchies, mesh extraction, **closest point**, **bulk properties**, with accuracy guarantees) · Marschner, Zhang, Palmer & Solomon 2021 (**Sum-of-Squares Geometry Processing** — closest-point and distance queries on higher-order surfaces via convex relaxation) · Zhang, Marschner, Solomon & Tamstorf 2023 (**SOS Collision Detection for Curved Shapes and Paths** — exact collision on Bézier triangles without tessellating, ~300×, plus curved-trajectory CCD) · Ling, Madan, Sharp & Jacobson 2025 (**Uniform Sampling of Surfaces by Casting Rays** — Cauchy–Crofton; uniform samples *and* surface area from a ray-all-intersections subroutine alone).
*The single most load-bearing cluster in this section. Directions 1 and 2 are both downstream of it.*

### Collision, contact and friction
Nießner, Siegl, Schäfer & Loop 2013 (**Real-time Collision Detection for Dynamic Hardware Tessellated Objects** — collision against **displacement-mapped** patches via hardware tessellation and 1-bit voxelisation) · Otaduy & Lin 2003 (sensation-preserving simplification; **CLODs**, SGP 2003) and the 6-DoF haptic line, with haptic textures built from measured **height-displacement profiles** · Macklin, Erleben, Müller, Chentanez, Jeschke & Corse 2020 (local optimisation for robust **SDF collision**) · *Coupling Friction with Visual Appearance* 2021 (friction coefficients derived from surface micro-geometry facets) · *Contact detection between curved fibres: high order makes a difference*, ToG 2024 (low-order proxies produce **spurious contact forces**) · IPC / C-IPC (Li, Ferguson, Schneider, Langlois, Panozzo, Zorin, Jiang, Kaufman) · the SIGGRAPH *Contact and Friction Simulation* course (Andrews, Erleben et al.).
*Direction 1's territory. Nießner et al. 2013 is the paper that falsifies "nobody does contact against displaced geometry".*

### Displacement prefiltering and gradient statistics
Olano & Baker 2010 (**LEAN**) · Dupuy, Heitz, Iehl, Poulin, Neyret & Ostromoukhov 2013 (**LEADR** — first two moments of the **displacement gradients** stored in mipmaps, driving a Beckmann NDF with masking and shadowing) · Wu, Zhao, Yan & Ramamoorthi 2019 (**appearance-preserving prefiltering of displacement-mapped surfaces** — joint displacement+BRDF prefiltering, 6D shadowing/masking/interreflection scaling function) · Boubekeur's group: MIPNet, *Hybrid mesh-volume LoDs for all-scale pre-filtering*.
*Direction 2's territory — and note this is **the same group** as [[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)|TFDM]]/[[Thonat et al. 2023 — RMIP|RMIP]]. Concurrent-work risk on direction 2 is concentrated, not diffuse.*

### Fabrication, slicing and solid properties
Lefebvre's **IceSL** (GPU modeller/slicer, CSG over meshes/voxels/implicits/shaders **direct to G-code, no STL**) · direct slicing of implicits, NURBS, and dilated/eroded models in the CAD/AM literature · *Displacement Mapping as a Surface Texturing Tool for Additively Photopolymerized Components*, Micromachines 2024 (**displacement-mapped assets printed by refining the mesh to 10% of the smallest feature size** — the named victim direction 5 lacked) · generalized winding numbers (Jacobson, Kavan & Sorkine 2013; Barill et al. 2018; one-shot GWN 2024) · Sellán, Aigerman & Jacobson 2021 (**Swept Volumes**) · Mirtich 1996 (polyhedral mass properties) · curved/high-order meshing: Bézier guarding and its 3D successor, curvilinear CFD mesh generation.
*Direction 5's territory, and the one that came out stronger.*

### Grid-free / query-driven geometry processing
Sawhney & Crane 2020 (**Monte Carlo Geometry Processing**, walk on spheres) · Sawhney, Seyb, Jarosz & Crane 2022 (variable coefficients) · Sawhney, Miller, Gkioulekas & Crane 2023 (**Walk on Stars**).
*Not prior art for any direction, but the strongest available argument for the folder's thesis: an entire PDE-solving literature that needs **only closest-point queries** and no meshing. If displaced micro-geometry could answer closest-point queries, WoS/WoSt would run on it directly. Nobody has said this.*

## Blind spots

Entire fields that **no paper in this vault cites at all**. Absence here is evidence about this literature's attention, not about the world — and as of 2026-08-10 three of these have been checked against the world and were **not** empty. Annotated accordingly.

- **Collision detection and proximity queries** — no reference to distance fields for contact, continuous collision detection, or contact manifolds. → **Checked. Not a gap in the world:** see *Collision, contact and friction* above. Still a gap in *this* representation.
- **Computational fabrication** — no slicing, no additive-manufacturing, no mass-properties work. → **Checked. Direct slicing is mature** for CSG/implicits/NURBS; the gap is specifically tangent-space displacement over a base mesh.
- **Robotics** — nothing on grasp planning, tactile or haptic surface representation. → **Partly checked: haptics is not open** — Otaduy & Lin have rendered forces from height-displacement profiles since 2003. Grasp planning and tactile sensing remain unchecked.
- **Radiative transfer and canopy modelling** — no Beer–Lambert, leaf-area-index, or vegetation light-interception work, despite foliage being the field's canonical asset.
- **Remote sensing / LiDAR simulation** — absent.
- **Geospatial and terrain analysis** — RMIP names "high-resolution geo mapping" in a closing sentence and cites nobody; no DEM or viewshed literature.
- **Simulation meshing** — no CFD boundary-layer or FEM meshing work, despite the shell being a natural prismatic layer.
- **Medical and anatomical modelling** — absent.
- **Formal methods** — one entry (ACL2, Waldemarson & Doggett) in the entire vault.

## Per-paper source lists

Full reference detail lives in each paper note's *Relation to other work* section:

[[Thonat et al. 2021 — Tessellation-Free Displacement Mapping (TFDM)]] · [[Ogaki 2023 — Nonlinear Ray Tracing for Displacement and Shell Mapping]] · [[Thonat et al. 2023 — RMIP]] · [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)]] · [[Maggiordomo et al. 2023 — Micro-Mesh Construction]] · [[Dou et al. 2024 — Differentiable Micro-Mesh Construction]] · [[Barczak et al. 2024 — DGF]] · [[Gruen et al. 2024 — Ray Tracing Animated Displaced Micro-Meshes]] · [[Mendiratta et al. 2026 — NeuBase]] · [[Gruen et al. 2020 — Sub-Triangle Opacity Masks]] · [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps]] · [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps]]

Back to [[MOC — Open Questions]] · [[Home]]
