---
title: Plan — Step 1 dataset loader
tags: [plan, step-1, dataset, displacement-map, manifest, reproducibility]
status: complete
created: 2026-08-23
updated: 2026-08-23
parent: "[[Plan — Step 1 ray reference experiment]]"
implementation-status: complete
---

# Plan — Step 1 dataset loader

> [!abstract] Purpose
> Implement the frozen procedural and file-backed displacement corpus, persist exact source/crop/normalization metadata, and generate the dataset-manifest figure. This step traces no rays and makes no hierarchy-benefit decision. It prevents asset selection, crop selection, and normalization from changing after query results are visible.

## 1. Scope and non-goals

This unit adds:

- all procedural IDs required by the development, decision, held-out, and sweep manifests;
- the seven declared source PNGs and two declared native-crop variants;
- deterministic scalar decode, crop, resize, percentile normalization, descriptors, IDs, and hashes;
- tests and a manifest/thumbnail figure; and
- an adapter boundary that later lets the lifecycle runner request a dataset ID.

It does **not** add `S02`, controlled ray families, traversal runs, gate evaluation, CUDA, DDA, external baselines, or new assets. No source is downloaded and no crop is chosen visually.

## 2. Frozen module boundary

Add `scripts/step1_datasets.py`. It may import NumPy, Pillow, hashing/path utilities, and the existing descriptor/schema helpers, but it must not import ray generation, rational-ray construction, traversal, or leaf intersection.

Primary API:

```text
dataset_registry() -> immutable logical specifications
load_height_grid(dataset_id, cells, seed, data_root) -> DatasetGrid
dataset_manifest(dataset_ids, cells, seeds, data_root) -> records
```

`DatasetGrid` contains a float64 `(cells+1,cells+1)` height array plus canonical metadata. Returned arrays are read-only. Callers must not infer source/crop rules from an ID string; the registry is authoritative.

The current `prototype_first_order_ray.make_heights` remains the legacy P3 path. The new loader initially lives beside it. Only after loader tests pass may the lifecycle runner accept new dataset IDs.

## 3. Procedural registry

Coordinates use `u=ix/cells`, `v=iy/cells`, with array index `[iy,ix]`.

| ID | Frozen construction |
|---|---|
| `P00-constant` | `h=0.47` |
| `P01-ramp-u` | `h=0.2+0.6u` |
| `P02-ramp-oblique` | `h=0.12+0.58u+0.17v` |
| `P03-sine-bump` | Current two-cycle sine/cosine field plus Gaussian centered at `(0.63,0.37)` with denominator `0.025` |
| `P04-smooth-noise-sNN` | `default_rng(seed).normal`, then exactly six edge-padded five-point passes with center weight 4 and axial weights 1, divided by 8; min/max map to `[0.2,0.8]` |
| `P05-impulse-center` | `0.2`, with `[cells//2,cells//2]=0.9` |
| `P05b-impulse-offcenter` | `0.2`, with `[floor(0.67*cells),floor(0.31*cells)]=0.9` |
| `P06-checker-1` | `0.2+0.6*((ix+iy) mod 2)` |
| `P07-checker-4` | `0.2+0.6*((floor(ix/4)+floor(iy/4)) mod 2)` |
| `P08-white-noise-sNN` | `0.2+0.6*default_rng(seed).random` |

The explicit `[0.2,0.8]` P08 range is the frozen decision-corpus definition. The legacy `noise` generator's `[0,1]` range remains historical P3 behavior and is not silently relabeled as P08 decision evidence.

Seeded IDs must include a two-digit seed suffix in their logical `map_id`; the actual integer seed remains separately recorded. Seed is ignored—but still recorded—for unseeded procedural maps.

## 4. File registry and audited source contract

All current files are 1024×1024. Official runs require exact SHA-256 equality; a mismatch is a hard manifest error.

| Logical source | Path | Pillow mode / native scalar | SHA-256 |
|---|---|---|---|
| `R00-test` | `data/test_disp.png` | `L`, uint8 | `a08085c6c5d9c7a305611cceb0a5fbe09340e75bb89d2f0e20bc0da320a844e2` |
| `R01-rock` | `data/disp_rock.png` | `L`, uint8 | `610a4cced609a603f800724ae06238ca5150efeefb784a9e97991f8927f34979` |
| `R02-cobble` | `data/disp_cobble.png` | `L`, uint8 | `67294a12a2868168e684d016de9cb399bcc85f58129083101bbecfc97db0378a` |
| `R03-terrain` | `data/disp_terrain.png` | `L`, uint8 | `d322d365307231d8ea523f92a6d30ac649f7a74a722d9ede42fa393f7bdf7dd2` |
| `R04-brick` | `data/disp_brick.png` | `RGBA`, red uint8 | `52cb4288e90fafc2e3e17cf2a5699391e946b397c12ba68c0ea48ed47e3166af` |
| `R05-leather` | `data/disp_leather.png` | `I;16`, uint16 | `85efb039dad9c5d67241ad4b40eb29cf572ead597ce14297576e386f167042c0` |
| `R06-sandrock` | `data/disp_sandrock.png` | `RGB`, red uint8 | `addc075e39cabdf73638db13a7ac1c2a8e30f744ef68880ab24b4deda1df9c65` |

Variants:

