#!/usr/bin/env python3
"""Regenerate macos9/test/sdk-stubs/signatures.tsv from the Multiversal Interfaces.

    tools/gen-signatures.py

Needs network access to raw.githubusercontent.com, so CI does not run it; CI
runs tools/check-stubs.py against the committed table. Source of the data:
https://github.com/autc04/multiversal (defs/*.yaml), the API definitions Retro68
uses, generated from the headers that shipped with Executor 2000 and
redistributable. They are not Apple's Universal Interfaces or Inside Macintosh.

Only functions that the stubs declare are written, so the table stays small.
"""
import glob
import re
import sys
import urllib.request
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
STUBS = ROOT / "macos9/test/sdk-stubs"
BASE = "https://raw.githubusercontent.com/autc04/multiversal/master/defs/"
DEFS = ["TextEdit", "WindowMgr", "FileMgr", "QuickDraw", "EventMgr", "MenuMgr", "ControlMgr",
        "MemoryMgr", "CQuickDraw", "FontMgr", "ScriptMgr", "DialogMgr", "ToolboxUtil", "ToolboxEvent",
        "OSEvent", "ResourceMgr", "Finder", "StdFilePkg"]

sigs = {}
for d in DEFS:
    text = urllib.request.urlopen(BASE + d + ".yaml", timeout=30).read().decode()
    for e in yaml.safe_load(text) or []:
        if isinstance(e, dict) and "function" in e:
            fn = e["function"]
            # Arguments passed as trap-word bits (register: TrapBit<...>) are what
            # the variants (FreeMem, FreeMemSys) set; the C function has no such
            # parameter.
            args = [a["type"] for a in fn.get("args", [])
                    if not str(a.get("register", "")).startswith("TrapBit<")]
            sigs[fn["name"]] = (fn.get("return", "void"), args)

declared = set()
for h in sorted(glob.glob(str(STUBS / "*.h"))):
    for m in re.finditer(r"^[A-Za-z_][A-Za-z0-9_ \*]*?\b([A-Za-z_][A-Za-z0-9_]*)\(", Path(h).read_text(), re.M):
        declared.add(m.group(1))

lines = ["# name|return|argument types, from autc04/multiversal defs (see tools/gen-signatures.py)"]
for name in sorted(declared & set(sigs)):
    ret, args = sigs[name]
    lines.append(f"{name}|{ret}|{';'.join(args)}")
(STUBS / "signatures.tsv").write_text("\n".join(lines) + "\n")
print(f"wrote {len(lines) - 1} signatures; {len(declared - set(sigs))} stub functions have no Multiversal entry")
