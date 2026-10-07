# Images

The Mac never decodes a JPEG, a PNG or a WebP, and it never talks to an image
host. The bridge fetches, shrinks and reduces each image to a small indexed-colour
bitmap, and the Mac wraps the bytes in a PixMap and draws them with CopyBits.
This page is the contract between the two. The test vectors in
[`bridge/test/vectors/image/convert.json`](../bridge/test/vectors/image/convert.json)
are its examples, and the Mac client's parser will be tested against the same
bytes.

What exists today is the bridge half (the converter and `GET /v1/image`) and the
Mac's reader and PixMap builder, which no window uses yet; the plan is in [PARITY.md](PARITY.md) under
avatars, post images, alt text, the viewer and link cards (#32), and attaching
images (#33).

## Asking for an image

`GET /v1/image?ref=...&w=...&depth=...`, with the usual bearer token.

- `ref` is an opaque reference the bridge put in a post (`author.avatar` and
  `images[].ref`, see [BRIDGE_PROTOCOL.md](BRIDGE_PROTOCOL.md)). It has the shape
  `feed_thumbnail/plain/did:plc:.../bafy...@jpeg`: a kind (`avatar`,
  `avatar_thumbnail`, `feed_thumbnail` or `feed_fullsize`), the account, a blob
  id and `jpeg` or `png`. The Mac passes it back unchanged. It is never a URL,
  and the bridge only ever fetches from `cdn.bsky.app`, so this route cannot be
  turned into a way of making the bridge request somewhere else.
- `w` is the widest picture the Mac wants, 16 to 320, default 320. Height is
  limited to 320. An image is never enlarged.
- `depth` is 4 (16 colours, the default) or 8 (256 colours).

The answer is `200` with `content-type: application/x-platinum-image` and the
blob below, or `400 invalid_image` for a bad request, `413` if the converted
image would still be over the size limit, `502 image_unavailable` if the image
could not be fetched, decoded or was not a JPEG or PNG. Source images over 5 MB
or 12 million pixels are refused. The bridge keeps the last few converted images
in memory (about 4 MB), so scrolling back does not fetch again.

## The blob ("PLIM", version 1)

All numbers are big-endian, as the Mac wants them.

| offset | size | field |
| ------ | ---- | ----- |
| 0 | 4 | magic, the bytes `PLIM` |
| 4 | 1 | version, 1 |
| 5 | 1 | depth in bits per pixel: 4 or 8 |
| 6 | 2 | width in pixels, at least 1 |
| 8 | 2 | height in pixels, at least 1 |
| 10 | 2 | rowBytes: whole bytes per row, rounded up to even |
| 12 | 2 | colour count N, 1 to 2^depth |
| 14 | 2 | reserved, 0 |
| 16 | 3N | palette, N entries of red, green, blue, 8 bits each |
| 16+3N | height x rowBytes | pixels |

Pixels are packed most significant bit first, which is the PixMap order: at depth
4 the left pixel is the high nibble. Each value indexes the palette. The unused
nibble at the end of an odd-width row, and any padding bytes, are 0. The whole
blob is at most 80,000 bytes, which a 320 x 320 image at depth 4 fits within
(51,264). The Mac refuses anything larger without reading on, and refuses a
blob whose declared sizes do not add up to the bytes it received.

The palette is 8-bit; the Mac widens each channel to 16 bits by repeating the byte
(`0xAB` becomes `0xABAB`) when it builds the colour table.

## How the bridge makes it

All integer arithmetic, in this order, so the same input gives the same bytes:

1. Decode (JPEG or PNG only).
2. Flatten transparency onto white: `(c*a + 255*(255-a) + 127) / 255`, integer
   division.
3. Shrink to fit `w` x 320 by averaging the source pixels that fall in each
   destination pixel, each channel rounded half up (`(sum + n/2) / n`).
4. Build a palette of at most 2^depth colours, sorted ascending by
   (red, green, blue). An image with no more colours than that gets exactly its own
   colours. Otherwise it is median cut over a 5-bit-per-channel histogram.
5. Map each pixel to the nearest palette entry by squared RGB distance, the lowest
   index winning a tie. There is no dithering.

The vectors pin steps 1 to 3 and the palette order exactly for images with few
colours, which are the cases small enough to work out by hand. For images with
many colours the tests check what has to be true (the palette size, valid
indices, a bound on the average error) rather than every byte of the median cut,
so a better quantiser can replace it without breaking the contract.

## What I have not checked

The PixMap and CopyBits calls are written from the Multiversal Interfaces
(the struct layouts, `CopyBits`, `GetCTSeed`) and the stub checker compares their
signatures, but I have not seen Apple's own headers and nothing here has drawn
one of these blobs on a Mac. Two choices to look at first on hardware:

- The destination of `CopyBits` is `&window->portBits`. In a colour window that
  field aliases `portPixMap` and `portVersion` (the Multiversal `GrafPort` and
  `CGrafPort` definitions line up that way), which is how `CopyBits` is told the
  destination is a colour port. If a window turns out not to be, the alternative
  is the pixmap handle from `portPixMap`.
- The colour table gets a fresh seed from `GetCTSeed`, so QuickDraw doesn't reuse
  a stale inverse table from another image.
