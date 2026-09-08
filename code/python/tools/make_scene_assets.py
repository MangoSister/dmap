"""Writes the small meshes the scene set of the path tracer plan (T2) needs
into code/data/scenes/assets, and prints the numbers the scene files quote.

- s7_triangle.obj: the experiment triangle (index n_triangles // 3 of
  cc_torus.obj under fan triangulation, as the A-MVP validators pick it)
  with its vertex normals and the identity chart as texture coordinates.
- plane.obj: a 2 x 2 quad in the xz plane, normal +y, texture coordinates
  over [0, 1]^2 (u along +x, v along -z).

Run: python make_scene_assets.py
"""

import os

import numpy as np

DATA = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "data"))
OUT = os.path.join(DATA, "scenes", "assets")


def load_obj(path):
    v, vt, vn, faces = [], [], [], []
    with open(path) as f:
        for line in f:
            tok = line.split()
            if not tok:
                continue
            if tok[0] == "v":
                v.append([float(x) for x in tok[1:4]])
            elif tok[0] == "vt":
                vt.append([float(x) for x in tok[1:3]])
            elif tok[0] == "vn":
                vn.append([float(x) for x in tok[1:4]])
            elif tok[0] == "f":
                corners = []
                for c in tok[1:]:
                    parts = c.split("/")
                    vi = int(parts[0]) - 1
                    ti = int(parts[1]) - 1 if len(parts) > 1 and parts[1] else -1
                    ni = int(parts[2]) - 1 if len(parts) > 2 and parts[2] else -1
                    corners.append((vi, ti, ni))
                for k in range(1, len(corners) - 1):  # fan triangulation, as dmap::load_base_obj
                    faces.append((corners[0], corners[k], corners[k + 1]))
    return np.array(v), np.array(vt), np.array(vn), faces


def write_s7_triangle():
    v, vt, vn, faces = load_obj(os.path.join(DATA, "simple", "cc_torus.obj"))
    t = len(faces) // 3
    corners = faces[t]
    p = np.array([v[c[0]] for c in corners])
    if len(vn):
        n = np.array([vn[c[2]] for c in corners])
    else:
        raise SystemExit("cc_torus.obj without vn: area-weighted normals would be needed here")
    n = n / np.linalg.norm(n, axis=1, keepdims=True)
    path = os.path.join(OUT, "s7_triangle.obj")
    with open(path, "w") as f:
        f.write("# triangle %d of cc_torus.obj (fan triangulation), the A-MVP experiment asset\n" % t)
        f.write("o s7_triangle\n")
        for q in p:
            f.write("v %.9g %.9g %.9g\n" % tuple(q))
        for tc in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0)]:
            f.write("vt %g %g\n" % tc)
        for q in n:
            f.write("vn %.9g %.9g %.9g\n" % tuple(q))
        f.write("f 1/1/1 2/2/2 3/3/3\n")
    e1, e2 = p[1] - p[0], p[2] - p[0]
    mean_edge = (np.linalg.norm(e1) + np.linalg.norm(e2) + np.linalg.norm(p[2] - p[1])) / 3
    centroid = p.mean(axis=0)
    t1 = e1 / np.linalg.norm(e1)
    nrm = np.cross(e1, e2)
    nrm /= np.linalg.norm(nrm)
    t2 = np.cross(nrm, t1)
    print("s7_triangle.obj: triangle %d, mean_edge %.9g" % (t, mean_edge))
    print("  strength for amplitude 0.2: %.9g" % (0.2 * mean_edge))
    print("  centroid", centroid)

    def world(local):
        return centroid + mean_edge * (local[0] * t1 + local[1] * t2 + local[2] * nrm)

    # The S8 cameras, in the triangle frame (t1, t2, n) and mean-edge units.
    for name, pos, target in [("cam1", (2.4, 1.5, 1.6), (0.0, 0.0, -0.8)), ("cam2", (2.4, -0.8, -0.5), (0.0, -0.1, -1.0))]:
        print("  %s pos = [%.6g, %.6g, %.6g] target = [%.6g, %.6g, %.6g]" % ((name,) + tuple(world(pos)) + tuple(world(target))))
    print("  normal", nrm)
    return path


def write_plane():
    path = os.path.join(OUT, "plane.obj")
    with open(path, "w") as f:
        f.write("# 2 x 2 quad in the xz plane, normal +y, texture coordinates over [0, 1]^2\n")
        f.write("o plane\n")
        for q in [(-1, 0, 1), (1, 0, 1), (1, 0, -1), (-1, 0, -1)]:
            f.write("v %g %g %g\n" % q)
        for tc in [(0, 0), (1, 0), (1, 1), (0, 1)]:
            f.write("vt %g %g\n" % tc)
        f.write("vn 0 1 0\n")
        f.write("f 1/1/1 2/2/1 3/3/1\n")
        f.write("f 1/1/1 3/3/1 4/4/1\n")
    print("plane.obj written")
    return path


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    write_s7_triangle()
    write_plane()
