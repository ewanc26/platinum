#!/usr/bin/env python3
"""Check that the Mac image tests use exactly the bridge's image vectors, in the
same order, so the bridge and the Mac client are tested against the same bytes:
the blobs in macos9/test/test_image_blob.c (the reader), and the full cases
(size, bounds, depth, RGBA input and blob) in macos9/test/test_image_encode.c
(the encoder). Run from CI."""
import json
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
want = [v["blob"] for v in json.loads((root / "bridge/test/vectors/image/convert.json").read_text())["vectors"]]
src = (root / "macos9/test/test_image_blob.c").read_text()
block = src.split("/* VECTOR_BLOBS_BEGIN */")[1].split("/* VECTOR_BLOBS_END */")[0]
have = re.findall(r'"([0-9a-f]+)"', block)
if have != want:
    print("FAIL: macos9/test/test_image_blob.c does not match bridge/test/vectors/image/convert.json")
    print(f"  json has {len(want)} blobs, the C test has {len(have)}")
    sys.exit(1)

vectors = json.loads((root / "bridge/test/vectors/image/convert.json").read_text())["vectors"]
want_cases = [f'{v["width"]} {v["height"]} {v["maxWidth"]} {v["maxHeight"]} {v["depth"]} {v["rgba"]} {v["blob"]}' for v in vectors]
enc = (root / "macos9/test/test_image_encode.c").read_text()
enc_block = enc.split("/* VECTOR_CASES_BEGIN */")[1].split("/* VECTOR_CASES_END */")[0]
have_cases = re.findall(r'"([^"]+)"', enc_block)
if have_cases != want_cases:
    print("FAIL: macos9/test/test_image_encode.c cases do not match bridge/test/vectors/image/convert.json")
    print(f"  json has {len(want_cases)} cases, the C test has {len(have_cases)}")
    sys.exit(1)
print(f"ok: image vectors ({len(want)} blobs identical; {len(want_cases)} encoder cases identical)")
