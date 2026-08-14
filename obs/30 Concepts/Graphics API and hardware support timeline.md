---
title: Graphics API and hardware support timeline
tags: [concept, hardware, api, vulkan, directx, status]
researched: 2026-08-09
---

# Graphics API and hardware support timeline

The state of micro-geometry in shipping APIs and silicon. **Researched August 2026** — this area moves fast, and several notes below already contradict vendor pages that were never updated.

**The headline:** the two micromap kinds diverged completely. **Opacity** micromaps went from research prototype to a ratified cross-vendor Khronos extension and a DirectX tier. **Displacement** micromaps were withdrawn by their own vendor and replaced with cluster acceleration structures. Half the papers in this vault are oriented around a hardware target that no longer exists.

## Timeline

| Date | Event |
|---|---|
| 2020-07 | [[Gruen et al. 2020 — Sub-Triangle Opacity Masks]] proposes storing sub-triangle opacity in the BVH |
| 2022 | [[NVIDIA 2022 — Ada Lovelace Architecture]] ships both an Opacity Micromap Engine and a Displaced Micro-Mesh Engine |
| 2022-08-24 | `VK_EXT_opacity_micromap` finalised — extension #397, revision 2, ratified |
| 2023 | `VK_NV_displacement_micromap` released (vendor extension, provisional) |
| 2024-10-09 | UL ships an Opacity Micromap feature test in 3DMark for Android with MediaTek — OMM reaches mobile |
| 2024-12-09 | *Indiana Jones and the Great Circle* ships OMMs in its path-tracing release |
| **2025-02-06** | **RTX Mega Geometry** launches: `VK_NV_cluster_acceleration_structure` (CLAS) and `VK_NV_partitioned_acceleration_structure` |
| **2025-02-13** | **`Displacement-MicroMap-SDK` archived.** "The vulkan extension `VK_NV_displacement_micromap` is no longer available." |
| 2025-03 | DXR 1.2 announced at GDC 2025: Opacity Micromaps + Shader Execution Reordering |
| 2025-05-30 | Agility SDK **1.616-retail** ships OMM (`D3D12_RAYTRACING_TIER_1_2`); 1.717-preview adds SER |
| 2026-05-08 | **`VK_KHR_opacity_micromap`** ratified — extension #624, shipped in the Vulkan 1.4.351 spec update |
| 2026-05 | AMD **DGF SDK v1.2** with SuperCompression |
| 2026-07 | HPG 2026: [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps\|CSM opacity micromaps]] compress OMMs 351× and confirm the DMM deprecation in print |
| 2026-07 | HPG 2026: [[van Antwerpen et al. 2026 — Real-time Path Tracing of Massive Dynamic Foliage\|NVIDIA foliage]] ships **PTLAS + clusters + OMM** at 60M plants; never mentions micro-meshes |
| 2026-07 | HPG 2026: [[Gruen et al. 2026 — Ray Tracing Massive Amounts of Animated Geometry\|AMD]] declares "DMMs have not been widely adopted" and uses tetrahedral cages instead |

## Opacity micromaps: promoted

`VK_EXT_opacity_micromap` (2022) was already cross-vendor at the contributor level — Kubisch and Werness (NVIDIA), Barczak (Intel), Smith (AMD). Its API: `VkMicromapEXT` objects built via `vkCmdBuildMicromapsEXT`, with `VkAccelerationStructureTrianglesOpacityMicromapEXT` chained onto the geometry at BLAS build. A **micromap triangle index buffer** maps each geometry triangle to a micromap triangle, which is what enables the reuse and deduplication [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|Waldemarson & Doggett]] show is essential. Indexing uses a space-filling curve on the triangular domain, chosen for locality.

**`VK_KHR_opacity_micromap` (2026-05-08) is a redesign, not a rename.** From the spec: *"While this is significant API breakage from the EXT, it is a better design choice."* What changed:

- micromaps are integrated as **acceleration structure types** rather than a parallel API;
- **discardable micromaps and the `DATA_UPDATE` build flag are removed**;
- host-side build/copy/query commands and `hostMicromapCommands` are **EXT-only legacy**, absent from KHR;
- KHR requires an **explicit execution mode declaration**, where EXT always consumed micromap data in shaders.

Contributors span Qualcomm, NVIDIA, AMD, Intel, MediaTek, Imagination and Valve — genuinely cross-vendor. On mobile, `VK_ARM_pipeline_opacity_micromap` lets Ray Query pipelines declare they will *not* use OMMs, so tile-based deferred renderers can skip the handling overhead.

**DirectX.** D3D12's OMM is the same primitive: a uniformly subdivided mesh of `4ᴺ` micro-triangles on a `2ᴺ × 2ᴺ` barycentric grid, in 1-bit or 2-bit format, ordered along a space-filling curve, with **OMM arrays stored separately from the BLAS so they can be reused across geometries**. Retail use needs Agility SDK 1.616.1 and `D3D12_RAYTRACING_TIER_1_2`; RayQuery + OMM additionally needs 1.717.1-preview and Shader Model 6.9.

**Vendor support** as stated by Microsoft on 2025-05-30: NVIDIA full support across all GeForce RTX GPUs; **AMD "planned for future hardware platforms"**; **Intel "currently evaluating"**; WARP supports OMM from 1.0.14.2.

