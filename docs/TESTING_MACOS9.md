# Testing Platinum on Mac OS 9

There are four layers, and passing one proves nothing about the next. Every
report of this work names the layer it ran.

| Layer | Command | Proves | Does not prove |
|---|---|---|---|
| C89 sources | `make` target in CI `macos9`, or the loop in `ci.yml` | every Mac source is strict C89 and agrees with the SDK declarations | that it links, runs or draws |
| UI logic | `macos9/test/test_ui.c` | layout rectangles, hit-testing, selection, scrolling and shortcuts | anything about QuickDraw output |
| Retro68 build | `tools/build-macos9.sh 68k` / `ppc` | a real Classic Mac application compiles and links for both CPUs | that it launches |
| Runtime | `tools/macos9-qemu/harness.py run` | a Mac OS 9 guest boots, draws, accepts QMP input and is screenshotted | live bridge behaviour; the fixture has no transport |

Physical hardware remains a fifth layer and is still the only one that validates
Platinum against a real Mac. See AGENTS.md, "Hardware validation".

## Host UI logic tests

`macos9/test/test_ui.c` links the real `ui.c` and `timeline.c` over the stub
transport and `macos9/test/ui_stubs.c`. That second file is the narrow exception
to the "declaration-only" rule in `macos9/test/sdk-stubs/README.md`: it
implements only the Toolbox calls `ui.c` makes, with real rectangle geometry and
no-op drawing. It is not linked into the application, and no SDK declaration was
changed for it.

Covered: 640×480 and 800×600 layouts, compact and short windows, content
rectangles below the minimum and empty rectangles; every region non-negative and
ordered; toolbar button edges; navigation rows; timeline row boundaries and the
heading strip; scrolling clamps for arrows and paging; Down-at-the-end asking
for the next page and doing nothing without one; every Command shortcut; empty,
one-post and full timelines; and that recomputing the layout preserves state.

Run it the way CI does:

```sh
cd macos9
clang -std=c89 -pedantic-errors -Wall -Wextra -Wno-unused-parameter \
  -Wno-multichar -Werror -Wdeclaration-after-statement -Wstrict-prototypes \
  -Wvla -I . -I test/sdk-stubs -I ../../wolfram/include -I test \
  -o /tmp/test_ui ui.c timeline.c post_media.c text_codec.c \
  bridge_client.c json_min.c test/wolfram_stub.c test/ui_stubs.c \
  test/test_ui.c && /tmp/test_ui
```

## Fixture application

`macos9/fixture.c` builds `PlatinumFixture` when
`-DPLATINUM_BUILD_FIXTURE=ON` is passed. It links the production `ui.c`,
`timeline.c` and drawing code, fills the timeline from compiled-in data and
links the transport stub, so it has no token, no preferences and no network.
Its creator is `PTLF`, not `PTLM`, and releases build without it.

Keys `1`–`9` and `0` select: populated timeline, empty, one post, long text,
selected post with the inspector, loading, error, unpaired, 640×480 and
800×600 content. Arrows and the mouse go through the real
`platinum_ui_handle_*` functions, so input changes what the next screenshot
shows. The mapping lives in `tools/macos9-qemu/harness.py`.

## Runtime layer: QEMU

Requirements, all supplied by the owner and none by this repository:

- `qemu-system-ppc` with the `mac99` machine (developed against QEMU 11.1.3);
- an installed Mac OS 9.2.2 PowerPC disk image;
- optionally the PowerPC Mac ROM QEMU should boot;
- optionally an HFS image containing `PlatinumFixture`, written on the host and
  closed before QEMU starts.

Point the harness at them:

```sh
export PLATINUM_OS9_BASE=/path/to/macos9.qcow2      # required
export PLATINUM_OS9_ROM=/path/to/ppc-rom.bin        # optional
export PLATINUM_OS9_TRANSFER=/path/to/fixture.hfs   # optional
python3 tools/macos9-qemu/harness.py check
```

`check` prints the QEMU version and whether `mac99` is present, then reports
every missing input and exits 2. It never downloads anything: Mac OS 9 media and
ROMs are not committed, redistributed or fetched by CI.

A run boots `-machine mac99 -display none` with a fresh qcow2 overlay whose base
is opened read-only, and drives the guest only through structured QMP:
`input-send-event` for keys and pointer, `screendump` for images. It does not use
`human-monitor-command`, and `-nographic` is not used because the guest needs
its display device even when the host has no window.

```sh
python3 tools/macos9-qemu/harness.py run --out macos9-runtime \
  --launch-keys h,o,m,e --scenarios populated,empty,one-post
```

Boot is detected by waiting for a framebuffer with actual content, bounded by
`--boot-timeout`, not by one fixed sleep. `--debug-vnc` exposes
`vnc=127.0.0.1:1` for a developer; nothing binds a public interface. `--keep`
retains the overlay and `qemu.log`, `summary.json`, `qemu-command.json` and the
screenshots for inspection. The overlay is deleted at the end otherwise.

The fixture has to be launched inside the guest. Use a Startup Items entry or a
small launcher on the transfer volume, and pass the keys it needs through
`--launch-keys`. Classic Mac OS 9 has no AppleScript or Accessibility UI
scripting, so nothing in the harness relies on it.

## Screenshots and references

The harness writes `scenario-<name>.ppm` through QMP `screendump`, requesting
PPM so comparison needs no Python packages.

```sh
python3 tools/macos9-qemu/compare.py --actual macos9-runtime \
  --references macos9/test/visual --mask 0,0,16,16
```

`compare.py` counts pixels whose channels differ by more than `--tolerance` and
fails a scenario when more than `--allowed-fraction` of the unmasked pixels
differ. Use `--mask` for regions that cannot be stable (pointer, insertion
point, clock). `--update` writes the actual screenshots in as references; do
that only from a real guest run, and commit references only once they are
stable. With no references present it exits 2 — "not verified", never a pass.

## CI

The C89 and Retro68 jobs stay on GitHub-hosted runners and build the fixture.
The runtime job is `.github/workflows/macos9-runtime.yml`: manual dispatch only,
on a self-hosted runner labelled `macos9`, with the media at the paths the
runner exports. It is not a required check, because a public runner cannot hold
legal Mac OS 9 media, and it fails loudly rather than skipping when the media
are absent.

## Current status

- Host C89 compile and `test_ui`: run in CI on every pull request.
- Retro68 68k and PowerPC builds, including the fixture: run in CI on every
  pull request.
- QEMU boot, fixture launch, QMP input and screenshots: **blocked on this host**
  — QEMU 11.1.3 and `mac99` are present, but no owner-supplied Mac OS 9 image or
  ROM exists here. No runtime claim has been made.
