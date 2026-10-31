# 0009: Images remain blocked on a native decoder

- Status: accepted; blocks the image parts of the bridge removal. See ADR
  0011 for the decoder finding that revises this: the decoder exists but is not
  C89-usable.
- Date: 2026-10-31

## Context

The bridge converted JPEG/PNG to Platinum's indexed PLIM bitmap because the Mac
had no decoder. With no bridge at runtime, the Mac must fetch the CDN's JPEG or
PNG and decode it, or stop offering images.

The AT Protocol CDN serves post images as JPEG by default (PNG is available for
some assets), with thumbnail and full-size presets. Uploads can be up to
4000x4000 and 2 MB, which is far beyond what can be decoded into memory on a
small OS 9 machine (4000x4000 RGBA is 64 MB).

## Decision

- Do not remove the bridge's image route until a decoder is adopted, licensed,
  compiled for PowerPC, and tested against real CDN output.
- When one is adopted: fetch only the `feed_thumbnail` preset, enforce a byte
  cap and a pixel cap before allocating, decode into a bounded bitmap, and draw
  through the existing QuickDraw path. No full-size 4000x4000 decode.
- Until then, show a placeholder (initials or a generic icon) and say in the
  parity table that images are unavailable, not "implemented".
- Do not present a host-side decoder test as Mac OS 9 image support; the
  decoder and QuickDraw path must run in the guest.

## Consequences

- The bridge cannot be deleted while images are expected on every screen, even
  if timelines and profiles are native. That is an explicit dependency, not an
  oversight.
- A decoder is a new third-party licence and a new memory budget; it needs its
  own ADR before code.
