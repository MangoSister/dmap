"""Batched affine arithmetic over a fixed symbol set, plus interval helpers.

An affine form x = x0 + sum_k c_k*eps_k + rad*eps_priv represents every value
reachable with |eps| <= 1. The five named symbols are shared between forms,
which is what preserves correlation ("Taylor-model bound pyramid" §2, §6):
the same eps drawn in two terms cancels instead of compounding. `rad`
collects residues of nonlinear operations; each form's private symbol is
never shared, so a plain radius suffices.

All fields are numpy arrays with a common batch shape, so one AffineForm can
hold a quantity for every cell of a pyramid level at once.

Intervals are (lo, hi) tuples of arrays. They are the output format of the
bound propagation and the arithmetic of the interval ablations.
"""

import numpy as np

N_SYMBOLS = 5
EPS_U, EPS_V, EPS_H, EPS_GU, EPS_GV = range(N_SYMBOLS)


class AffineForm:

    def __init__(self, x0, coeffs=None, rad=None):
        self.x0 = np.asarray(x0, dtype=np.float64)
        if coeffs is None:
            coeffs = np.zeros((N_SYMBOLS,) + self.x0.shape)
        self.coeffs = np.asarray(coeffs, dtype=np.float64)
        if rad is None:
            rad = np.zeros_like(self.x0)
        self.rad = np.asarray(rad, dtype=np.float64)

    @classmethod
    def from_symbol(cls, x0, symbol_coeffs):
        """Form x0 + sum of the given {symbol: coefficient} terms."""
        x0 = np.asarray(x0, dtype=np.float64)
        coeffs = np.zeros((N_SYMBOLS,) + x0.shape)
        for k, c in symbol_coeffs.items():
            coeffs[k] = c
        return cls(x0, coeffs)

    def dev(self):
        """Total deviation radius: sup |x - x0|."""
        return np.abs(self.coeffs).sum(axis=0) + self.rad

    def interval(self):
        d = self.dev()
        return self.x0 - d, self.x0 + d

    def abs_max(self):
        return np.abs(self.x0) + self.dev()

    def __add__(self, other):
        if isinstance(other, AffineForm):
            return AffineForm(self.x0 + other.x0, self.coeffs + other.coeffs,
                              self.rad + other.rad)
        return AffineForm(self.x0 + other, self.coeffs.copy(), self.rad.copy())

    __radd__ = __add__

    def __neg__(self):
        return AffineForm(-self.x0, -self.coeffs, self.rad.copy())

    def __sub__(self, other):
        return self + (-other if isinstance(other, AffineForm) else -np.asarray(other))

    def __rsub__(self, other):
        return (-self) + other

    def scale(self, c):
        """Multiply by an exact constant (scalar or batch array)."""
        c = np.asarray(c, dtype=np.float64)
        return AffineForm(c * self.x0, c * self.coeffs, np.abs(c) * self.rad)

    def __mul__(self, other):
        """Affine product. The linear part is exact; the cross terms of the
        deviations go into rad as dev(x)*dev(y) (standard affine
        multiplication), which is what keeps shared symbols cancelling in
        sums like det G = G11*G22 - G12^2."""
        if not isinstance(other, AffineForm):
            return self.scale(other)
        x0, y0 = self.x0, other.x0
        coeffs = x0 * other.coeffs + y0 * self.coeffs
        rad = (np.abs(x0) * other.rad + np.abs(y0) * self.rad
               + self.dev() * other.dev())
        return AffineForm(x0 * y0, coeffs, rad)

    __rmul__ = __mul__

    def square(self):
        """x*x with the quadratic term anchored at the midpoint of its
        range: the deviation square lies in [0, dev^2], so anchoring at
        dev^2/2 with radius dev^2/2 halves the residue of the generic
        product."""
        d = self.dev()
        coeffs = 2.0 * self.x0 * self.coeffs
        rad = 2.0 * np.abs(self.x0) * self.rad + 0.5 * d * d
        return AffineForm(self.x0 * self.x0 + 0.5 * d * d, coeffs, rad)

    def times_interval(self, center, radius):
        """Product with an interval constant c ± r: exact scale by the center
        plus |x|_max * r into rad."""
        z = self.scale(center)
        z.rad = z.rad + self.abs_max() * np.abs(radius)
        return z


def affine_sqrt(x):
    """Affine square root by secant linearization (standard affine
    arithmetic; TFDM uses the same device for 1/sqrt).

    For z in [lo, hi] with 0 <= lo <= hi, writing a = sqrt(lo), b = sqrt(hi):
    the secant s(z) = a + (z - lo)/(a + b) underestimates sqrt with maximum
    error e = (b - a)^2 / (4 (a + b)), so sqrt(z) = s(z) + e/2 ± e/2. The
    result keeps x's symbols, which is the point: downstream sums like
    T - sqrt(disc) cancel the shared linear parts instead of adding widths.

    Valid for every realized value of x that is nonnegative; the caller
    guarantees realizability (a negative interval floor is clipped)."""
    lo, hi = x.interval()
    a = np.sqrt(np.maximum(lo, 0.0))
    b = np.sqrt(np.maximum(hi, 0.0))
    denom = a + b
    ok = denom > 1e-300
    alpha = np.where(ok, 1.0 / np.where(ok, denom, 1.0), 0.0)
    e = np.where(ok, (b - a) ** 2 / np.where(ok, 4.0 * denom, 1.0), 0.0)
    z = x.scale(alpha)
    z.x0 = z.x0 + (a - alpha * np.maximum(lo, 0.0) + 0.5 * e)
    z.rad = z.rad + 0.5 * e
    return z


# --- intervals: (lo, hi) tuples of arrays ----------------------------------

def iv_const(c):
    c = np.asarray(c, dtype=np.float64)
    return c, c.copy()


def iv_from_cr(center, radius):
    return center - radius, center + radius


def iv_center_rad(iv):
    lo, hi = iv
    return 0.5 * (lo + hi), 0.5 * (hi - lo)


def iv_add(x, y):
    return x[0] + y[0], x[1] + y[1]


def iv_sub(x, y):
    return x[0] - y[1], x[1] - y[0]


def iv_neg(x):
    return -x[1], -x[0]


def iv_scale(x, c):
    """Multiply by an exact constant (scalar or array, any sign)."""
    a, b = c * x[0], c * x[1]
    return np.minimum(a, b), np.maximum(a, b)


def iv_mul(x, y):
    p = np.stack([x[0] * y[0], x[0] * y[1], x[1] * y[0], x[1] * y[1]])
    return p.min(axis=0), p.max(axis=0)


def iv_square(x):
    """Exact interval square (tighter than iv_mul(x, x): [x^2] never dips
    below zero)."""
    lo2, hi2 = x[0] ** 2, x[1] ** 2
    hi = np.maximum(lo2, hi2)
    lo = np.where((x[0] <= 0.0) & (x[1] >= 0.0), 0.0, np.minimum(lo2, hi2))
    return lo, hi


def iv_sqrt(x):
    return np.sqrt(np.maximum(x[0], 0.0)), np.sqrt(np.maximum(x[1], 0.0))


def iv_recip(x, floor=1e-300):
    """1/x for an interval with lo > 0 (clamped at floor)."""
    lo = np.maximum(x[0], floor)
    hi = np.maximum(x[1], floor)
    return 1.0 / hi, 1.0 / lo


def iv_width(x):
    return x[1] - x[0]
