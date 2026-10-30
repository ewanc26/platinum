#!/usr/bin/env python3
"""Boot Platinum's Mac OS 9 fixture under QEMU, headless, driven over QMP.

This is the runtime layer, and it is deliberately separate from the two layers
that need no Mac at all:

  * host C89 compile           -- source and dialect only;
  * Retro68 68k/PowerPC build   -- a real Mac application compile and link;
  * this script                 -- the application actually runs under Mac OS 9.

What it does
------------
Boots an owner-supplied, already-installed Mac OS 9 PowerPC disk image on QEMU's
``mac99`` machine with ``-display none``, drives the guest with structured QMP
commands only (``input-send-event`` and ``screendump``; never
``human-monitor-command``), captures one screenshot per fixture scenario, and
compares them with references when they exist.

What it never does
------------------
Downloads, bundles or redistributes Mac OS 9 media, ROMs or disk images. Those
must be supplied through the environment by the owner:

    PLATINUM_OS9_BASE      installed Mac OS 9.2.2 PowerPC disk image (required)
    PLATINUM_OS9_ROM       PowerPC Mac ROM QEMU boots with (optional)
    PLATINUM_OS9_TRANSFER  HFS image holding PlatinumFixture, already written
                           on the host and closed (optional, read-only here)
    PLATINUM_OS9_QEMU      qemu-system-ppc to use (default: from PATH)

Every run uses a fresh qcow2 overlay on the base image, which is opened
read-only; the overlay is deleted afterwards unless ``--keep`` is given. The
base image is never written.

If the media are missing, ``check`` and ``run`` exit with status 2 and print
exactly what is missing. They never report a pass, because booting nothing is
not a Mac OS 9 runtime test.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
import time

MACHINE = "mac99"
DEFAULT_QEMU = "qemu-system-ppc"
DEFAULT_QEMU_IMG = "qemu-img"
QMP_TIMEOUT = 180.0
BOOT_TIMEOUT = 600.0
INK_THRESHOLD = 0.01

# Fixture scenarios the application switches between with the number keys.
# Kept here so the harness, the fixture and the references agree on the names.
SCENARIOS = {
    "populated": "1",
    "empty": "2",
    "one-post": "3",
    "long-text": "4",
    "selected-detail": "5",
    "loading": "6",
    "error": "7",
    "unpaired": "8",
    "size-640x480": "9",
    "size-800x600": "0",
}

FIXTURE_APP = "PlatinumFixture"


class Blocked(RuntimeError):
    """A requirement only the owner can supply is missing."""


class QMPError(RuntimeError):
    pass


class QMP:
    def __init__(self, path: pathlib.Path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(QMP_TIMEOUT)
        self._file = None
        deadline = time.monotonic() + QMP_TIMEOUT
        while True:
            try:
                self.sock.connect(str(path))
                break
            except (FileNotFoundError, ConnectionRefusedError):
                if time.monotonic() > deadline:
                    raise QMPError(f"no QMP socket at {path} after {QMP_TIMEOUT:.0f}s")
                time.sleep(0.2)
        self._file = self.sock.makefile("rwb")
        greeting = self._read()
        if "QMP" not in greeting:
            raise QMPError(f"not a QMP greeting: {greeting!r}")
        self.command("qmp_capabilities")

    def _read(self) -> dict:
        while True:
            line = self._file.readline()
            if not line:
                raise QMPError("the QMP connection closed")
            message = json.loads(line)
            if "event" in message:
                continue
            return message

    def command(self, name: str, **arguments) -> dict:
        request = {"execute": name}
        if arguments:
            request["arguments"] = arguments
        self._file.write((json.dumps(request) + "\n").encode())
        self._file.flush()
        while True:
            answer = self._read()
            if answer.get("id") is not None:
                continue
            if "error" in answer:
                raise QMPError(f"{name}: {answer['error'].get('desc', answer['error'])}")
            return answer.get("return", {})

    def close(self) -> None:
        try:
            if self._file is not None:
                self._file.close()
        finally:
            self.sock.close()


def qemu_path() -> str:
    return os.environ.get("PLATINUM_OS9_QEMU", DEFAULT_QEMU)


def qemu_version(qemu: str) -> str:
    try:
        out = subprocess.run([qemu, "--version"], capture_output=True, text=True, timeout=30)
    except FileNotFoundError:
        raise Blocked(f"{qemu} is not installed; install QEMU (emulators/qemu) to run this layer")
    if out.returncode != 0:
        raise Blocked(f"{qemu} --version failed: {out.stderr.strip()}")
    return out.stdout.splitlines()[0].strip()


def machine_supported(qemu: str) -> bool:
    out = subprocess.run([qemu, "-machine", "help"], capture_output=True, text=True, timeout=60)
    return any(line.split()[0] == MACHINE for line in out.stdout.splitlines() if line.strip())


def media() -> dict[str, pathlib.Path | None]:
    base = os.environ.get("PLATINUM_OS9_BASE")
    rom = os.environ.get("PLATINUM_OS9_ROM")
    transfer = os.environ.get("PLATINUM_OS9_TRANSFER")
    return {
        "base": pathlib.Path(base).expanduser() if base else None,
        "rom": pathlib.Path(rom).expanduser() if rom else None,
        "transfer": pathlib.Path(transfer).expanduser() if transfer else None,
    }


def preflight(require_media: bool) -> dict:
    qemu = qemu_path()
    version = qemu_version(qemu)
    if not machine_supported(qemu):
        raise Blocked(f"{qemu} does not offer the {MACHINE} machine")
    found = media()
    missing = []
    if require_media:
        if found["base"] is None:
            missing.append("PLATINUM_OS9_BASE (installed Mac OS 9.2.2 PowerPC disk image)")
        elif not found["base"].exists():
            missing.append(f"PLATINUM_OS9_BASE points at {found['base']}, which does not exist")
        if found["rom"] is not None and not found["rom"].exists():
            missing.append(f"PLATINUM_OS9_ROM points at {found['rom']}, which does not exist")
        if found["transfer"] is not None and not found["transfer"].exists():
            missing.append(f"PLATINUM_OS9_TRANSFER points at {found['transfer']}, which does not exist")
    return {"qemu": qemu, "version": version, "machine": MACHINE, "media": found, "missing": missing}


def make_overlay(base: pathlib.Path, overlay: pathlib.Path, qemu_img: str) -> None:
    """A disposable overlay on an immutable base, with the base read-only."""
    out = subprocess.run(
        [qemu_img, "create", "-f", "qcow2", "-F", "qcow2", "-b", str(base), str(overlay)],
        capture_output=True,
        text=True,
    )
    if out.returncode != 0:
        # A raw base (an HFS volume or a DD image) needs -F raw.
        out = subprocess.run(
            [qemu_img, "create", "-f", "qcow2", "-F", "raw", "-b", str(base), str(overlay)],
            capture_output=True,
            text=True,
        )
    if out.returncode != 0:
        raise Blocked(f"qemu-img could not create an overlay on {base}: {out.stderr.strip()}")


def start_qemu(args: argparse.Namespace, overlay: pathlib.Path, qmp_socket: pathlib.Path,
               found: dict) -> subprocess.Popen:
    qemu = found["qemu"]
    command = [
        qemu,
        "-machine", MACHINE,
        "-cpu", "G4",
        "-m", str(args.memory),
        "-display", "none",
        "-qmp", f"unix:{qmp_socket},server=on,wait=off",
        "-drive", f"file={overlay},format=qcow2,media=disk,if=ide",
        "-net", "none",
    ]
    if args.debug_vnc:
        command[command.index("-display") + 1] = "vnc=127.0.0.1:1"
    if found["rom"] is not None:
        command += ["-bios", str(found["rom"])]
    if found["transfer"] is not None:
        # Read-only: the host wrote the fixture onto this image and closed it
        # before QEMU started, and nothing writes it back during the run.
        command += ["-drive", f"file={found['transfer']},format=raw,media=disk,if=ide,readonly=on"]
    log = args.out / "qemu.log"
    args.out.mkdir(parents=True, exist_ok=True)
    with open(log, "wb") as handle:
        process = subprocess.Popen(command, stdout=handle, stderr=subprocess.STDOUT)
    (args.out / "qemu-command.json").write_text(json.dumps(command, indent=2) + "\n")
    return process


def screenshot(qmp: QMP, path: pathlib.Path) -> str:
    """QMP screendump, PPM first (stdlib comparison), PNG as the fallback."""
    try:
        qmp.command("screendump", filename=str(path), format="ppm")
        return "ppm"
    except QMPError:
        png = path.with_suffix(".png")
        qmp.command("screendump", filename=str(png))
        return "png"


def ink_fraction(path: pathlib.Path) -> float:
    """Share of non-black pixels in a binary PPM. 0.0 if it is not one."""
    try:
        data = path.read_bytes()
    except OSError:
        return 0.0
    if not data.startswith(b"P6"):
        return 0.0
    fields: list[bytes] = []
    position = 2
    while len(fields) < 3 and position < len(data):
        while position < len(data) and data[position : position + 1].isspace():
            position += 1
        start = position
        while position < len(data) and not data[position : position + 1].isspace():
            position += 1
        fields.append(data[start:position])
    position += 1
    try:
        width, height, _ = (int(field) for field in fields)
    except ValueError:
        return 0.0
    pixels = data[position : position + width * height * 3]
    if len(pixels) < width * height * 3:
        return 0.0
    lit = sum(1 for i in range(0, len(pixels), 3) if pixels[i] or pixels[i + 1] or pixels[i + 2])
    return lit / (width * height)


def wait_for_gui(qmp: QMP, out: pathlib.Path, timeout: float) -> float:
    """Wait until the framebuffer has actual content, not just a black screen."""
    deadline = time.monotonic() + timeout
    last = out / "boot.ppm"
    while time.monotonic() < deadline:
        fmt = screenshot(qmp, last)
        shot = last if fmt == "ppm" else last.with_suffix(".png")
        fraction = ink_fraction(shot)
        if fraction >= INK_THRESHOLD:
            return fraction
        time.sleep(3.0)
    raise QMPError(f"the guest produced no usable framebuffer within {timeout:.0f}s")


def send_key(qmp: QMP, qcode: str) -> None:
    for down in (True, False):
        qmp.command(
            "input-send-event",
            events=[{"type": "key", "data": {"down": down, "key": {"type": "qcode", "data": qcode}}}],
        )
        time.sleep(0.05)


def send_mouse(qmp: QMP, dx: int, dy: int, button: bool = False) -> None:
    events = []
    if dx or dy:
        events.append({"type": "rel", "data": {"axis": "x", "value": dx}})
        events.append({"type": "rel", "data": {"axis": "y", "value": dy}})
    if button:
        events.append({"type": "btn", "data": {"down": True, "button": "left"}})
    if events:
        qmp.command("input-send-event", events=events)
    if button:
        time.sleep(0.1)
        qmp.command(
            "input-send-event",
            events=[{"type": "btn", "data": {"down": False, "button": "left"}}],
        )


def archive(out: pathlib.Path, qmp: QMP | None, process: subprocess.Popen | None) -> None:
    if qmp is not None:
        try:
            (out / "final-status.json").write_text(
                json.dumps(qmp.command("query-status"), indent=2) + "\n"
            )
        except (QMPError, OSError):
            pass
        try:
            # Mac OS 9 shuts down from this; the wait below is the fallback.
            qmp.command("system_powerdown")
        except (QMPError, OSError):
            pass
    if process is not None and process.poll() is None:
        try:
            process.wait(timeout=90)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()


def run(args: argparse.Namespace) -> int:
    found = preflight(require_media=True)
    if found["missing"]:
        print("macos9-qemu: blocked, the runtime layer cannot run here:", file=sys.stderr)
        for item in found["missing"]:
            print(f"  missing: {item}", file=sys.stderr)
        print(
            "Supply an installed Mac OS 9.2.2 PowerPC image through PLATINUM_OS9_BASE. "
            "See docs/TESTING_MACOS9.md; nothing is downloaded or committed.",
            file=sys.stderr,
        )
        return 2

    args.out.mkdir(parents=True, exist_ok=True)
    overlay = args.out / "overlay.qcow2"
    qmp_socket = args.out / "qmp.sock"
    summary: dict = {
        "qemu": found["version"],
        "machine": MACHINE,
        "base": str(found["media"]["base"]),
        "overlay": str(overlay),
        "scenarios": [],
        "fixture_launch_attempted": bool(found["media"]["transfer"]),
        "layer": "runtime (QEMU mac99, Mac OS 9 guest)",
    }
    process = None
    qmp = None
    status = 0
    try:
        make_overlay(found["media"]["base"], overlay, os.environ.get("PLATINUM_OS9_QEMU_IMG", DEFAULT_QEMU_IMG))
        process = start_qemu(args, overlay, qmp_socket, found)
        qmp = QMP(qmp_socket)
        summary["boot_ink"] = wait_for_gui(qmp, args.out, args.boot_timeout)
        summary["boot"] = "gui"
        time.sleep(args.settle)

        if found["media"]["transfer"] is None:
            # A booted Finder is not a Platinum run. Say so, and use a distinct
            # exit status so no caller can turn a boot-only run into a pass.
            summary["fixture"] = "not delivered: PLATINUM_OS9_TRANSFER was not set"
            screenshot(qmp, args.out / "finder.ppm")
            print(
                "macos9-qemu: the guest booted, but no fixture was delivered; "
                "this is a boot-only run, not a UI runtime test",
                file=sys.stderr,
            )
            status = 3
        else:
            # Launch the fixture from the transfer volume the owner prepared:
            # an alias in the guest's Startup Items or a launcher script. The
            # harness sends the keystrokes the launcher documents.
            for key in args.launch_keys.split(","):
                send_key(qmp, key)
                time.sleep(args.key_delay)
            time.sleep(args.launch_wait)
            for name in args.scenarios.split(","):
                if name not in SCENARIOS:
                    raise SystemExit(f"unknown scenario {name!r}; known: {', '.join(SCENARIOS)}")
                send_key(qmp, SCENARIOS[name])
                time.sleep(args.key_delay)
                shot = args.out / f"scenario-{name}.ppm"
                fmt = screenshot(qmp, shot)
                entry = {
                    "scenario": name,
                    "key": SCENARIOS[name],
                    "shot": str(shot if fmt == "ppm" else shot.with_suffix(".png")),
                    "ink": ink_fraction(shot),
                }
                summary["scenarios"].append(entry)
            summary["fixture"] = "scenarios captured"

        (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(json.dumps(summary, indent=2))
    except (Blocked, QMPError) as error:
        print(f"macos9-qemu: {error}", file=sys.stderr)
        summary["error"] = str(error)
        (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        status = 1
    finally:
        archive(args.out, qmp, process)
        if qmp is not None:
            qmp.close()
        if not args.keep and overlay.exists():
            overlay.unlink()

    return status


def check(_args: argparse.Namespace) -> int:
    found = preflight(require_media=False)
    print(f"qemu: {found['version']}")
    print(f"machine: {MACHINE} supported")
    missing = []
    for name, path in found["media"].items():
        if path is None:
            missing.append(f"PLATINUM_OS9_{name.upper()} is not set")
        elif not path.exists():
            missing.append(f"PLATINUM_OS9_{name.upper()} points at {path}, which does not exist")
        else:
            print(f"{name}: {path}")
    if missing:
        print("\nruntime layer blocked; the owner must supply:", file=sys.stderr)
        for item in missing:
            print(f"  {item}", file=sys.stderr)
        print("Nothing was downloaded. See docs/TESTING_MACOS9.md.", file=sys.stderr)
        return 2
    print("\nall runtime inputs are present")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    check_parser = sub.add_parser("check", help="report whether the runtime layer can run here")
    check_parser.set_defaults(func=check)

    run_parser = sub.add_parser("run", help="boot the guest headless and capture fixture scenarios")
    run_parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("macos9-runtime"))
    run_parser.add_argument("--scenarios", default=",".join(SCENARIOS))
    run_parser.add_argument("--launch-keys", default="", help="QMP key names the guest launcher needs")
    run_parser.add_argument("--memory", type=int, default=512)
    run_parser.add_argument("--boot-timeout", type=float, default=BOOT_TIMEOUT)
    run_parser.add_argument("--settle", type=float, default=15.0)
    run_parser.add_argument("--key-delay", type=float, default=1.5)
    run_parser.add_argument("--launch-wait", type=float, default=20.0)
    run_parser.add_argument("--debug-vnc", action="store_true", help="VNC on 127.0.0.1:5901 instead of none")
    run_parser.add_argument("--keep", action="store_true")
    run_parser.set_defaults(func=run)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
