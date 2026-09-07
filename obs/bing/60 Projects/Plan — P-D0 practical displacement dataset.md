---
title: Plan — P-D0 practical displacement dataset
tags: [plan, displacement-mapping, dataset, GPU, DDA, reproducibility]
status: complete
created: 2026-08-23
updated: 2026-08-23
parent: "[[Plan — Practical first-order DDA ray traversal]]"
implementation-status: passed
---

# Plan — P-D0 practical displacement dataset

> [!abstract] P-D0 decision
> Freeze and audit the full-resolution performance maps and native-frequency correctness windows before any certified DDA implementation or timing. P-D0 may decode, hash, describe, and visualize displacement inputs. It may not run traversal, select assets/crops from timing, or claim rendering performance.

## 1. Existing assets to retain

| ID | Local source | Intended role |
|---|---|---|
| `PH-rock05-1k` | `data/disp_rock.png` | low-resolution/cached end of resolution sweep |
| `PH-rock05-2k` | `data/disp_rock_2k.png` | middle of resolution sweep |
| `PH-rock05-4k` | `data/disp_rock_4k.png` | bandwidth/capacity end of resolution sweep |
| `PH-cobble08-1k` | `data/disp_cobble.png` | rounded blocks plus deep joints |
| `PH-rockyterrain02-1k` | `data/disp_terrain.png` | low-frequency terrain plus rocks |
| `PH-brick001-1k` | `data/disp_brick.png` | structured discontinuities/joints |
| `PH-leather02-1k` | `data/disp_leather.png` | fine coherent grain; native 16-bit audit |
| `PH-coastsandrocks02-1k` | `data/disp_sandrock.png` | mixed scales; native multi-channel audit |

`data/test_disp.png` remains a synthetic rendering/control input but is not counted as a practical material.

## 2. Additional public assets

Acquire exactly one `4K` displacement image for each of these Poly Haven CC0 assets, using lossless PNG at the highest available source bit depth unless only a floating EXR preserves the published displacement:

1. `PH-concrete-layers-4k` — layered formwork, chips, and cracks;
2. `PH-sand01-4k` — smooth macro undulation plus fine compacted detail;
3. `PH-dirt-4k` — footprints/debris and mixed-frequency soil;
4. `PH-rockytrail02-4k` — rugged gravel/path structure.

Persist the exact Poly Haven asset slug, download URL, CC0 declaration URL, content hash, byte count, native dimensions, channel policy, dtype/bit depth, and acquisition date. Do not substitute a visually similar asset without revising this plan before download.

If a new asset cannot be fetched reproducibly, mark it unavailable and stop P-D0 rather than quietly replacing it.

## 3. Decode and scalar contract

- Decode PNG with a path that preserves `uint16`; decode EXR to float without display transfer.
- Never use `stbi_load`'s forced RGBA8 conversion for P-D0 scientific values.
- Scalar single-channel sources remain scalar. For RGB/RGBA sources, accept a channel only when the published displacement file encodes identical grayscale channels; otherwise record and use the documented displacement channel.
- No sRGB transfer is applied to displacement values.
- Preserve source values in the manifest. Scene displacement scale is a later independent variable; do not percentile-normalize the performance maps.
- Record finite range, p0.1/p1/p50/p99/p99.9, constant/NaN counts, gradient and second-difference statistics at native resolution, and multi-scale residual descriptors.

## 4. Frozen correctness windows

For each distinct practical full-resolution image, choose four `65×65` sample windows (representing `64×64` cells) before traversal:

1. one low-discrepancy position in each image quadrant;
2. positions derived only from the source SHA-256 and window index;
3. no filtering or resizing;
4. exact integer crop coordinates persisted; and
5. window hashes over canonical native scalar values.

These windows feed the exhaustive CPU oracle. They retain native texture frequency while bounding polynomial count. No crop may be moved after viewing ray results.

## 5. Performance map contract

GPU tests use complete source maps at native `1K/2K/4K` resolution. Mip/hierarchy construction may add conservative padding but may not resample the leaf displacement. Record expected height bytes and min/max/first-order hierarchy bytes for `float32`, outward `float16`, and candidate packed layouts.

