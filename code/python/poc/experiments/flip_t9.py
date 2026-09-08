"""FLIP scores for the T9 emitter ladder renders (path tracer plan, T9).

Usage:
    python flip_t9.py <task_dir> [<task_dir> ...]

Each task directory holds reference.exr and one <strategy>_<sampler>.exr
per ladder entry, written by test_emitter_ladder. Computes the HDR-FLIP
mean error of every entry against the reference, writes flip.csv into the
task directory, saves each error map (magma) as a PNG next to it, and
prints the table.
"""

import csv
import sys
from pathlib import Path

import flip_evaluator
import imageio.v3 as iio
import numpy as np


def score(task_dir: Path):
    ref = task_dir / "reference.exr"
    if not ref.exists():
        print(f"{task_dir}: no reference.exr")
        return []
    rows = []
    for test in sorted(task_dir.glob("*.exr")):
        if test == ref:
            continue
        err_map, mean_err, _ = flip_evaluator.evaluate(str(ref), str(test), "HDR")
        rows.append((test.stem, mean_err))
        iio.imwrite(test.with_name(test.stem + "_flip.png"), (np.asarray(err_map) * 255).astype(np.uint8))
    with open(task_dir / "flip.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["image", "mean_flip"])
        w.writerows(rows)
    for name, err in rows:
        print(f"{task_dir.name}  {name:32s}  mean FLIP {err:.4f}")
    return rows


def main():
    for arg in sys.argv[1:]:
        score(Path(arg))


if __name__ == "__main__":
    main()
