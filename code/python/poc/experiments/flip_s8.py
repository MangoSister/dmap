"""FLIP scores for the S8 renders (plan phase S8).

Usage:
    python flip_s8.py <render_task_dir>

Reads the EXR images written by the render_displaced_emitter task, computes
the HDR-FLIP mean error of every test image against its viewpoint's
reference, writes flip.csv into the task directory, and saves each FLIP
error map (magma colormap) as a PNG next to it. Prints the table.
"""

import csv
import sys
from pathlib import Path

import flip_evaluator
import imageio.v3 as iio
import numpy as np


def main():
    task_dir = Path(sys.argv[1])
    rows = []
    for vp in ["cam1", "cam2"]:
        ref = task_dir / f"{vp}_reference.exr"
        if not ref.exists():
            continue
        tests = sorted(p for p in task_dir.glob(f"{vp}_*.exr") if p != ref)
        for test in tests:
            err_map, mean_err, _ = flip_evaluator.evaluate(str(ref), str(test), "HDR")
            rows.append((vp, test.stem.removeprefix(f"{vp}_"), mean_err))
            iio.imwrite(test.with_name(test.stem + "_flip.png"),
                        (np.asarray(err_map) * 255).astype(np.uint8))

    with open(task_dir / "flip.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["viewpoint", "image", "mean_flip"])
        w.writerows(rows)

    for vp, name, err in rows:
        print(f"{vp}  {name:28s}  mean FLIP {err:.4f}")
    print(f"wrote {task_dir / 'flip.csv'}")


if __name__ == "__main__":
    main()
