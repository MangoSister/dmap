"""Experiment 10 -- Does bound tightness depend on the chart?

Raised 2026-09-06. The Phase 1 tightness study (exp05) used the identity
chart: barycentric coordinates are the texture coordinates. Real assets map
each triangle into the texture by an arbitrary affine chart. The pointwise
formulas are covariant under that map, and the pyramid is a property of the
texture alone, so nothing in the method changes in real arithmetic. But an
interval enclosure is not invariant under a linear change of variables:
under a skewed chart the metric entries become anisotropic, det G =
G00 G11 - G01^2 cancels more, and the certified intervals could widen. That
would make the Phase 1 kill criterion chart-dependent. This experiment
measures it.

Setup: a chart is the constant Jacobian J = d(u, v)/d(s, t). The base
triangle parameterized by (s, t) is P(J (s, t)), M(J (s, t)); since P and M
are affine, that is again a Triangle, with edges [e1 e2] J and normal
derivatives [Mu Mv] J, built exactly by construction. Same texture on the
(s, t) tile, same base plane, same physical amplitude (the scale comes from
the original triangle's mean edge). exp05's machinery then measures
certified width / true range per level and quantity, for each chart.

Charts, all with |det J| = 1 so the cell footprint keeps its area and only
its shape changes: identity; anisotropic scale at condition number 2 and 4;
rotation by 30 degrees; shear at condition number 2.6; a mirror (det -1).

Verdict: conservativeness must hold under every chart (a violation means
the propagation is not chart-generic), and the worst median ratio at the
criterion levels must stay within the Phase 1 kill ratio for every chart on
typical content, as it did for the identity chart (2.77).
"""

import time

import numpy as np

from _common import DATA_DIR, verdict
from dmapref.displacement import downsample_box, load_texture
from dmapref.mesh import Triangle, load_obj
from exp05_pyramid_tightness import (CRITERION_TEXELS, KILL_RATIO, MIN_KEPT,
                                     QUANTITIES, RANGE_FLOOR, mean_edge,
                                     triangle_tightness)

TEX_NODES = 257
N_TRIANGLES = 5
AMPLITUDES = [0.05, 0.2]
ASSETS = [("cc_torus.obj", "disp_rock.png"),
          ("cc_torus.obj", "disp_cobble.png")]
PRIMARY = ["sqrt_det", "lam_max", "cone"]

c30, s30 = np.cos(np.radians(30.0)), np.sin(np.radians(30.0))
CHARTS = [
    ("identity", np.eye(2)),
    ("scale 2:1", np.diag([np.sqrt(2.0), 1.0 / np.sqrt(2.0)])),
    ("scale 4:1", np.diag([2.0, 0.5])),
    ("rotate 30", np.array([[c30, -s30], [s30, c30]])),
    ("shear 1.0", np.array([[1.0, 1.0], [0.0, 1.0]])),
    ("mirror", np.diag([1.0, -1.0])),
]


def reparameterized(tri, J):
    """The same base triangle with parameters (s, t) related to (u, v) by
    (u, v) = J (s, t). P and M are affine, so the result is a Triangle whose
    edges are [e1 e2] J and whose normal derivatives are [Mu Mv] J."""
    E = np.stack([tri.e1, tri.e2], axis=1) @ J
    D = np.stack([tri.Mu, tri.Mv], axis=1) @ J
    return Triangle(tri.q0, tri.q0 + E[:, 0], tri.q0 + E[:, 1],
                    tri.m0, tri.m0 + D[:, 0], tri.m0 + D[:, 1])


