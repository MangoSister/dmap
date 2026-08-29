"""Export golden .npy files for the C++ port of the pointwise evaluation.

Phase S1 of the A-MVP implementation plan ("Plan — A-MVP sampling
implementation"): the C++ `displaced_surface` module must reproduce the
numpy reference. This script writes, per case, the inputs (triangle,
texture grid, scale, sample points) and the reference outputs of
dense_reference.pointwise_fields at seeded random sample points.

Output layout: code/data/golden/pointwise/<case>/<name>.npy, one array per
file, all float64. The C++ validation task `validate_pointwise` loads these
and compares.
"""

import sys
from pathlib import Path

import numpy as np

from _common import DATA_DIR, CODE_DIR
from dmapref.mesh import load_obj
from dmapref.displacement import load_texture, downsample_box
from dmapref.dense_reference import pointwise_fields

GOLDEN_DIR = CODE_DIR / "data" / "golden" / "pointwise"
SEED = 2027
TEX_NODES = 257                    # match exp05's downsampled grid
N_POINTS = 4096                    # sample points per case
N_TRIANGLES = 3                    # seeded triangles per mesh
CASES = [("cc_torus.obj", "disp_rock.png"),
         ("spot.obj", "disp_cobble.png")]
AMPLITUDES = [0.05, 0.2]


def mean_edge(tri):
    return (np.linalg.norm(tri.e1) + np.linalg.norm(tri.e2)
            + np.linalg.norm(tri.e2 - tri.e1)) / 3.0


def main():
    rng = np.random.default_rng(SEED)
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)

    meshes = {m: load_obj(DATA_DIR / m) for m in sorted({c[0] for c in CASES})}
    textures = {t: downsample_box(load_texture(DATA_DIR / t), TEX_NODES)
                for t in sorted({c[1] for c in CASES})}

    n_cases = 0
    for mesh_name, tex_name in CASES:
        mesh = meshes[mesh_name]
        values = textures[tex_name]
        tri_ids = rng.choice(mesh.n_triangles, size=N_TRIANGLES, replace=False)
        for amp in AMPLITUDES:
            for t in tri_ids:
                tri = mesh.triangle(int(t))
                scale = amp * mean_edge(tri)
                X = rng.random(N_POINTS)
                Y = rng.random(N_POINTS)
                f = pointwise_fields(tri, values, scale, X, Y)

                case = (f"{Path(mesh_name).stem}_{Path(tex_name).stem}"
                        f"_a{amp}_t{int(t)}")
                out = GOLDEN_DIR / case
                out.mkdir(parents=True, exist_ok=True)

                np.save(out / "q.npy", np.stack([tri.q0, tri.q1, tri.q2]))
                np.save(out / "m.npy", np.stack([tri.m0, tri.m1, tri.m2]))
                np.save(out / "values.npy", np.asarray(values, dtype=np.float64))
                np.save(out / "scale.npy", np.array([scale]))
                np.save(out / "X.npy", X)
                np.save(out / "Y.npy", Y)
                for name in ["h", "hu", "hv", "G00", "G01", "G11",
                             "det", "sqrt_det", "lam_min", "lam_max"]:
                    np.save(out / f"{name}.npy", f[name])
                np.save(out / "n.npy", f["n"])
                n_cases += 1

    print(f"wrote {n_cases} cases to {GOLDEN_DIR}")


if __name__ == "__main__":
    main()
