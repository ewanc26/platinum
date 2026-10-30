# Mac OS 9 reference screenshots

These are screenshots of **PlatinumFixture running in a real Mac OS 9 guest**,
captured through QMP `screendump` by `tools/macos9-qemu/harness.py`. They are
not renders of a host-side imitation and not screenshots of another application.

No reference images are committed yet, because none could be produced honestly:
the QEMU harness has not been run against an owner-supplied Mac OS 9 installation
here. See `docs/TESTING_MACOS9.md`, "Current status".

To produce them:

```sh
python3 tools/macos9-qemu/harness.py run --out macos9-runtime
python3 tools/macos9-qemu/compare.py --actual macos9-runtime \
  --references macos9/test/visual --update
```

Review the images before committing them: they must show the Platinum main
window, not the Finder. Use a fixed guest resolution and the same QEMU version
and ROM as the run that recorded the current references, and note both in the
pull request that changes them.

`compare.py` masks the regions passed with `--mask` (pointer, insertion point,
clock) and allows a small fraction of differing pixels. Do not lower those
tolerances to hide a real layout change; update the references deliberately
instead, and say why in the changelog.

Never commit Mac OS 9 installation media, ROMs, VM disks, personal data or
screenshots containing an account name or token.
