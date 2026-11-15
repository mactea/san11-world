"""Export sea-only map paths for the offline shipping calibration (no game launch).

City-to-shore access, port permissions and the Egyptian land transfer are excluded.
Run from the repository root; the output is consumed by sea_route_test.exe.
"""
import argparse
import heapq
import json
from pathlib import Path

import numpy as np


def distance(a, b):
    x, y = a
    u, v = b
    dy = (y - x // 2) - (v - u // 2)
    return max(abs(x - u), abs(dy), abs(x - u + dy))


def neighbors(p):
    x, y = p
    d = 1 if x % 2 else -1
    return ((x, y - 1), (x, y + 1), (x - 1, y), (x + 1, y),
            (x - 1, y + d), (x + 1, y + d))


def sea_path(terrain, start, end):
    rows, cols = terrain.shape
    queue = [(distance(start, end), 0, start)]
    best, previous = {start: 0}, {}
    while queue:
        _, steps, p = heapq.heappop(queue)
        if steps != best[p]:
            continue
        if p == end:
            path = [p]
            while p != start:
                p = previous[p]
                path.append(p)
            return path[::-1]
        for q in neighbors(p):
            x, y = q
            if (0 <= x < rows and 0 <= y < cols and terrain[q] == 8
                    and steps + 1 < best.get(q, float("inf"))):
                best[q], previous[q] = steps + 1, p
                heapq.heappush(queue, (steps + 1 + distance(q, end), steps + 1, q))
    raise ValueError(f"No sea-only path: {start} -> {end}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    world = root / "data/world"
    terrain = np.load(world / "world_types.npy")
    cities = json.loads((world / "cities_world.json").read_text(encoding="utf-8"))
    points = {c["name"]: (c["lo"], c["hi"]) for c in cities}
    info = json.loads((world / "world_info.json").read_text(encoding="utf-8"))
    china = json.loads((root / "data/cities_georef.json").read_text(encoding="utf-8"))
    beihai = next(c for c in china if c["id"] == 7)
    points["北海"] = (beihai["lo"] + info["china_lo"], beihai["hi"] + info["china_hi"])
    rows, cols = terrain.shape

    def shore(p):
        candidates = ((x, y) for x in range(max(0, p[0] - 35), min(rows, p[0] + 36))
                      for y in range(max(0, p[1] - 35), min(cols, p[1] + 36))
                      if terrain[x, y] == 8)
        return min(candidates, key=lambda q: distance(p, q))

    stops = ["北海", "番禺", "扶南", "馬六", "穆濟", "貝列"]
    pairs = list(zip(stops, stops[1:])) + [("黎軒", "羅馬")]
    paths, metadata = [], []
    for a, b in pairs:
        start, end = shore(points[a]), shore(points[b])
        path = sea_path(terrain, start, end)
        assert all(terrain[p] == 8 for p in path)
        assert all(distance(p, q) == 1 for p, q in zip(path, path[1:]))
        paths.append(path)
        metadata.append(dict(origin=a, destination=b, sea_steps=len(path) - 1,
                             shore_offsets=[distance(points[a], start), distance(points[b], end)]))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="ascii") as out:
        out.write(f"{cols} {rows} {len(paths)}\n")
        for path in paths:
            out.write(f"{len(path)}\n")
            out.writelines(f"{x} {y}\n" for x, y in path)
    args.output.with_suffix(".json").write_text(json.dumps(dict(
        method="Sea-terrain-only shortest paths; not native navigation; no port or land access validation",
        legs=metadata), ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Exported {len(paths)} legs, {sum(len(p) - 1 for p in paths)} sea steps")


if __name__ == "__main__":
    main()
