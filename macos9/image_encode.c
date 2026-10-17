#include "image_encode.h"

#include <string.h>

/*
 * Mirrors bridge/src/image/convert.ts, steps 2, 3 and 5 of docs/IMAGES.md and
 * the exact-palette branch of step 4. Every product that can pass 2^31 is an
 * unsigned long: a 12-million-pixel source times a 320-pixel bound is 3.84e9.
 * Nothing here allocates: the caller's buffer receives the blob, and each
 * destination pixel is recomputed from the source on demand, in two passes.
 */

#define HEADER_BYTES 16UL
#define MAX_BLOB_BYTES 80000UL
#define MAX_COLOURS 256

/* Alpha onto white: (c*a + 255*(255-a) + 127) / 255, integer. */
static unsigned long flat_channel(const unsigned char *px, int c)
{
    unsigned long a = px[3];
    unsigned long v = px[c];

    if (a == 255UL)
        return v;
    return (v * a + 255UL * (255UL - a) + 127UL) / 255UL;
}

/*
 * The destination size, as convert.ts shrink(): scale by min(maxW/w, maxH/h),
 * never enlarge, never below 1 pixel on either side.
 */
static void destination_size(unsigned long width, unsigned long height,
                             unsigned long max_w, unsigned long max_h,
                             unsigned long *dw, unsigned long *dh)
{
    *dw = width;
    *dh = height;
    if (width <= max_w && height <= max_h)
        return;
    if (max_w * height <= max_h * width) {
        *dw = max_w;
        *dh = height * max_w / width;
    } else {
        *dh = max_h;
        *dw = width * max_h / height;
    }
    if (*dw < 1)
        *dw = 1;
    if (*dh < 1)
        *dh = 1;
}

/*
 * Area average of destination pixel (dx, dy): the flattened source pixels in
 * [floor(dx*w/dw), floor((dx+1)*w/dw)) by the same for rows, each channel
 * rounded half up. A block is never empty.
 */
static void destination_pixel(const unsigned char *rgba, unsigned long width,
                              unsigned long height, unsigned long dw,
                              unsigned long dh, unsigned long dx,
                              unsigned long dy, unsigned long average[3])
{
    unsigned long y0 = dy * height / dh;
    unsigned long y1 = (dy + 1) * height / dh;
    unsigned long x0 = dx * width / dw;
    unsigned long x1 = (dx + 1) * width / dw;
    unsigned long x;
    unsigned long y;
    unsigned long n;
    unsigned long sum[3];
    int c;

    if (y1 <= y0)
        y1 = y0 + 1;
    if (x1 <= x0)
        x1 = x0 + 1;
    n = (y1 - y0) * (x1 - x0);
    sum[0] = 0;
    sum[1] = 0;
    sum[2] = 0;
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            const unsigned char *px = rgba + (y * width + x) * 4;

            for (c = 0; c < 3; c++)
                sum[c] += flat_channel(px, c);
        }
    }
    for (c = 0; c < 3; c++)
        average[c] = (sum[c] + n / 2) / n;
}

static unsigned long key_of(const unsigned long average[3])
{
    return (average[0] << 16) | (average[1] << 8) | average[2];
}

static long index_of(const unsigned long *keys, unsigned long count,
                     unsigned long key)
{
    unsigned long i;

    for (i = 0; i < count; i++) {
        if (keys[i] == key)
            return (long)i;
    }
    return -1;
}

static void put16(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)((v >> 8) & 255UL);
    p[1] = (unsigned char)(v & 255UL);
}