> [!warning] Unverified
> Secondary reporting claims Intel's Celestial (Xe3) will support OMM. Not confirmed against an Intel primary source. I also found no authoritative 2026 update to the vendor matrix above.

## Displacement micromaps: withdrawn

Both `Displacement-MicroMap-SDK` and `Displacement-MicroMap-Toolkit` are **archived and read-only**. NVIDIA's stated replacement:

> We recommend exploring NVIDIA RTX Mega Geometry, which can provide similar functionality with greater flexibility.

`VK_NV_cluster_acceleration_structure` is described as **not a one-to-one replacement**, but as the preferred direction for high-tessellation geometry. The `vk_tessellated_clusters` sample demonstrates real-time adaptive tessellation with displacement — i.e. displacement is now something you *build out of clusters*, not a primitive the BVH understands. See [[Karis et al. 2021 — Nanite]] for where the cluster idea comes from.

This is corroborated in the peer-reviewed literature, not only by repository notices. [[Chernaik et al. 2026 — Common Subtree Merging Compressed Opacity Micromaps|Chernaik et al.]] state it plainly at HPG 2026:

> While initially available through the provisional Vulkan vendor extension `VK_NV_displacement_micromap`, the extension is now **deprecated by the more general cluster acceleration structure approach** from `VK_NV_cluster_acceleration_structure`.

Blackwell's 4th-generation RT Core doubled ray–triangle intersection throughput and upgraded the Triangle Intersection engine to a **Triangle Cluster Intersection Engine**, adding Triangle Cluster Compression and Linear Swept Spheres. Mega Geometry runs on all RTX cards but is hardware-accelerated only on Blackwell.

> [!warning] Unverified: did Blackwell physically remove the DMM engine?
> Confirmed: Ada had a DMM engine; Blackwell's published RT-core feature list does not mention one; the entire software stack was withdrawn in Feb 2025. The strong inference is that DMM is dead as a shipping feature, but **no primary NVIDIA statement says the silicon removed the unit**. The RTX PRO Blackwell architecture whitepaper is the place to settle it.

> [!warning] Stale vendor page
> NVIDIA's micro-mesh developer page still advertises DMM ("up to a 50X increase in geometry"), links the archived toolkit, cites RTX 40 Series for hardware acceleration, and carries **no deprecation notice** (retrieved 2026-08-09). Do not treat it as current.

## AMD: DGF

[[Barczak et al. 2024 — DGF|DGF]] is now a shipping open-source SDK (**v1.2, May 2026**), adding **SuperCompression** — smaller and more portable while preserving exact block reconstruction and fast decode to either DGF blocks or conventional meshlets. AMD describes DGF as "a hardware-friendly format which will be directly supported by future GPU architectures".

> [!warning] Unverified
> Secondary sources (VideoCardz) report DGF gets full hardware support in **RDNA 5**, with shader-based decompression on current cards, and quote ~22–30% further savings from SuperCompression. GPUOpen's own page states only "All vendors supported" and does not name an architecture. **RDNA 4 / RX 9000 (launched 2025-02-28) has no DGF hardware decompressor.**

AMD has no displaced-micro-mesh equivalent; its bet is DGF plus cluster-style compression. It nonetheless contributed to both the EXT and KHR opacity extensions.

## Shipping evidence

- **Indiana Jones and the Great Circle** (NVIDIA post-mortem, 2025-05-15, RTX 5080, 26.9M triangles in the RTAS): `TraceMain` **7.90 → 3.58 ms (−55%)**, `SharcUpdate` −55%, and **any-hit shader samples fell from 17% to 3%** of periodic samples. Baking used 4-state format at max subdivision level 6; **95% of OMMs bake in under 1 s and are under 200 KB**, with a 128 MB total budget. Indirect rays use a **2-state approximation** via `gl_RayFlagsForceOpacityMicromap2StateEXT`; shadow rays stay fully conservative 4-state — a direct application of the state conversion described in [[Micromap]].
- **Alan Wake 2** (Remedy, GDC 2025): ray tracing cost down roughly **one third** using OMM and SER together.
- **The Witcher 4** (CD Projekt RED / UE5, ~GDC 2026): RTX Mega Geometry foliage using **both clusters and opacity micromaps**, 4K at 80 FPS with DLSS Quality on an RTX 5090.

Microsoft claims up to **2.3×** in path-traced scenes from OMM alone.

> [!warning] Unverified
> Blender's OMM/micro-mesh status could not be confirmed either way (only a Cycles issue discussing OMM VRAM cost). Whether Unreal exposes OMM as a first-class engine feature versus an NVIDIA branch/plugin is also unconfirmed. The canonical GitHub home of the Opacity Micromap SDK is ambiguous — `NVIDIAGameWorks/Opacity-MicroMap-SDK` and `NVIDIA-RTX/OMM` both appear; check both before citing.

## Why it matters for this vault

Read alongside [[Tessellation-free vs pre-tessellation]]: several papers here justify their software approach by pointing at what hardware *would* do. That argument has aged in opposite directions depending on which micromap they meant. For opacity, hardware won — [[Waldemarson & Doggett 2024 — Succinct Opacity Micromaps|the succinct encoding]] loses to the native path. For displacement, the software methods ([[Thonat et al. 2023 — RMIP|RMIP]], [[Hoetzlein 2025 — Projective Displacement Mapping (PDM)|PDM]]) outlived the hardware they were competing with.
