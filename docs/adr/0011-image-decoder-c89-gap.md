# 0011: Wolfram's image decoder is real but not C89-usable

- Status: accepted; revises the blocker in ADR 0009
- Date: 2026-10-31

## Context

ADR 0009 blocked native images on the absence of a C89-compatible decoder with
a suitable licence. The sibling study found that the decoder exists: Wolfram
ships `wf_image_decode_rgba` (`include/wolfram/image.h`, vendored stb_image plus
a box filter), and Indigo and Cobalt both decode images in constrained clients.
Platinum's pinned Wolfram, v0.36.2, already contains it.

That does not make it usable in Platinum's Mac target. Compiling
`wolfram/src/image.c` with the target's flags fails:

- `wolfram/include/wolfram/image.h` includes `wolfram/util.h`, which pulls
  `agent.h` → `repo.h` → `repo/cbor.h`; that header has a trailing comma in an
  enumerator list, rejected under `-std=c89 -pedantic-errors`.
- `stb_image.h` contains `//` comments; `-Wcomment` plus `-Werror` makes that
  fatal.

The image module needs none of the agent or repository surface. The fault is
the header layering, not the decoder.

## Decision

- **Do not compile Wolfram's image module into the Mac target now**, and do not
  relax `-std=c89 -pedantic-errors -Werror` for it. The strict dialect is a
  deliberate constraint of this target and is worth more than images.
- **Keep the bridge image route** until a C89-clean decoder exists. The image
  path is therefore not a reason to keep the rest of the bridge, but it is still
  a bridge dependency; bridge removal remains gated on it.
- **Ask upstream** for one of:
  1. a standalone `wolfram/image.h` and `image.c` with no `util.h`/agent
     dependency, and `stb_image.h` converted to C89 comments; or
  2. a documented `-DWOLFRAM_IMAGE_STANDALONE` build of the same;
  plus a note in Wolfram's compatibility table saying the module is C89-clean.
- **Do not copy stb_image into Platinum.** A second vendored copy would drift
  and would hide the upstream fix that benefits every C89 client.

## Consequences

- The Mac continues to request image blobs from the bridge; the bridge remains
  on the runtime path for images even after the screens migrate.
- The decoder itself is not the risk it looked like in ADR 0009: it exists and
  is used by Indigo. The remaining work is a packaging change in Wolfram.
- If Wolfram declines the split, the fallback is an opt-in Platinum-side
  compilation unit with its own C89-clean stb copy, recorded in a superseding
  ADR with the licence and maintenance cost spelled out.
