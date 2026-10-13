#!/usr/bin/env python3
"""Draw a Steiner tree written by `flute solve --output`.

    python3 tools/draw.py result.txt                 # open a window
    python3 tools/draw.py result.txt -o tree.png     # write an image

Pins are black dots, net-breaking points red squares. Requires matplotlib.
"""
import argparse
import sys


def read_result(path):
    pins, breaking, edges = [], [], []
    with open(path, encoding="utf-8") as handle:
        for number, line in enumerate(handle, 1):
            fields = line.split()
            if not fields or fields[0] in ("#", "pins", "edges", "length") or line.startswith("#"):
                continue
            try:
                kind, values = fields[0], [float(v) for v in fields[1:]]
            except ValueError:
                raise SystemExit(f"{path}:{number}: malformed line")
            if kind == "P" and len(values) == 2:
                pins.append(tuple(values))
            elif kind == "B" and len(values) == 2:
                breaking.append(tuple(values))
            elif kind == "E" and len(values) == 4:
                edges.append(tuple(values))
            else:
                raise SystemExit(f"{path}:{number}: unexpected line '{line.strip()}'")
    return pins, breaking, edges


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("result", help="file written by 'flute solve --output'")
    parser.add_argument("-o", "--output", help="write an image instead of opening a window")
    parser.add_argument("--no-grid", action="store_true", help="do not draw the Hanan grid")
    args = parser.parse_args()

    try:
        import matplotlib

        if args.output:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("draw.py needs matplotlib (pip install -r tools/requirements.txt)", file=sys.stderr)
        return 1

    pins, breaking, edges = read_result(args.result)
    if not pins:
        print("the result file contains no pins", file=sys.stderr)
        return 1

    figure, axes = plt.subplots(figsize=(8, 8))
    if not args.no_grid and len(pins) <= 200:
        for x in sorted({p[0] for p in pins}):
            axes.axvline(x, color="0.8", linewidth=0.6, linestyle=":")
        for y in sorted({p[1] for p in pins}):
            axes.axhline(y, color="0.8", linewidth=0.6, linestyle=":")
    for x1, y1, x2, y2 in edges:
        axes.plot([x1, x2], [y1, y2], color="tab:blue", linewidth=1.6, solid_capstyle="round")
    axes.scatter(*zip(*pins), color="black", s=18, zorder=3, label="pins")
    if breaking:
        axes.scatter(*zip(*breaking), color="tab:red", marker="s", s=36, zorder=4, label="breaking points")
    axes.set_aspect("equal", adjustable="datalim")
    axes.legend(loc="best")
    axes.set_title(f"{len(pins)} pins, {len(edges)} edges")

    if args.output:
        figure.savefig(args.output, dpi=150, bbox_inches="tight")
    else:
        plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())
