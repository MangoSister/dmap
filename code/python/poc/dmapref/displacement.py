"""Displacement sources: textures from disk and synthetic analytic fields.

Two kinds of displacement field are used in Phase 0:
- textures (PNG height maps), evaluated through an interpolant (interpolant.py);
- synthetic fields with closed-form h and gradient, for ground-truth tests.

A displacement field object exposes h(x, y) and grad(x, y) over the unit
square. Following the project's per-face parameterization, each triangle
gets the unit square as its own tile; the triangle's (u, v) coordinates are
used directly as texture coordinates (identity chart).
"""

import imageio.v3 as iio
import numpy as np


def load_texture(path):
    """Load an image as a float64 height field in [0, 1], shape (H, W).

    Color images are averaged to one channel. Row 0 is the top of the image;
    we treat the array as values on a regular grid over the unit square with
    y increasing along rows.
    """
    raw = iio.imread(path)
    img = np.asarray(raw, dtype=np.float64)
    if img.ndim == 3:
        img = img[..., :3].mean(axis=2)
    if np.issubdtype(raw.dtype, np.integer):
        img = img / np.iinfo(raw.dtype).max
    return img


def downsample_box(values, n):
    """Box-filter a (H, W) grid down to (n, n). H and W are cropped to the
    nearest multiple of n first."""
    H, W = values.shape
    h_crop, w_crop = (H // n) * n, (W // n) * n
    v = values[:h_crop, :w_crop]
    return v.reshape(n, h_crop // n, n, w_crop // n).mean(axis=(1, 3))


class SyntheticField:
    """Analytic displacement with exact gradient, for ground-truth tests."""

    def __init__(self, h_fn, grad_fn):
        self._h = h_fn
        self._grad = grad_fn

    def h(self, x, y):
        return self._h(x, y)

    def grad(self, x, y):
        """Returns (dh/dx, dh/dy) as a length-2 array (or two arrays)."""
        return self._grad(x, y)


def constant(c):
    return SyntheticField(
        lambda x, y: np.full_like(np.asarray(x, dtype=np.float64), c),
        lambda x, y: np.array([0.0, 0.0]),
    )


def ramp(gx, gy, c=0.0):
    return SyntheticField(
        lambda x, y: c + gx * x + gy * y,
        lambda x, y: np.array([gx, gy]),
    )


def sinusoid(amp=0.3, fx=2.0, fy=3.0, px=1.0, py=-1.0):
    """h = amp * sin(fx*x + px) * cos(fy*y + py)."""
    return SyntheticField(
        lambda x, y: amp * np.sin(fx * x + px) * np.cos(fy * y + py),
        lambda x, y: np.array([
            amp * fx * np.cos(fx * x + px) * np.cos(fy * y + py),
            -amp * fy * np.sin(fx * x + px) * np.sin(fy * y + py),
        ]),
    )
