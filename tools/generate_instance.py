#!/usr/bin/env python3
"""Generate a random net in STP format for experiments.

Example:  python3 tools/generate_instance.py 500 --seed 1 --size 200 > net500.txt
"""
import argparse
import random
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("pins", type=int, help="number of pins")
    parser.add_argument("--size", type=float, default=200.0, help="side length of the square (default 200)")
    parser.add_argument("--seed", type=int, default=0, help="random seed (default 0)")
    parser.add_argument("--integer", action="store_true", help="use integer coordinates")
    args = parser.parse_args()
    if args.pins < 1:
        parser.error("the number of pins must be positive")

    rng = random.Random(args.seed)

    def coordinate() -> str:
        value = rng.uniform(0, args.size)
        return str(round(value)) if args.integer else f"{value:.2f}"

    out = sys.stdout
    out.write("Section Terminals\n")
    out.write(f"Terminals {args.pins}\n")
    for pin in range(1, args.pins + 1):
        out.write(f"T {pin}\n")
    out.write("End\n\nSection Coordinates\n")
    for pin in range(1, args.pins + 1):
        out.write(f"DD {pin} {coordinate()} {coordinate()}\n")
    out.write("End\n\nEOF\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
