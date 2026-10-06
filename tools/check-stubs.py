#!/usr/bin/env python3
"""Check the SDK stubs against the real Toolbox signatures.

    tools/check-stubs.py            verify (CI)
    tools/check-stubs.py --list     print every current difference, for the allowlist

macos9/test/sdk-stubs/signatures.tsv (from the Multiversal Interfaces, see
tools/gen-signatures.py) says what each function takes. For every function a
stub header declares, this fails if it is missing from that table, or if its
argument count or any argument's kind differs, unless it is listed in
macos9/test/sdk-stubs/known-mismatches.txt. The allowlist may only shrink: an
entry that is no longer needed fails the check, so it has to be deleted.
"""
import glob
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STUBS = ROOT / "macos9/test/sdk-stubs"

STR = {"StringPtr", "ConstStringPtr", "Str255", "ConstStr255Param", "Str63", "Str31"}
I8 = {"SInt8", "SignedByte", "int8_t"}
I16 = {"short", "INTEGER", "SInt16", "int16_t", "OSErr", "ScriptCode", "CharParameter", "char", "EventMask"}
I32 = {"long", "LONGINT", "SInt32", "int32_t", "Size", "OSStatus", "OSType", "FourCharCode"}
BOOL = {"Boolean", "bool"}


def kind(t):
    t = re.sub(r"\b(const|struct|unsigned)\b", "", t).strip()
    t = re.sub(r"\s+", " ", t)
    if t in STR or t == "Ptr":
        return "ptr:mem"
    if "*" in t:
        # A plain pointer is compared by what it points to, so two pointer
        # arguments in the wrong order (FSRead's buffer and count) differ.
        base = re.sub(r"\*", "", t).strip()
        if base in ("void", "Ptr", "char", "unsigned char"):
            return "ptr:mem"
        return "ptr:" + kind(base)
    if t in I8:
        return "i8"
    if t in I16:
        return "i16"
    if t in I32:
        return "i32"
    if t in BOOL:
        return "bool"
    if t.endswith("UPP") or t.endswith("Proc") or t.endswith("ProcPtr") or t == "ProcPtr":
        return "ptr"
    if t.endswith("Handle"):
        return "ptr"
    if t.endswith("Ptr") and len(t) > 3:
        return "ptr:" + kind(t[:-3])
    return t


def split_args(text):
    text = text.strip()
    if text in ("", "void"):
        return []
    out, depth, cur = [], 0, ""
    for ch in text:
        if ch in "(<":
            depth += 1
        elif ch in ")>":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return [a.strip() for a in out]


def arg_type(arg):
    # drop the parameter name: the last identifier, unless the arg is a bare type
    m = re.match(r"^(.*?)(\b[A-Za-z_][A-Za-z0-9_]*)(\[\d*\])?$", arg.strip())
    if not m:
        return arg
    head, name = m.group(1).strip(), m.group(2)
    known = {"Boolean", "short", "long", "int", "char", "void", "Point", "OSErr", "StringPtr"}
    if head == "" or name in known and head == "":
        return arg.strip()
    return head if head else name


def stub_functions():
    funcs = {}
    for h in sorted(glob.glob(str(STUBS / "*.h"))):
        text = Path(h).read_text()
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        for m in re.finditer(r"^[A-Za-z_][A-Za-z0-9_ \*]*?\b([A-Za-z_][A-Za-z0-9_]*)\(([^;{]*)\);", text, re.M):
            name, args = m.group(1), m.group(2)
            if name.startswith("PLATINUM"):
                continue
            funcs[name] = [kind(arg_type(a)) for a in split_args(args)]
    return funcs


def real_table():
    table = {}
    for line in (STUBS / "signatures.tsv").read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        name, ret, args = line.split("|")
        table[name] = [kind(a) for a in args.split(";")] if args else []
    return table


def same(a, b):
    """Equal, or both pointers and one of them is opaque (a handle, a proc)."""
    return a == b or (a.startswith("ptr") and b.startswith("ptr") and "ptr" in (a, b))


def differences():
    real = real_table()
    out = {}
    for name, got in stub_functions().items():
        if name not in real:
            out[name] = "not in the Multiversal table"
        elif len(got) != len(real[name]):
            out[name] = f"takes {len(real[name])} arguments, the stub declares {len(got)}"
        elif not all(same(a, b) for a, b in zip(got, real[name])):
            out[name] = f"argument kinds differ: real {real[name]}, stub {got}"
    return out


def allowlist():
    path = STUBS / "known-mismatches.txt"
    names = {}
    for line in path.read_text().splitlines() if path.exists() else []:
        line = line.strip()
        if line and not line.startswith("#"):
            names[line.split()[0]] = line
    return names


def main():
    diff = differences()
    if "--list" in sys.argv:
        for name in sorted(diff):
            print(f"{name}  # {diff[name]}")
        return 0
    allowed = allowlist()
    bad = 0
    for name, why in sorted(diff.items()):
        if name not in allowed:
            print(f"FAIL: stub {name}: {why}")
            bad = 1
    for name in sorted(allowed):
        if name not in diff:
            print(f"FAIL: known-mismatches.txt lists {name}, which now matches (or is gone); delete the line")
            bad = 1
    if not bad:
        print(f"ok: stubs ({len(diff)} known mismatches still to fix)")
    return bad


sys.exit(main())
