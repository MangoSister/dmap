"""Writes the regression scene of the path tracer plan (T7) into
code/data/scenes/assets/regression_s4_base.obj: scene S4 with its displaced
objects replaced by their base meshes, every placement baked into one OBJ
with one shape per object, so that ks's unchanged `small_pt`, which takes a
single mesh asset, renders it. The shapes, in order: floor, torus_a,
torus_b, emitter, lamp, with the transforms of scenes/s4_mixed.toml (ks
composes scale, then the XYZ Euler rotation, then the translation).

Run: python make_regression_scene.py
"""

import os

import numpy as np

from make_scene_assets import load_obj

DATA = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "data"))
OUT = os.path.join(DATA, "scenes", "assets", "regression_s4_base.obj")


def rotation_xyz(euler_deg):
    a, b, c = np.radians(euler_deg)
    rx = np.array([[1, 0, 0], [0, np.cos(a), -np.sin(a)], [0, np.sin(a), np.cos(a)]])
    ry = np.array([[np.cos(b), 0, np.sin(b)], [0, 1, 0], [-np.sin(b), 0, np.cos(b)]])
    rz = np.array([[np.cos(c), -np.sin(c), 0], [np.sin(c), np.cos(c), 0], [0, 0, 1]])
    return rx @ ry @ rz


def placement(scale=(1, 1, 1), euler=(0, 0, 0), translation=(0, 0, 0)):
    m = np.eye(4)
    m[:3, :3] = rotation_xyz(euler) @ np.diag(scale)
    m[:3, 3] = translation
    return m


PLACEMENTS = [
    ("floor", "scenes/assets/plane.obj", placement(scale=(6.0, 1.0, 6.0))),
    ("torus_a", "simple/cc_torus.obj", placement(translation=(-1.8, 0.5, 0.0))),
    ("torus_b", "simple/cc_torus.obj", placement(euler=(0.0, 30.0, 0.0), translation=(1.8, 0.5, 0.0))),
    ("emitter", "scenes/assets/plane.obj", placement(scale=(0.8, 1.0, 0.8), euler=(180.0, 0.0, 0.0), translation=(0.0, 2.5, 0.0))),
    ("lamp", "scenes/assets/plane.obj", placement(scale=(0.5, 1.0, 0.5), euler=(150.0, 0.0, 0.0), translation=(0.0, 3.0, -2.5))),
]


def write():
    v_base, vt_base, vn_base = 0, 0, 0
    with open(OUT, "w") as f:
        f.write("# Scene S4 without displacement, every placement baked (T7 regression scene)\n")
        for name, asset, m in PLACEMENTS:
            v, vt, vn, faces = load_obj(os.path.join(DATA, asset))
            normal_matrix = np.linalg.inv(m[:3, :3]).T
            f.write("o %s\n" % name)
            for q in v:
                p = m[:3, :3] @ q + m[:3, 3]
                f.write("v %.9g %.9g %.9g\n" % tuple(p))
            for tc in vt:
                f.write("vt %.9g %.9g\n" % tuple(tc))
            for q in vn:
                n = normal_matrix @ q
                n = n / np.linalg.norm(n)
                f.write("vn %.9g %.9g %.9g\n" % tuple(n))
            for tri in faces:
                f.write("f")
                for vi, ti, ni in tri:
                    f.write(" %d/%d/%d" % (vi + 1 + v_base, ti + 1 + vt_base, ni + 1 + vn_base))
                f.write("\n")
            v_base += len(v)
            vt_base += len(vt)
            vn_base += len(vn)
            print("%s: %d vertices, %d faces" % (name, len(v), len(faces)))
    print("written", OUT)


if __name__ == "__main__":
    write()