The Rock 05 1K/2K/4K images must be checked for semantic identity and resolution consistency. If they are separately authored/filtered rather than a true resolution family, report that limitation and retain them as three related assets rather than treating individual texels as nested ground truth.

## 6. Required figures

Generate an input-audit contact sheet with, for every asset:

- native displacement thumbnail under a common percentile display only;
- a shaded height-field preview using the same declared preview amplitude and camera;
- resolution, bit depth, and source label; and
- markers for the four native correctness windows.

Add a descriptor plot showing multi-scale plane-residual energy versus high-frequency energy. These are dataset figures, not method-quality or performance figures.

Separately index the already existing rendering comparisons:

- Mode 0 versus heuristic Mode 1 on curved, sphere, quad, and twisted-quad shells;
- oracle/M0/M1 correctness images; and
- the obsolete direct-CC Mode 3 torus comparison.

Every index caption must state that none of these images is the new first-order certified DDA.

## 7. Persistence and tests

Create a content-addressed run under `experiments/practical_dataset_manifest/` with:

- `assets.jsonl.gz`;
- `windows.jsonl.gz`;
- `result.json` and immutable configuration;
- source/download receipts; and
- generated PDF/SVG/PNG figures under `figures/practical_dataset_manifest/`.

Tests independently verify source hashes, dimensions/modes/dtypes, lossless scalar decode, grayscale-channel agreement, deterministic windows, byte-identical regeneration, window bounds/hashes, resolution-family metadata, and figure existence.

## 8. P-D0 gate

Pass only if:

- at least ten distinct practical material identities are audited, including the four frozen additions;
- Rock 05 provides the complete 1K/2K/4K sweep;
- at least four practical maps preserve more than 8 bits or floating precision through the scientific loader;
- all native windows and hashes regenerate exactly;
- the corpus spans smooth/coherent, mixed-frequency, structured joints, fine grain, and declared difficult high-frequency content; and
- no method traversal or timing influenced inclusion or crop selection.

After P-D0 passes, write the exact P-D1 tube-supercover DDA reference plan before implementation. If P-D0 is blocked only by external acquisition, existing-source decode/tests may remain a development artifact but cannot be promoted as the final performance manifest.

## 9. Completed outcome — 2026-08-23

P-D0 passed every frozen gate in content-addressed run `p-d0-71627ba7b07f`:

- 12 audited native images representing 10 practical material identities;
- 48 immutable `65×65` correctness windows, four per source image;
- six images preserving more than 8 bits through the scientific loader;
- exact publisher-MD5 verification for the four newly acquired official 4K PNGs;
- all required smooth/coherent, mixed-frequency, structured-joint, fine-grain, and difficult high-frequency regimes; and
- no traversal or timing input used for source or window selection.

The Rock 05 `1K/2K/4K` images have cross-resolution correlation above `0.99996`, but exact BOX-downsample equality is false. They are therefore retained as a related published resolution family, not treated as texel-nested ground truth.

Implementation and audit artifacts:

- `scripts/practical_datasets.py` — immutable registry, lossless scalar decoder, descriptors, window generator, and deterministic persistence;
- `scripts/generate_practical_dataset_manifest.py` — manifest, dataset figures, and historical-render index;
- `scripts/test_practical_datasets.py` — seven passing source/decode/window/reproducibility tests;
- `experiments/practical_dataset_manifest/p-d0-71627ba7b07f/` — configuration, receipts, asset/window manifests, resolution audit, and result; and
- `figures/practical_dataset_manifest/` — input audit, descriptor plot, and explicitly historical Mode 0/Mode 1 render comparison.

The existing render audit confirms that the old heuristic Mode 1 can look close on favorable curved/sphere cases but has visible corruption and large cross-shaped failures on the twisted proxy. These images motivate certified tube coverage; they are not evidence for the new first-order DDA.

---

Sources: [Poly Haven Rock 05](https://polyhaven.com/a/rock_05) · [Concrete Layers](https://polyhaven.com/a/concrete_layers) · [Sand 01](https://polyhaven.com/a/sand_01) · [Dirt](https://polyhaven.com/a/dirt) · [Rocky Trail 02](https://polyhaven.com/a/rocky_trail_02)

Related: [[Plan — Practical first-order DDA ray traversal]] · [[Plan — Step 1 dataset loader]]
