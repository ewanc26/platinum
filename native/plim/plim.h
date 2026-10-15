#ifndef PLATINUM_PLIM_H
#define PLATINUM_PLIM_H

/*
 * PLIM encoder: decoded RGBA8 pixels in, the "PLIM" version 1 blob out (see
 * docs/IMAGES.md). It does the same steps, in the same integer order, as
 * bridge/src/image/convert.ts, so the bytes match the TypeScript for the same
 * input. It does not decode JPEG or PNG; the caller does that.
 *
 * rgba        width x height x 4 bytes, R G B A, not premultiplied.
 * max_width   widest output picture, 1 to 320 (the bridge's w parameter).
 * max_height  tallest output picture, 1 to 320 (the bridge uses 320).
 * colours     16 (depth 4) or 256 (depth 8).
 *
 * On PLIM_OK, *out points at a malloc'd blob of *out_len bytes; the caller
 * frees it with free(). On any other return, *out is NULL and nothing leaks.
 */

#define PLIM_OK 0
#define PLIM_ERR_ARGS (-1)      /* a NULL pointer, or a bad size or colour count */
#define PLIM_ERR_INPUT (-2)     /* source image empty, or over 12 million pixels */
#define PLIM_ERR_TOO_LARGE (-3) /* the blob would be over 80,000 bytes */
#define PLIM_ERR_NOMEM (-4)

int plim_encode(const unsigned char *rgba, int width, int height,
                int max_width, int max_height, int colours,
                unsigned char **out, long *out_len);

#endif
