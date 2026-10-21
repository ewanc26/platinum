#include "image_encode.h"

#include <stdlib.h>
#include <string.h>

/*
 * Mirrors bridge/src/image/convert.ts exactly: the flatten and shrink of steps
 * 2 and 3 of docs/IMAGES.md, the exact-palette branch of step 4 (used when the
 * shrunk image has no more distinct colours than 2^depth), the median cut that
 * runs otherwise, and the nearest-colour mapping of step 5. Every product that
 * can pass 2^31 is an unsigned long.
 *
 * The median cut keeps a dense histogram over the 5-bit-per-channel bins
 * (2^15 of them), with per-bin sums, plus a per-bin box owner. Those tables are
 * static, so the encoder never allocates, but they are about 640 KB and the
 * encoder is NOT reentrant: two calls at once would share them.
 */

#define HEADER_BYTES 16UL
#define MAX_BLOB_BYTES 80000UL
#define MAX_COLOURS 256
#define BIN_COUNT 32768 /* 5 bits per channel: 2^15 */

static unsigned long bin_n[BIN_COUNT];
static unsigned long bin_r[BIN_COUNT];
static unsigned long bin_g[BIN_COUNT];
static unsigned long bin_b[BIN_COUNT];
static unsigned short bin_owner[BIN_COUNT];
static unsigned short bin_members[BIN_COUNT];
static int sort_channel;

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

/* The exact branch: the distinct colours, sorted ascending, if there are at most `limit`. */
static int exact_palette(const unsigned char *rgba, unsigned long width,
                         unsigned long height, unsigned long dw,
                         unsigned long dh, unsigned long limit,
                         unsigned long keys[], unsigned long *count)
{
    unsigned long dx;
    unsigned long dy;
    unsigned long i;
    unsigned long average[3];

    *count = 0;
    for (dy = 0; dy < dh; dy++) {
        for (dx = 0; dx < dw; dx++) {
            unsigned long key;
            int found = 0;

            destination_pixel(rgba, width, height, dw, dh, dx, dy, average);
            key = key_of(average);
            for (i = 0; i < *count && !found; i++) {
                if (keys[i] == key)
                    found = 1;
            }
            if (!found) {
                if (*count == limit)
                    return 0;
                keys[(*count)++] = key;
            }
        }
    }
    for (i = 1; i < *count; i++) {
        unsigned long v = keys[i];
        unsigned long j = i;

        while (j > 0 && keys[j - 1] > v) {
            keys[j] = keys[j - 1];
            j--;
        }
        keys[j] = v;
    }
    return 1;
}

static unsigned long bin_channel(unsigned long bin, int ch)
{
    if (ch == 0)
        return bin >> 10;
    if (ch == 1)
        return (bin >> 5) & 31UL;
    return bin & 31UL;
}

static int compare_members(const void *pa, const void *pb)
{
    unsigned long a = *(const unsigned short *)pa;
    unsigned long b = *(const unsigned short *)pb;
    unsigned long va = bin_channel(a, sort_channel);
    unsigned long vb = bin_channel(b, sort_channel);

    if (va != vb)
        return va < vb ? -1 : 1;
    if (a != b)
        return a < b ? -1 : 1;
    return 0;
}

/*
 * The median cut of convert.ts buildPalette, with the same boxes in the same
 * order. Writes the palette, sorted ascending and without duplicates, to keys
 * and returns its size, which is at most `max_colours`.
 */
