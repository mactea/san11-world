"""Köppen-Geiger climate classes for make_world.py's land cover (M11_MAP.md, "气候").

Source: Beck et al. (2023), High-resolution (1 km) Köppen-Geiger maps for 1901-2099, Scientific Data 10, 724,
doi:10.1038/s41597-023-02549-6 (CC BY 4.0), period 1901-1930 (the earliest, closest to the ancient climate), as
the 0.1-degree map reprojected to Web Mercator (EPSG:3857, 4096 x 4096, nearest neighbour) by
github.com/dropbop/koppen (public/data/cogs/1901-1930.tif). Classes 1..30 in Beck's order (KG_CLASSES), 0 = sea.

    python koppen.py            data/raw/koppen/koppen_1901-1930_3857.tif -> data/koppen_1901_1930_0p1.npz

The .npz (committed) holds the classes on a 0.1-degree latitude/longitude grid covering the world map (STEP,
LON0, LAT0: cell centres LON0 + (j + 0.5) * STEP, LAT0 - (i + 0.5) * STEP); make_world.py reads only it.
"""
import argparse
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "data"
TIF = DATA / "raw" / "koppen" / "koppen_1901-1930_3857.tif"
URL = "https://raw.githubusercontent.com/dropbop/koppen/main/public/data/cogs/1901-1930.tif"
NPZ = DATA / "koppen_1901_1930_0p1.npz"
STEP, LON0, LON1, LAT0, LAT1 = 0.1, -20.0, 165.0, 80.0, -10.0
KG_CLASSES = ["", "Af", "Am", "Aw", "BWh", "BWk", "BSh", "BSk", "Csa", "Csb", "Csc", "Cwa", "Cwb", "Cwc", "Cfa", "Cfb",
              "Cfc", "Dsa", "Dsb", "Dsc", "Dsd", "Dwa", "Dwb", "Dwc", "Dwd", "Dfa", "Dfb", "Dfc", "Dfd", "ET", "EF"]
MERC = 20037508.342789244


def convert(tif=TIF, out=NPZ):
    import tifffile
    with tifffile.TiffFile(tif) as t:
        p = t.pages[0]
        a = p.asarray()
        scale = p.tags["ModelPixelScaleTag"].value[0]
        tie = p.tags["ModelTiepointTag"].value
    x0, y0 = tie[3], tie[4]
    lat = LAT0 - (np.arange(round((LAT0 - LAT1) / STEP)) + 0.5) * STEP
    lon = LON0 + (np.arange(round((LON1 - LON0) / STEP)) + 0.5) * STEP
    y = MERC / np.pi * np.log(np.tan(np.pi / 4 + np.radians(lat) / 2))
    x = MERC / 180.0 * lon
    rows = np.clip(np.floor((y0 - y) / scale).astype(int), 0, a.shape[0] - 1)
    cols = np.clip(np.floor((x - x0) / scale).astype(int), 0, a.shape[1] - 1)
    k = a[rows[:, None], cols[None, :]].astype(np.uint8)
    np.savez_compressed(out, k=k, step=STEP, lon0=LON0, lat0=LAT0)
    return k


def load(path=NPZ):
    z = np.load(path)
    return z["k"], float(z["step"]), float(z["lon0"]), float(z["lat0"])


def classes_at(lon, lat, grid=None):
    """Köppen class (0..30) at each lon/lat (nearest 0.1-degree cell)"""
    k, step, lon0, lat0 = grid or load()
    i = np.clip(np.floor((lat0 - np.asarray(lat)) / step).astype(int), 0, k.shape[0] - 1)
    j = np.clip(np.floor((np.asarray(lon) - lon0) / step).astype(int), 0, k.shape[1] - 1)
    return k[i, j]


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("tif", nargs="?", default=str(TIF))
    ap.add_argument("--out", default=str(NPZ))
    a = ap.parse_args()
    k = convert(Path(a.tif), Path(a.out))
    print(f"{a.out}: {k.shape}, classes {np.unique(k).tolist()}, {Path(a.out).stat().st_size >> 10} KB")
