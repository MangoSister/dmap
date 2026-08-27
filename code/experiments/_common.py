"""Shared experiment plumbing: import path, output dir, verdict printing."""

import sys
from pathlib import Path

CODE_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CODE_DIR))

OUT_DIR = CODE_DIR / "experiments" / "out"
OUT_DIR.mkdir(parents=True, exist_ok=True)

DATA_DIR = CODE_DIR / "data" / "simple"


def verdict(passed, message):
    print(f"\nVERDICT: {'PASS' if passed else 'FAIL'} — {message}")