static unsigned long median_cut(const unsigned char *rgba, unsigned long width,
                                unsigned long height, unsigned long dw,
                                unsigned long dh, unsigned long max_colours,
                                unsigned long keys[])
{
    static unsigned long box_bins[MAX_COLOURS];
    static unsigned long lo[3][MAX_COLOURS];
    static unsigned long hi[3][MAX_COLOURS];
    static unsigned long sum_n[MAX_COLOURS];
    static unsigned long sum_r[MAX_COLOURS];
    static unsigned long sum_g[MAX_COLOURS];
    static unsigned long sum_b[MAX_COLOURS];
    static unsigned short order[MAX_COLOURS];
    static unsigned long box_key[MAX_COLOURS];
    unsigned long dx;
    unsigned long dy;
    unsigned long bin;
    unsigned long nbox;
    unsigned long id;
    unsigned long pos;
    unsigned long m;
    unsigned long i;
    unsigned long count = 0;
    unsigned long average[3];

    memset(bin_n, 0, sizeof(bin_n));
    memset(bin_r, 0, sizeof(bin_r));
    memset(bin_g, 0, sizeof(bin_g));
    memset(bin_b, 0, sizeof(bin_b));
    for (dy = 0; dy < dh; dy++) {
        for (dx = 0; dx < dw; dx++) {
            unsigned long k;

            destination_pixel(rgba, width, height, dw, dh, dx, dy, average);
            k = ((average[0] >> 3) << 10) | ((average[1] >> 3) << 5) | (average[2] >> 3);
            bin_n[k]++;
            bin_r[k] += average[0];
            bin_g[k] += average[1];
            bin_b[k] += average[2];
        }
    }

    /* One box holds every occupied bin. Box ids are creation order; `order` is the box list. */
    nbox = 1;
    order[0] = 0;
    for (bin = 0; bin < BIN_COUNT; bin++) {
        if (bin_n[bin] > 0)
            bin_owner[bin] = 0;
    }

    while (nbox < max_colours) {
        unsigned long pick_pos = nbox;
        unsigned long pick_span = 0;
        int pick_ch;

        /* Count bins and find each box's 5-bit range, channel by channel. */
        for (id = 0; id < nbox; id++) {
            box_bins[id] = 0;
            for (i = 0; i < 3; i++) {
                lo[i][id] = 31;
                hi[i][id] = 0;
            }
        }
        for (bin = 0; bin < BIN_COUNT; bin++) {
            if (bin_n[bin] == 0)
                continue;
            id = bin_owner[bin];
            box_bins[id]++;
            for (i = 0; i < 3; i++) {
                unsigned long v = bin_channel(bin, (int)i);

                if (v < lo[i][id])
                    lo[i][id] = v;
                if (v > hi[i][id])
                    hi[i][id] = v;
            }
        }

        /* The first box in list order with the widest range; the first channel with the widest span. */
        for (pos = 0; pos < nbox; pos++) {
            unsigned long b = order[pos];
            unsigned long span = 0;
            int ch;

            if (box_bins[b] < 2)
                continue;
            for (ch = 0; ch < 3; ch++) {
                if (hi[ch][b] - lo[ch][b] > span)
                    span = hi[ch][b] - lo[ch][b];
            }
            if (span > pick_span) {
                pick_span = span;
                pick_pos = pos;
            }
        }
        if (pick_pos == nbox)
            break;

        id = order[pick_pos];
        pick_ch = 0;
        for (i = 1; i < 3; i++) {
            if (hi[i][id] - lo[i][id] > hi[pick_ch][id] - lo[pick_ch][id])
                pick_ch = (int)i;
        }

        /* Sort the box's bins by the chosen channel, then by bin key. */
        m = 0;
        for (bin = 0; bin < BIN_COUNT; bin++) {
            if (bin_n[bin] > 0 && bin_owner[bin] == id)
                bin_members[m++] = (unsigned short)bin;
        }
        sort_channel = pick_ch;
        qsort(bin_members, (size_t)m, sizeof(bin_members[0]), compare_members);

        /* The cut: the smallest k in 1..m-1 whose prefix holds half the pixels, else m-1. */
        {
            unsigned long total = 0;
            unsigned long acc = 0;
            unsigned long cut = 1;
            unsigned long new_id = nbox;

            for (i = 0; i < m; i++)
                total += bin_n[bin_members[i]];
            for (i = 0; i + 1 < m; i++) {
                acc += bin_n[bin_members[i]];
                cut = i + 1;
                if (acc * 2 >= total)
                    break;
            }
            for (i = cut; i < m; i++)
                bin_owner[bin_members[i]] = (unsigned short)new_id;

            /* splice: the low half stays at the pick position, the high half goes right after it. */
            for (i = nbox; i > pick_pos + 1; i--)
                order[i] = order[i - 1];
            order[pick_pos + 1] = (unsigned short)new_id;
            nbox++;
        }
    }

    /* The palette: each box's mean, weighted by its pixel count and rounded half up. */
    for (id = 0; id < nbox; id++) {
        sum_n[id] = 0;
        sum_r[id] = 0;
        sum_g[id] = 0;
        sum_b[id] = 0;
    }
    for (bin = 0; bin < BIN_COUNT; bin++) {
        if (bin_n[bin] == 0)
            continue;
        id = bin_owner[bin];
        sum_n[id] += bin_n[bin];
        sum_r[id] += bin_r[bin];
        sum_g[id] += bin_g[bin];
        sum_b[id] += bin_b[bin];
    }
    for (pos = 0; pos < nbox; pos++) {
        unsigned long b = order[pos];
        unsigned long n = sum_n[b];
        unsigned long mr = (sum_r[b] + n / 2) / n;
        unsigned long mg = (sum_g[b] + n / 2) / n;
        unsigned long mb = (sum_b[b] + n / 2) / n;

        box_key[pos] = (mr << 16) | (mg << 8) | mb;
    }

    /* Drop duplicate colours, then sort ascending, as the converter does. */
    for (pos = 0; pos < nbox; pos++) {
        int found = 0;

        for (i = 0; i < count && !found; i++) {
            if (keys[i] == box_key[pos])
                found = 1;
        }
        if (!found)
            keys[count++] = box_key[pos];
    }
    for (i = 1; i < count; i++) {
        unsigned long v = keys[i];
        unsigned long j = i;

        while (j > 0 && keys[j - 1] > v) {
            keys[j] = keys[j - 1];
            j--;
        }
        keys[j] = v;
    }
    return count;
}

/* Index of the nearest palette entry by squared RGB distance; the lowest index wins a tie. */
static unsigned long nearest(const unsigned long *keys, unsigned long count,
                             const unsigned long average[3])
{
    unsigned long i;
    unsigned long best = 0;
    long best_d = 0;

    for (i = 0; i < count; i++) {
        long dr = (long)((keys[i] >> 16) & 255UL) - (long)average[0];
        long dg = (long)((keys[i] >> 8) & 255UL) - (long)average[1];
        long db = (long)(keys[i] & 255UL) - (long)average[2];
        long d = dr * dr + dg * dg + db * db;

        if (i == 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
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

    if (!exact_palette(rgba, width, height, dw, dh, limit, keys, &count))
        count = median_cut(rgba, width, height, dw, dh, limit, keys);

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

    /* Pass 2: map each pixel to its nearest palette entry and pack the indices, high nibble first. */
    pixels = out + HEADER_BYTES + count * 3UL;
    for (dy = 0; dy < dh; dy++) {
        for (dx = 0; dx < dw; dx++) {
            unsigned long index;
            unsigned char *cell;

            destination_pixel(rgba, width, height, dw, dh, dx, dy, average);
            index = nearest(keys, count, average);
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
