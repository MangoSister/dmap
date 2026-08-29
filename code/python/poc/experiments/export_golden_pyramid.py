"""Export golden .npy files for the C++ port of the Taylor pyramid.

Phase S3 of the A-MVP implementation plan: the C++ `TaylorPyramid` must
reproduce the numpy reference node-for-node at every level (same fold, same
double arithmetic). Cases: the two Phase 1 textures at 257 nodes with
representative scales, plus a seeded random grid at 65 nodes.

Output layout: code/data/golden/pyramid/<case>/values.npy, scale.npy, and
per level k: h0_L<k>.npy, gu_L<k>.npy, gv_L<k>.npy, r_L<k>.npy,
ru_L<k>.npy, rv_L<k>.npy. All float64.
"""

import numpy as np

from _common import DATA_DIR, CODE_DIR
from dmapref.displacement import load_texture, downsample_box
from dmapref.pyramid import TaylorPyramid

GOLDEN_DIR = CODE_DIR / "data" / "golden" / "pyramid"
SEED = 2027
TEX_NODES = 257


def export_case(name, values, scale):
    pyr = TaylorPyramid(values, scale)
    out = GOLDEN_DIR / name
    out.mkdir(parents=True, exist_ok=True)
    np.save(out / "values.npy", np.asarray(values, dtype=np.float64))
    np.save(out / "scale.npy", np.array([scale]))
    for k, level in enumerate(pyr.levels):
        for key, arr in level.items():
            np.save(out / f"{key}_L{k}.npy", arr)
    return pyr.n_levels


def main():
    rng = np.random.default_rng(SEED)
    n = 0
    n += export_case("disp_rock_257",
                     downsample_box(load_texture(DATA_DIR / "disp_rock.png"),
                                    TEX_NODES), 0.013)
    n += export_case("disp_cobble_257",
                     downsample_box(load_texture(DATA_DIR / "disp_cobble.png"),
                                    TEX_NODES), 0.07)
    n += export_case("random_65", rng.random((65, 65)), 1.0)
    print(f"wrote 3 cases ({n} levels total) to {GOLDEN_DIR}")


if __name__ == "__main__":
    main()
