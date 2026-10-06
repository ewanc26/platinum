#include "image_blob.h"

#include <stddef.h>
#include <string.h>

#define HEADER_BYTES 16L

static long be16(const unsigned char *p)
{
    return ((long)p[0] << 8) | (long)p[1];
}

int platinum_image_parse(const unsigned char *blob, long length,
                         platinum_image *out)
{
    long depth;
    long width;
    long height;
    long row_bytes;
    long colours;
    long need_row;
    long total;
    long x;
    long y;
    const unsigned char *row;

    if (out != NULL)
        memset(out, 0, sizeof(*out));
    if (blob == NULL || out == NULL || length < HEADER_BYTES)
        return PLATINUM_IMAGE_TOO_SHORT;
    if (length > PLATINUM_IMAGE_MAX_BYTES)
        return PLATINUM_IMAGE_TOO_BIG;
    if (blob[0] != 'P' || blob[1] != 'L' || blob[2] != 'I' || blob[3] != 'M')
        return PLATINUM_IMAGE_BAD_MAGIC;
    if (blob[4] != 1)
        return PLATINUM_IMAGE_BAD_VERSION;
    depth = blob[5];
    if (depth != 4 && depth != 8)
        return PLATINUM_IMAGE_BAD_DEPTH;
    width = be16(blob + 6);
    height = be16(blob + 8);
    if (width < 1 || height < 1 || width > PLATINUM_IMAGE_MAX_SIDE ||
        height > PLATINUM_IMAGE_MAX_SIDE)
        return PLATINUM_IMAGE_BAD_SIZE;
    row_bytes = be16(blob + 10);
    need_row = (width * depth + 7) / 8;
    need_row += need_row % 2;
    if (row_bytes != need_row)
        return PLATINUM_IMAGE_BAD_ROWBYTES;
    colours = be16(blob + 12);
    if (colours < 1 || colours > (1L << depth))
        return PLATINUM_IMAGE_BAD_COLOURS;

    total = HEADER_BYTES + colours * 3 + height * row_bytes;
    if (total > PLATINUM_IMAGE_MAX_BYTES)
        return PLATINUM_IMAGE_TOO_BIG;
    if (total != length)
        return PLATINUM_IMAGE_LENGTH_MISMATCH;

    out->palette = blob + HEADER_BYTES;
    out->pixels = blob + HEADER_BYTES + colours * 3;
    for (y = 0; y < height; ++y) {
        row = out->pixels + y * row_bytes;
        for (x = 0; x < width; ++x) {
            long index = depth == 8 ? row[x]
                                    : ((x & 1) ? (row[x >> 1] & 15)
                                               : (row[x >> 1] >> 4));
            if (index >= colours) {
                memset(out, 0, sizeof(*out));
                return PLATINUM_IMAGE_BAD_INDEX;
            }
        }
    }

    out->depth = (int)depth;
    out->width = (short)width;
    out->height = (short)height;
    out->row_bytes = (short)row_bytes;
    out->colours = (short)colours;
    return PLATINUM_IMAGE_OK;
}
