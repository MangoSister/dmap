"""Shared experiment plumbing: import path, output dir, verdict printing."""

import sys
from pathlib import Path

POC_DIR = Path(__file__).resolve().parents[1]      # code/python/poc
CODE_DIR = POC_DIR.parents[1]                      # code
sys.path.insert(0, str(POC_DIR))

OUT_DIR = POC_DIR / "experiments" / "out"
OUT_DIR.mkdir(parents=True, exist_ok=True)

DATA_DIR = CODE_DIR / "data" / "simple"


def verdict(passed, message):
    print(f"\nVERDICT: {'PASS' if passed else 'FAIL'} — {message}")
