#ifndef PLATINUM_IMAGE_BLOB_H
#define PLATINUM_IMAGE_BLOB_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The "PLIM" image blob the bridge sends for GET /v1/image; the format is
 * docs/IMAGES.md. This reads it and nothing else: no Toolbox, so it links on a
 * host for tests, and the drawing is image_pix.c.
 */

#define PLATINUM_IMAGE_MAX_BYTES 80000L
#define PLATINUM_IMAGE_MAX_SIDE 320

typedef struct platinum_image {
    int depth;       /* bits per pixel, 4 or 8 */
    short width;
    short height;
    short row_bytes; /* even */
    short colours;   /* palette entries, 1 to 2^depth */
    /* These point into the blob that was parsed; it must outlive them. */
    const unsigned char *palette; /* colours x (red, green, blue) */
    const unsigned char *pixels;  /* height x row_bytes, MSB first */
} platinum_image;

enum {
    PLATINUM_IMAGE_OK = 0,
    PLATINUM_IMAGE_TOO_SHORT = 1,
    PLATINUM_IMAGE_BAD_MAGIC = 2,
    PLATINUM_IMAGE_BAD_VERSION = 3,
    PLATINUM_IMAGE_BAD_DEPTH = 4,
    PLATINUM_IMAGE_BAD_SIZE = 5,      /* width or height 0 or over 320 */
    PLATINUM_IMAGE_BAD_ROWBYTES = 6,  /* not what the width and depth need */
    PLATINUM_IMAGE_BAD_COLOURS = 7,   /* 0, or more than the depth allows */
    PLATINUM_IMAGE_TOO_BIG = 8,       /* over PLATINUM_IMAGE_MAX_BYTES */
    PLATINUM_IMAGE_LENGTH_MISMATCH = 9,
    PLATINUM_IMAGE_BAD_INDEX = 10     /* a pixel names a colour that is not there */
};

/*
 * Check `blob` (length `length`) against the contract and, if it holds, fill
 * `out`. Nothing is trusted: every declared size is checked against the bytes
 * actually received before any is read, and every pixel index is checked
 * against the palette. Returns PLATINUM_IMAGE_OK or the first thing wrong; on a
 * failure `out` is zeroed.
 */
int platinum_image_parse(const unsigned char *blob, long length,
                         platinum_image *out);

#ifdef __cplusplus
}
#endif

#endif