int platinum_image_encode(const unsigned char *rgba, long width_arg,
                          long height_arg, int max_width, int max_height,
                          int depth, unsigned char *out, long out_size,
                          long *out_length)
{
    unsigned long width;
    unsigned long height;
    unsigned long dw;
    unsigned long dh;
    unsigned long limit;
    unsigned long keys[MAX_COLOURS];
    unsigned long count = 0;
    unsigned long rb;
    unsigned long total;
    unsigned long dx;
    unsigned long dy;
    unsigned long i;
    unsigned long average[3];
    unsigned char *pixels;
    unsigned long row_pad;

    if (out_length != NULL)
        *out_length = 0;
    if (rgba == NULL || out == NULL || out_length == NULL)
        return PLATINUM_ENCODE_BAD_ARG;
    if (width_arg < 1 || height_arg < 1 ||
        width_arg > PLATINUM_ENCODE_MAX_SOURCE_PIXELS / height_arg)
        return PLATINUM_ENCODE_BAD_ARG;
    if ((depth != 4 && depth != 8) || out_size < 0)
        return PLATINUM_ENCODE_BAD_ARG;
    if (max_width < 1 || max_width > PLATINUM_ENCODE_MAX_SIDE ||
        max_height < 1 || max_height > PLATINUM_ENCODE_MAX_SIDE)
        return PLATINUM_ENCODE_BAD_ARG;

    width = (unsigned long)width_arg;
    height = (unsigned long)height_arg;
    destination_size(width, height, (unsigned long)max_width,
                     (unsigned long)max_height, &dw, &dh);
    limit = depth == 8 ? 256UL : 16UL;

    /* Pass 1: the distinct colours of the shrunk image, which must fit the palette. */
    for (dy = 0; dy < dh; dy++) {
        for (dx = 0; dx < dw; dx++) {
            unsigned long key;

            destination_pixel(rgba, width, height, dw, dh, dx, dy, average);
            key = key_of(average);
            if (index_of(keys, count, key) < 0) {
                if (count == limit)
                    return PLATINUM_ENCODE_TOO_MANY_COLOURS;
                keys[count++] = key;
            }
        }
    }
    /* Sorted ascending by (red, green, blue): an insertion sort of at most 256 keys. */
    for (i = 1; i < count; i++) {
        unsigned long v = keys[i];
        unsigned long j = i;

        while (j > 0 && keys[j - 1] > v) {
            keys[j] = keys[j - 1];
            j--;
        }
        keys[j] = v;
    }

    rb = (dw * (unsigned long)depth + 7UL) / 8UL;
    row_pad = rb % 2UL;
    rb += row_pad;
    total = HEADER_BYTES + count * 3UL + dh * rb;
    if (total > MAX_BLOB_BYTES)
        return PLATINUM_ENCODE_TOO_LARGE;
    *out_length = (long)total;
    if ((unsigned long)out_size < total)
        return PLATINUM_ENCODE_BUFFER_SMALL;

    memset(out, 0, (size_t)total);
    out[0] = 'P';
    out[1] = 'L';
    out[2] = 'I';
    out[3] = 'M';
    out[4] = 1;
    out[5] = (unsigned char)depth;
    put16(out + 6, dw);
    put16(out + 8, dh);
    put16(out + 10, rb);
    put16(out + 12, count);
    put16(out + 14, 0);
    for (i = 0; i < count; i++) {
        out[HEADER_BYTES + i * 3 + 0] = (unsigned char)((keys[i] >> 16) & 255UL);
        out[HEADER_BYTES + i * 3 + 1] = (unsigned char)((keys[i] >> 8) & 255UL);
        out[HEADER_BYTES + i * 3 + 2] = (unsigned char)(keys[i] & 255UL);
    }

    /* Pass 2: pack the indices, most significant bit first. */
    pixels = out + HEADER_BYTES + count * 3UL;
    for (dy = 0; dy < dh; dy++) {
        for (dx = 0; dx < dw; dx++) {
            unsigned long index;
            unsigned char *cell;

            destination_pixel(rgba, width, height, dw, dh, dx, dy, average);
            index = (unsigned long)index_of(keys, count, key_of(average));
            cell = pixels + dy * rb;
            if (depth == 8) {
                cell[dx] = (unsigned char)index;
            } else if (dx % 2 == 0) {
                cell[dx / 2] |= (unsigned char)(index << 4);
            } else {
                cell[dx / 2] |= (unsigned char)index;
            }
        }
    }
    return PLATINUM_ENCODE_OK;
}
