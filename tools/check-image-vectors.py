#!/usr/bin/env python3
"""Check that the blobs in macos9/test/test_image_blob.c are exactly the blobs in
bridge/test/vectors/image/convert.json, in the same order, so the bridge and the
Mac client are tested against the same bytes. Run from CI."""
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
print(f"ok: image vectors ({len(want)} blobs identical)")
