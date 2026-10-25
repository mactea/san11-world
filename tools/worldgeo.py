"""World hex grid <-> geography for the San11PK world map.

Hex coordinates follow the game: a position is (lo, hi) where `lo` is the array row (low 16 bits,
grows EAST) and `hi` is the column (high 16 bits, grows SOUTH). Odd rows are shifted half a hex
towards +hi. The world has ROWS x COLS hexes; the stock 200x200 China map is embedded at
(CHINA_LO, CHINA_HI). worldmod.ini: height=ROWS, width=COLS, china_y=CHINA_LO, china_x=CHINA_HI.

Outside China the grid is a plain equirectangular projection at the game's own scale
(about 9 km per hex). Koei's China is close to linear but sheared and locally distorted, so a
smooth displacement field, fitted to the 42 city seats, bends real geography so that it meets Koei's
coastlines and rivers at the edge of the China block.

The field (a thin-plate spline through the 42 seats) is used as it is inside the block. Outside it, the
spline's own extrapolation grows to 100+ hexes a few hundred hexes out, so it is replaced by the value on the
block's edge (at the nearest edge point) faded to zero over CORE + FADE hexes; within CORE + SEAM hexes of the block the spline
itself is kept, then blended in, so the field and its first derivatives are continuous across the edge (no seam). Beyond
CORE + FADE hexes the grid is the plain projection (M11_MAP.md, "投影").
"""
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy.interpolate import RBFInterpolator

DLON = 0.0969          # degrees of longitude per row (fitted from the 42 cities)
DLAT = 0.0825          # degrees of latitude per column


@dataclass(frozen=True)
class WorldSpec:
    # Default world: Eurasia + North Africa (15W .. 159E, 77N .. 5.5S)
    rows: int = 1800       # east-west, multiple of 200
    cols: int = 1000       # north-south, multiple of 200
    lon0: float = -15.07   # longitude of row 0
    lat0: float = 77.0     # latitude of column 0
    china_lo: int = 1234   # even, keeps the odd-row hex parity of the China block
    china_hi: int = 436

    def linear_to_lonlat(self, lo, hi):
        lo = np.asarray(lo, dtype=np.float64)
        hi = np.asarray(hi, dtype=np.float64) + 0.5 * (np.asarray(lo, dtype=np.int64) & 1)
        return self.lon0 + DLON * lo, self.lat0 - DLAT * hi

    def lonlat_to_linear(self, lon, lat):
        return (np.asarray(lon) - self.lon0) / DLON, (self.lat0 - np.asarray(lat)) / DLAT


def _smoothstep(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


class WorldGeo:
    """Maps world hexes to (lon, lat), bending geography around Koei's China."""

    CORE = 8.0          # hexes beyond the block edge where the spline is kept as it is
    SEAM = 30.0         # hexes beyond CORE over which the spline itself is blended out
    FADE = 150.0        # hexes beyond CORE where the displacement reaches zero
    VERSION = "edge-carry 8/30/150"   # part of make_world.py's hex elevation cache key

    def __init__(self, spec: WorldSpec, georef: Path):
        self.spec = spec
        cities = json.loads(Path(georef).read_text(encoding="utf-8"))
        koei = np.array([[c["lo"] + spec.china_lo, c["hi"] + spec.china_hi] for c in cities], dtype=float)
        lin = np.stack(spec.lonlat_to_linear([c["lon"] for c in cities], [c["lat"] for c in cities]), axis=1)
        # displacement (in hexes) that carries a linear position onto Koei's position
        self.rbf = RBFInterpolator(koei, koei - lin, kernel="thin_plate_spline", degree=1, smoothing=2.0)
        self.residual_rms = float(np.sqrt(((self.rbf(koei) - (koei - lin)) ** 2).sum(1).mean()))

    def _box(self, lo, hi):
        """(distance to the China block, nearest point of the block)"""
        s = self.spec
        plo = np.clip(lo, s.china_lo, s.china_lo + 199)
        phi = np.clip(hi, s.china_hi, s.china_hi + 199)
        return np.hypot(lo - plo, hi - phi), plo, phi

    def disp(self, lo, hi):
        """displacement field (n, 2) in hexes at world positions lo, hi (1-d arrays): hex = linear + disp"""
        lo = np.asarray(lo, dtype=np.float64)
        hi = np.asarray(hi, dtype=np.float64)
        d, plo, phi = self._box(lo, hi)
        out = np.zeros((lo.size, 2))
        near = d < self.CORE + self.FADE
        if not near.any():
            return out
        x = np.stack([lo[near], hi[near]], 1)
        dn = np.maximum(d[near] - self.CORE, 0)
        edge = self.rbf(np.stack([plo[near], phi[near]], 1))
        w = 1 - _smoothstep(dn / self.SEAM)
        own = dn < self.SEAM
        if own.any():
            edge[own] = w[own, None] * self.rbf(x[own]) + (1 - w[own, None]) * edge[own]
        out[near] = edge * (1 - _smoothstep(dn / self.FADE))[:, None]
        return out

    def hex_to_lonlat(self, lo, hi, step=2):
        """(lon, lat) for arrays of world hex coordinates. The displacement field is evaluated on a
        lattice (every `step` hexes) and interpolated, which is accurate to well under a hex (0.3 at most
        next to the block, where the field bends most).
        The field is taken at the hex's geometric position (lo, hi + 0.5 * (lo & 1)), as make_world.py's
        lonlat_to_hex inverts it (lo may be fractional for sub-hex samples: its nearest row's shift)."""
        lo = np.asarray(lo, dtype=np.float64)
        x = np.asarray(hi, dtype=np.float64) + 0.5 * (np.round(lo).astype(np.int64) & 1)
        glo = np.arange(lo.min() // step * step, lo.max() + 2 * step, step)
        gx = np.arange(x.min() // step * step, x.max() + 2 * step, step)
        GL, GX = np.meshgrid(glo, gx, indexing="ij")
        disp = self.disp(GL.ravel(), GX.ravel()).reshape(len(glo), len(gx), 2)
        # bilinear interpolation of the displacement at the requested hexes
        fi = (lo - glo[0]) / step
        fj = (x - gx[0]) / step
        i0 = np.clip(fi.astype(int), 0, len(glo) - 2)
        j0 = np.clip(fj.astype(int), 0, len(gx) - 2)
        a = (fi - i0)[..., None]
        b = (fj - j0)[..., None]
        d = (disp[i0, j0] * (1 - a) * (1 - b) + disp[i0 + 1, j0] * a * (1 - b)
             + disp[i0, j0 + 1] * (1 - a) * b + disp[i0 + 1, j0 + 1] * a * b)
        return self.spec.lon0 + DLON * (lo - d[..., 0]), self.spec.lat0 - DLAT * (x - d[..., 1])


if __name__ == "__main__":
    import sys
    g = WorldGeo(WorldSpec(), Path(sys.argv[1] if len(sys.argv) > 1 else "data/cities_georef.json"))
    print(f"control-point fit residual: {g.residual_rms:.2f} hexes")
    cities = json.loads(Path(sys.argv[1] if len(sys.argv) > 1 else "data/cities_georef.json").read_text(encoding="utf-8"))
    lo = np.array([c["lo"] + g.spec.china_lo for c in cities])
    hi = np.array([c["hi"] + g.spec.china_hi for c in cities])
    lon, lat = g.hex_to_lonlat(lo, hi)
    err = np.hypot((lon - [c["lon"] for c in cities]) * np.cos(np.radians(lat)), lat - [c["lat"] for c in cities]) * 111
    print(f"city placement error: mean {err.mean():.1f} km, max {err.max():.1f} km")
