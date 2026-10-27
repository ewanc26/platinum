# Updating Platinum

There are two halves and they are not equally finished.

## The bridge (implemented)

The bridge can update itself from this repository's GitHub releases. It never
does so on its own. You run it, and it asks.

```sh
npm run build
npm run update -- check            # says whether a newer release exists
PLATINUM_BRIDGE_INSTALL_DIR=/srv/platinum npm run update -- apply
PLATINUM_BRIDGE_INSTALL_DIR=/srv/platinum npm run update -- rollback
```

`apply` asks for confirmation (`--yes` in a script) and then:

1. fetches `update.json` and its detached signature `update.json.sig` from the
   latest release, and checks the signature (Ed25519 over the exact bytes of
   `update.json`) against the public key in `bridge/src/update/update_key.ts`
   before reading a byte of the manifest. A release with no signature, or one
   signed by another key, is refused;
2. refuses the manifest unless it is for app `platinum-bridge`, schema 1, and the
   asset URL is under `https://github.com/ewanc26/platinum/releases/download/`.
   The manifest's own `signature` member stays reserved, and one that is set is
   refused;
3. downloads the archive with a size cap and checks its size and SHA-256 against
   the manifest before anything is unpacked;
4. rejects archive members that are absolute or contain `..`, unpacks into
   `releases/<version>/`, and repoints `current` at it atomically;
5. keeps the old target as `previous`, which `rollback` swaps back.

It does not restart the bridge; the new version starts when you restart it. The
archive includes production `node_modules`, so an update runs nothing from the
npm registry and publishes nothing to it. No token is read, sent or printed:
release assets are public.

Run the bridge from `$PLATINUM_BRIDGE_INSTALL_DIR/current` to use this layout.
A git checkout is a development install and is updated with git.

What the checksum and the signature are: the manifest and archive both come
from GitHub over TLS, so the SHA-256 catches corruption and a mismatched upload,
but not someone who can publish a release, because they can publish a matching
manifest. The signature closes that. `.github/workflows/sign-release.yml` signs
the published `update.json` with an Ed25519 key that exists only as the
repository secret `UPDATE_SIGNING_KEY` and attaches `update.json.sig`; the public
half is committed. The manifest carries the archive's SHA-256, so the signature
covers the download too. Replacing the key means shipping a bridge that carries
the new one, signed with the old. Verification runs Wolfram's C (its Node
addon), against Wolfram's Ed25519 vectors (`bridge/test/vectors/ed25519.json`). No
release has been cut with this yet (releases are paused, #62), so the signing
job has not run on a real release.

`scripts/release.sh <version>` produces the updater's inputs (the archive,
`<archive>.sha256` and `update.json`) and, without `--dry-run`, tags and
attaches them to a GitHub release. `--dry-run` writes them to `release/` and
publishes nothing.

### Where the logic lives

Manifest format, version comparison and checksum verification should be shared
with Cobalt and Indigo, so they are specified in
[wolfram#106](https://github.com/ewanc26/wolfram/issues/106) and live in
Wolfram's `update.h`. The bridge runs them in that C through Wolfram's Node
addon (`node/wolfram_core.c`), built by `tools/build-addon.sh` from the
Wolfram commit pinned in `.github/workflows/ci.yml`; that binding is what
replaced the TypeScript port of `wolfram/update.h`. The contract stays
testable from both sides:
`bridge/test/vectors/` copies Wolfram's vectors verbatim (CI diffs them) and
`bridge/test/update.test.ts` runs them through the addon.

## The Mac OS 9 client (not implemented)

The client cannot do modern TLS, so it cannot fetch a release from GitHub. The
design is that the bridge, which already terminates TLS and has verified the
build, tells the client a newer one exists and serves the file over the paired
channel; the client verifies the checksum with Wolfram's shared code and the
user confirms. Tracked in
[#48](https://github.com/ewanc26/platinum/issues/48).

Limits, stated as limits and not yet checked against Inside Macintosh or a
running system, because no Mac OS 9 environment has been available here:

- A running application's file is open and mapped, so replacing it in place is
  likely to be refused. The conservative design writes the new build beside it
  under a new name, and the user quits and swaps it. Whether `FSpExchangeFiles`
  can swap an open file has to be read from the Files chapter and tried.
- Downloads arrive as raw bytes in memory-limited heap. The data fork and
  resource fork of a classic application are separate; an archive that does not
  carry both (and the type and creator codes) produces a file that does not
  launch. The packaging format has to preserve them (MacBinary or an
  AppleSingle/AppleDouble wrapper) and is undecided.
- The PEF build is not produced by CI yet (#41), so there is no artifact to
  serve.

Nothing about the client half is claimed to work. When code lands it can only be
stub-verified in CI, as the rest of `macos9/` is, until the QEMU recipe (#43) has
been followed.