def main():
    rng = np.random.default_rng(10)
    t_start = time.perf_counter()
    textures = {name: downsample_box(load_texture(DATA_DIR / name), TEX_NODES)
                for name in sorted({t for _, t in ASSETS})}
    meshes = {name: load_obj(DATA_DIR / name)
              for name in sorted({m for m, _ in ASSETS})}
    tri_picks = {name: rng.choice(mesh.n_triangles, N_TRIANGLES, replace=False)
                 for name, mesh in meshes.items()}

    n_levels = (TEX_NODES - 1).bit_length()
    texels = [2 ** k for k in range(n_levels)]
    criterion = [k for k, t in enumerate(texels)
                 if CRITERION_TEXELS[0] <= t <= CRITERION_TEXELS[1]]

    # (chart, mesh, tex, amp, level, q) -> median ratio; violations per chart.
    med = {}
    viol = {name: 0 for name, _ in CHARTS}
    for chart_name, J in CHARTS:
        cond = np.linalg.cond(J)
        for mesh_name, tex_name in ASSETS:
            for amp in AMPLITUDES:
                acc = [{q: {"ratio": [], "n": 0, "kept": 0} for q in QUANTITIES}
                       for _ in range(n_levels)]
                for t in tri_picks[mesh_name]:
                    tri0 = meshes[mesh_name].triangle(int(t))
                    scale = amp * mean_edge(tri0)      # physical amplitude
                    tri = reparameterized(tri0, J)
                    rows, q_scale = triangle_tightness(
                        tri, textures[tex_name], scale)
                    for level, row in enumerate(rows):
                        for q in QUANTITIES:
                            w_c = row[q]["w_cert"].ravel()
                            w_t = row[q]["w_true"].ravel()
                            keep = w_t >= RANGE_FLOOR * q_scale[q]
                            a = acc[level][q]
                            a["ratio"].append(w_c[keep] / w_t[keep])
                            a["n"] += w_c.size
                            a["kept"] += int(keep.sum())
                            viol[chart_name] += row[q]["viol"]
                for level in range(n_levels):
                    for q in QUANTITIES:
                        a = acc[level][q]
                        r = np.concatenate(a["ratio"])
                        med[(chart_name, mesh_name, tex_name, amp, level, q)] = (
                            float(np.median(r)) if r.size else np.nan,
                            a["kept"] / a["n"])
        print(f"\nchart {chart_name} (condition {cond:.2f}, det {np.linalg.det(J):+.0f}):"
              f" violations {viol[chart_name]}")
        for mesh_name, tex_name in ASSETS:
            for amp in AMPLITUDES:
                parts = []
                for q in QUANTITIES:
                    vals = [med[(chart_name, mesh_name, tex_name, amp, k, q)][0]
                            for k in criterion]
                    parts.append(f"{q} " + "/".join(f"{v:4.2f}" for v in vals))
                print(f"  {tex_name} amp {amp}: " + "   ".join(parts)
                      + f"   (cells {'/'.join(str(texels[k]) for k in criterion)} texels)")

    # Summary: worst median at criterion levels on typical content, per chart,
    # and its ratio to the identity chart's.
    print("\nworst median ratio at criterion levels, 0.05x amplitude, over "
          f"{'/'.join(PRIMARY)}:")
    worst = {}
    for chart_name, J in CHARTS:
        w, where = 0.0, ""
        for (cn, mn, tn, amp, level, q), (m, kept) in med.items():
            if (cn != chart_name or amp != 0.05 or level not in criterion
                    or q not in PRIMARY or kept < MIN_KEPT):
                continue
            if m > w:
                w, where = m, f"{q} on {tn}, {texels[level]}-texel cells"
        worst[chart_name] = w
        print(f"  {chart_name:10s} {w:5.2f}  ({where})"
              f"   x{w / worst['identity']:.2f} of identity")
    total_viol = sum(viol.values())
    worst_all = max(worst.values())
    print(f"\nconservativeness violations over all charts: {total_viol}")
    print(f"elapsed: {time.perf_counter() - t_start:.1f} s")
    verdict(total_viol == 0 and worst_all <= KILL_RATIO,
            f"bounds conservative under every chart and within {KILL_RATIO}x of "
            f"the true range at cells up to {CRITERION_TEXELS[1]} texels for "
            f"every chart (worst median {worst_all:.2f}, identity {worst['identity']:.2f})"
            if worst_all <= KILL_RATIO else
            f"kill criterion is chart-dependent: worst median {worst_all:.2f} > "
            f"{KILL_RATIO} under a skewed chart (identity {worst['identity']:.2f})")


if __name__ == "__main__":
    main()
