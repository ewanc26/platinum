/*
 * GET /v1/image: the image ref and parameters, and the route (Part 9).
 *
 * The rules are those of bridge/src/image/fetch.ts and convert.ts. Decision on #118:
 * the JS flattens alpha onto white before it shrinks, and the PLIM encoder
 * (native/plim) does the same from RGBA, byte for byte against the vectors. The native
 * path therefore decodes the full image and never uses Wolfram's premultiplied
 * shrink. Its source limit is Wolfram's 2048 pixels per side, stricter than the JS's
 * 12 million pixels; a larger source is refused with 413, and this is recorded in
 * docs/BRIDGE_PROTOCOL.md.
 *
 * The bytes of the source are fetched and decoded by callbacks, so this file needs
 * neither the network nor Wolfram.
 */
#ifndef PLATINUM_NATIVE_IMAGE_ROUTE_H
#define PLATINUM_NATIVE_IMAGE_ROUTE_H

#include <stddef.h>

/* The ref as fetch.ts accepts it: avatar, avatar_thumbnail, feed_thumbnail or
 * feed_fullsize, then /plain/<did>/<cid>@jpeg or @png, at most 300 bytes. 0 ok, -1 not. */
int platinum_valid_image_ref(const char *ref);

/* w (16 to 320, default 320) and depth (4 or 8, default 4), as parseImageParams reads
 * them. w and depth may be NULL when absent. Returns 0 and fills the outputs, or -1. */
int platinum_parse_image_params(const char *w, const char *depth, int *width, int *colours);

typedef struct platinum_rgba {
    unsigned char *pixels;   /* width * height * 4 bytes, owned by the callee's allocator */
    int width;
    int height;
} platinum_rgba;

typedef enum platinum_image_result {
    PLATINUM_IMAGE_OK = 0,
    PLATINUM_IMAGE_TOO_LARGE,   /* a source over the decoder's limit */
    PLATINUM_IMAGE_ERROR        /* could not be fetched or decoded */
} platinum_image_result;

/* Fetches the source for ref and decodes it to RGBA. The route frees the pixels with
 * free(). */
typedef platinum_image_result (*platinum_image_source_fn)(void *ctx, const char *ref,
                                                          platinum_rgba *out);

/* Writes the PLIM blob into out (a malloc'd buffer the caller frees) and its length.
 * Returns the HTTP status: 200 with the blob, 400 for a bad ref or parameter, 413 when
 * the source or the result is too large, 502 when the source cannot be fetched. */
int platinum_route_image(void *ctx, platinum_image_source_fn fn, const char *ref,
                         const char *w, const char *depth,
                         unsigned char **out, long *out_len);

#endif
