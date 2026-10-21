#ifndef PLATINUM_IMAGE_ENCODE_H
#define PLATINUM_IMAGE_ENCODE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The encoder half of docs/IMAGES.md: decoded RGBA pixels in, a "PLIM" blob out.
 * The bytes are the bridge's (bridge/src/image/convert.ts) for every image,
 * including the median cut that runs when the shrunk image has more colours than
 * 2^depth. The quantiser is part of the contract: change it here and in
 * convert.ts together. Reads nothing from the Toolbox, so it links on a host.
 *
 * Not reentrant: the median cut uses static tables of about 640 KB.
 */

#define PLATINUM_ENCODE_MAX_SOURCE_PIXELS 12000000L
#define PLATINUM_ENCODE_MAX_SIDE 320

enum {
    PLATINUM_ENCODE_OK = 0,
    PLATINUM_ENCODE_BAD_ARG = 1,        /* NULL pointer, size, depth or bound out of range */
    PLATINUM_ENCODE_TOO_LARGE = 2,      /* the blob would be over 80,000 bytes */
    PLATINUM_ENCODE_BUFFER_SMALL = 3    /* out_size is short; *out_length holds the size needed */
};

/*
 * Encode `rgba` (width x height pixels, R G B A bytes, row by row) into `out`.
 * Alpha is flattened onto white, the image is shrunk to fit max_width x
 * max_height without enlarging, and each pixel is mapped to the palette.
 * depth is 4 or 8; max_width and max_height are 1 to 320.
 *
 * On PLATINUM_ENCODE_OK, *out_length is the blob length. On BUFFER_SMALL it is
 * the length the blob needs. On any other failure it is 0.
 */
int platinum_image_encode(const unsigned char *rgba, long width, long height,
                          int max_width, int max_height, int depth,
                          unsigned char *out, long out_size, long *out_length);

#ifdef __cplusplus
}
#endif

#endif