- macro: `R00-test-macro`, `R01-rock-macro`, `R02-cobble-macro`, `R03-terrain-macro`, `R04-brick-macro`, `R05-leather-macro`, `R06-sandrock-macro`;
- held-out native: `R01b-rock-native`, `R05b-leather-native`.

The macro crop is the integer half-open rectangle `[256,768)×[256,768)`. The native crop is the centered `(cells+1)²` sample window with `x0=(W-(cells+1))//2`, `y0=(H-(cells+1))//2`. Array row zero remains source top; no vertical flip is applied. Metadata records this convention.

## 5. Decode, crop, resize, and normalization

1. Open with Pillow and force complete decoding before leaving the context.
2. Verify file hash, size, expected mode, array shape, and expected unsigned integer dtype.
3. For RGB/RGBA use array channel zero; ignore other channels and alpha. For scalar modes use the scalar array.
4. Convert using the full native integer range: `/255` for uint8 and `/65535` for uint16. PNG color values are treated as scalar displacement samples; no sRGB transfer is applied.
5. Macro mode: crop `[256:768,256:768]`, convert to Pillow `F`, resize to `(cells+1,cells+1)` using `Image.Resampling.BOX`, then convert to float64.
6. Native mode: take the centered samples with no filter and retain float64 values.
7. Compute p1/p99 on the resulting grid with NumPy `quantile(..., method="linear")`.
8. Reject a non-finite grid or `p99-p1 <= 1024*eps*max(1,abs(p1),abs(p99))`.
9. Normalize `0.2+0.6*(x-p1)/(p99-p1)` and clamp to `[0.2,0.8]`.
10. Mark the final array read-only.

Metadata records source path relative to the repository, source hash/mode/dtype/bit depth/size, channel policy, crop rectangle and mode, resize filter, Pillow version, orientation, p1/p99, normalization endpoints, cells, seed, final-grid SHA-256, and all frozen map descriptors.

The final-grid hash is over canonical little-endian float64 C-order bytes plus shape, dataset ID, cells, and seed; it is not a platform-native `tobytes()` hash without an endianness declaration.

## 6. Manifest figure

After loader tests pass, generate one explanatory figure—not a result plot:

1. thumbnail grid for every decision-corpus map at `cells=32`, using a shared `[0.2,0.8]` color scale and nearest-neighbor display of the represented samples;
2. descriptor panel plotting plane-residual RMS against high-frequency fraction, with procedural/file-backed marker shape and frozen map-class color; and
3. labels containing logical dataset IDs, never source filenames alone.

Export PDF/SVG/PNG under `figures/step1_dataset/` and generate it from an executable notebook or small figure script using the existing paper style. Do not tune crop or inclusion after seeing this figure.

## 7. Tests before lifecycle integration

1. Registry contains exactly the declared IDs, paths, crop modes, and source hashes; duplicate IDs fail.
2. Every source passes audited mode, dtype, size, and SHA-256 checks, including the 16-bit leather path and red-channel RGB/RGBA policy.
3. Every procedural map is deterministic and matches its exact formula/seed construction at `cells=16` and `32`.
4. P01/P02 have the expected exact gradients and near-zero plane residual; P00 is constant; P05/P05b impulse indices are exact; P06/P07 block patterns are exact; P08 lies in `[0.2,0.8]`.
5. Macro crop metadata is exactly `[256,256,768,768]`; native crop coordinates and unfiltered source samples match an independent direct-array check.
6. Every file-backed final grid has shape `(cells+1,cells+1)`, finite float64 values, read-only storage, and range `[0.2,0.8]` with recorded p1/p99.
7. Repeated loads have identical canonical metadata, descriptors, and final-grid hashes.
8. `grid_id` changes with dataset, cells, seed, source hash, or crop mode, and remains stable otherwise.
9. Dataset manifest JSONL round-trips under `step1-v1`; the existing 39 P1–P4 tests remain unchanged.
10. The manifest figure regenerates successfully in the project Conda environment and includes every frozen decision map exactly once.

**Gate D1:** all ten tests pass, source hashes match, manifest records and figure are generated, and no traversal code changes. Only then write the controlled ray-generator plan and integrate dataset IDs into a new lifecycle scientific configuration.

## 8. D1 completion — 2026-08-23

D1 passes. `scripts/step1_datasets.py` contains the independent registry/loader and `scripts/generate_step1_dataset_manifest.py` emits the immutable manifest and overview figure. The complete Step 1 suite now has 50 passing tests: the 39 P1–P4 persistence/instrumentation/lifecycle tests plus 11 D1 tests. The final presentation revision is configuration `2a294309ffff751aba258cb5e2559dc340cd8cb3de1a302357e80b1af4c21454`, with 15 unique grid IDs and dataset-stream SHA-256 `4429e3702631236508c8209b0c3a3d3ee29add9dda75d8f8787a3cfbf205eab7`. Its records reopen under `step1-v1` with matching references and hashes.

Artifacts:

- `experiments/step1_dataset_manifest/dataset-2a294309ffff/`;
- `figures/step1_dataset/step1_dataset_manifest_v2.{pdf,svg,png}`; and
- [[Plan — Step 1 controlled ray generator]], frozen before D2 implementation.

The earlier `dataset-9ef793590fdd`/unversioned figure is retained as a development artifact; no scientific dataset value changed in the v2 presentation revision.

---

Related: [[Plan — Step 1 ray reference experiment]] · [[Plan — Step 1 persistence and instrumentation]]
