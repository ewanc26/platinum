#!/usr/bin/env python3
"""Compare Mac OS 9 fixture screenshots with references, without dependencies.

Screenshots come from QMP ``screendump`` in PPM, so this reads P6 PPM with the
standard library only. PNG is not handled: the harness writes PPM on purpose so
the comparison runs anywhere Python does.

Differences are counted, not judged pixel by pixel: a pixel differs when any
channel moves by more than ``--tolerance``, and a scenario passes while at most
``--allowed-fraction`` of its pixels differ. Regions that cannot be stable --
the pointer, a blinking insertion point, a clock -- are masked with ``--mask``.

References are not committed until they come from a real guest run. Until then
``compare`` reports that it has nothing to compare against and exits 2, which is
"not verified", never a pass.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import sys


def read_ppm(path: pathlib.Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"P6"):
        raise SystemExit(f"{path}: not a binary PPM (P6)")
    fields: list[int] = []
    position = 2
    while len(fields) < 3:
        while position < len(data) and data[position : position + 1].isspace():
            position += 1
        if data[position : position + 1] == b"#":
            while position < len(data) and data[position : position + 1] != b"\n":
                position += 1
            continue
        start = position
        while position < len(data) and not data[position : position + 1].isspace():
            position += 1
        fields.append(int(data[start:position]))
    position += 1
    width, height, maximum = fields
    if maximum != 255:
        raise SystemExit(f"{path}: only 8-bit PPM is supported")
    pixels = data[position : position + width * height * 3]
    if len(pixels) != width * height * 3:
        raise SystemExit(f"{path}: truncated pixel data")
    return width, height, pixels


def masked(width: int, height: int, masks: list[tuple[int, int, int, int]]) -> set[int]:
    covered: set[int] = set()
    for x, y, w, h in masks:
        for row in range(max(0, y), min(height, y + h)):
            for column in range(max(0, x), min(width, x + w)):
                covered.add(row * width + column)
    return covered


def compare_pair(reference: pathlib.Path, actual: pathlib.Path, tolerance: int,
                 masks: list[tuple[int, int, int, int]]) -> dict:
    rw, rh, rp = read_ppm(reference)
    aw, ah, ap = read_ppm(actual)
    if (rw, rh) != (aw, ah):
        return {"same_size": False, "reference": [rw, rh], "actual": [aw, ah]}
    skip = masked(rw, rh, masks)
    differing = 0
    for index in range(rw * rh):
        if index in skip:
            continue
        base = index * 3
        if (abs(rp[base] - ap[base]) > tolerance
                or abs(rp[base + 1] - ap[base + 1]) > tolerance
                or abs(rp[base + 2] - ap[base + 2]) > tolerance):
            differing += 1
    total = rw * rh - len(skip)
    return {
        "same_size": True,
        "pixels": total,
        "differing": differing,
        "fraction": (differing / total) if total else 1.0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--actual", type=pathlib.Path, default=pathlib.Path("macos9-runtime"),
                        help="directory the harness wrote scenario-*.ppm into")
    parser.add_argument("--references", type=pathlib.Path,
                        default=pathlib.Path("macos9/test/visual"))
    parser.add_argument("--tolerance", type=int, default=8,
                        help="per-channel difference that still counts as equal")
    parser.add_argument("--allowed-fraction", type=float, default=0.001,
                        help="share of unmasked pixels allowed to differ")
    parser.add_argument("--mask", action="append", default=[],
                        metavar="X,Y,W,H", help="repeatable unstable region")
    parser.add_argument("--update", action="store_true",
                        help="copy the actual screenshots in as references")
    parser.add_argument("--json", type=pathlib.Path, default=None)
    args = parser.parse_args()

    masks: list[tuple[int, int, int, int]] = []
    for value in args.mask:
        try:
            x, y, w, h = (int(part) for part in value.split(","))
        except ValueError:
            raise SystemExit(f"--mask {value!r} is not X,Y,W,H")
        masks.append((x, y, w, h))

    actuals = sorted(args.actual.glob("scenario-*.ppm"))
    if not actuals:
        print(f"compare: no scenario screenshots in {args.actual}", file=sys.stderr)
        return 2

    if args.update:
        args.references.mkdir(parents=True, exist_ok=True)
        for shot in actuals:
            shutil.copyfile(shot, args.references / shot.name)
            print(f"updated {args.references / shot.name}")
        return 0

    references = sorted(args.references.glob("scenario-*.ppm"))
    if not references:
        print(
            f"compare: no references in {args.references}; run --update from a real guest run first",
            file=sys.stderr,
        )
        return 2

    results: list[dict] = []
    regressions = 0
    missing = 0
    for shot in actuals:
        reference = args.references / shot.name
        if not reference.exists():
            missing += 1
            results.append({"scenario": shot.stem, "status": "no reference"})
            continue
        pair = compare_pair(reference, shot, args.tolerance, masks)
        if not pair.get("same_size", True):
            status = "size differs"
            regressions += 1
        elif pair["fraction"] > args.allowed_fraction:
            status = f"differs ({pair['fraction']:.4%})"
            regressions += 1
        else:
            status = f"ok ({pair['fraction']:.4%})"
        results.append({"scenario": shot.stem, "status": status, **pair})
        print(f"{shot.stem}: {status}")

    if args.json is not None:
        args.json.write_text(
            json.dumps({"regressions": regressions, "missing": missing, "results": results}, indent=2) + "\n"
        )
    print(f"compare: {regressions} regression(s), {missing} scenario(s) without a reference")
    return 1 if regressions else 0


if __name__ == "__main__":
    sys.exit(main())
