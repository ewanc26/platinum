/*
 * PLIM encoder, C89. The steps and their integer order are those of
 * bridge/src/image/convert.ts: flatten onto white, area-average shrink, palette
 * (exact colours if there are few enough, else median cut over a 5-bit
 * histogram), nearest-colour mapping, pack. The output format is docs/IMAGES.md.
 */

#include <stdlib.h>
#include <string.h>

#include "plim.h"

#define PLIM_HEADER_BYTES 16
#define PLIM_VERSION 1
#define PLIM_MAX_BLOB_BYTES 80000UL
#define PLIM_MAX_SOURCE_PIXELS 12000000UL
#define PLIM_MAX_SIDE 320
#define PLIM_BINS 32768 /* 5 bits per channel: 2^15 */

/* One non-empty 5-bit bin. r, g and b are sums of the 8-bit values, weighted by n. */
typedef struct plim_entry {
    int key;
    unsigned long n;
    unsigned long r;
    unsigned long g;
    unsigned long b;
} plim_entry;

typedef struct plim_box {
    int *items; /* indices into the entries array */
    int count;
} plim_box;

typedef struct plim_sort {
    long k;     /* channel value * 32768 + bin key: ascending order is (value, key) */
    int index;
} plim_sort;

static int cmp_ulong(const void *a, const void *b)
{
    unsigned long x = *(const unsigned long *)a;
    unsigned long y = *(const unsigned long *)b;

    if (x < y)
        return -1;
    if (x > y)
        return 1;
    return 0;
}

static int cmp_sort(const void *a, const void *b)
{
    long x = ((const plim_sort *)a)->k;
    long y = ((const plim_sort *)b)->k;

    if (x < y)
        return -1;
    if (x > y)
        return 1;
    return 0;
}

/* Channel 0 is red, 1 green, 2 blue; each is 5 bits of a bin key. */
static int bin_value(int key, int ch)
{
    if (ch == 0)
        return key >> 10;
    if (ch == 1)
        return (key >> 5) & 31;
    return key & 31;
}

/* The widest channel span of a box; the lowest channel wins a tie. */
static int box_range(const plim_entry *entries, const int *items, int count,
                     int *ch_out)
{
    int best_span = -1;
    int best_ch = 0;
    int ch;

    for (ch = 0; ch < 3; ch++) {
        int lo = 31;
        int hi = 0;
        int i;

        for (i = 0; i < count; i++) {
            int v = bin_value(entries[items[i]].key, ch);

            if (v < lo)
                lo = v;
            if (v > hi)
                hi = v;
        }
        if (hi - lo > best_span) {
            best_span = hi - lo;
            best_ch = ch;
        }
    }
    *ch_out = best_ch;
    return best_span;
}

/*
 * The palette, as buildPalette() in convert.ts. Writes up to max_colours
 * entries of (red, green, blue) into pal, sorted ascending, and returns their
 * count, or a negative error.
 */
static int build_palette(const unsigned char *rgb, unsigned long npix,
                         int max_colours, unsigned char *pal)
{
    unsigned long *keys;
    unsigned long i;
    unsigned long u;
    unsigned long mk[256];
    plim_entry *bins;
    plim_entry *entries;
    plim_box *boxes;
    plim_sort *sorted;
    int n_entries;
    int nboxes;
    int result;
    int j;

    keys = malloc(npix * sizeof(unsigned long));
    if (keys == NULL)
        return PLIM_ERR_NOMEM;
    for (i = 0; i < npix; i++)
        keys[i] = ((unsigned long)rgb[3 * i] << 16) |
                  ((unsigned long)rgb[3 * i + 1] << 8) |
                  (unsigned long)rgb[3 * i + 2];
    qsort(keys, npix, sizeof(unsigned long), cmp_ulong);
    u = 0;
    for (i = 0; i < npix; i++) {
        if (u == 0 || keys[i] != keys[u - 1])
            keys[u++] = keys[i];
    }
    if (u <= (unsigned long)max_colours) {
        for (i = 0; i < u; i++) {
            pal[3 * i] = (unsigned char)((keys[i] >> 16) & 255);
            pal[3 * i + 1] = (unsigned char)((keys[i] >> 8) & 255);
            pal[3 * i + 2] = (unsigned char)(keys[i] & 255);
        }
        free(keys);
        return (int)u;
    }
    free(keys);

    /* Too many colours: the 5-bit histogram, then median cut. */
    bins = calloc(PLIM_BINS, sizeof(plim_entry));
    if (bins == NULL)
        return PLIM_ERR_NOMEM;
    for (i = 0; i < npix; i++) {
        unsigned long r = rgb[3 * i];
        unsigned long g = rgb[3 * i + 1];
        unsigned long b = rgb[3 * i + 2];
        int bk = (int)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
        bins[bk].n += 1;
        bins[bk].r += r;
        bins[bk].g += g;
        bins[bk].b += b;
    }
    n_entries = 0;
    for (j = 0; j < PLIM_BINS; j++) {
        if (bins[j].n > 0)
            n_entries++;
    }
    entries = malloc((size_t)n_entries * sizeof(plim_entry));
    boxes = malloc((size_t)max_colours * sizeof(plim_box));
    sorted = NULL;
    if (entries == NULL || boxes == NULL) {
        free(entries);
        free(boxes);
        free(bins);
        return PLIM_ERR_NOMEM;
    }
    {
        int e = 0;

        for (j = 0; j < PLIM_BINS; j++) {
            if (bins[j].n > 0) {
                entries[e] = bins[j];
                entries[e].key = j;
                e++;
            }
        }
    }
    free(bins);
    bins = NULL;

    boxes[0].items = malloc((size_t)n_entries * sizeof(int));
    if (boxes[0].items == NULL) {
        free(entries);
        free(boxes);
        return PLIM_ERR_NOMEM;
    }
    for (j = 0; j < n_entries; j++)
        boxes[0].items[j] = j;
    boxes[0].count = n_entries;
    nboxes = 1;
    result = PLIM_OK;

    while (nboxes < max_colours) {
        int pick = -1;
        int pick_span = 0;
        int ch = 0;
        int i2;
        plim_box *box;
        unsigned long total;
        unsigned long acc;
        int cut;
        int k;
        int count;
        int *left;
        int *right;

        for (i2 = 0; i2 < nboxes; i2++) {
            int c;
            int span;

            if (boxes[i2].count < 2)
                continue;
            span = box_range(entries, boxes[i2].items, boxes[i2].count, &c);
            if (span > pick_span) {
                pick = i2;
                pick_span = span;
            }
        }
        if (pick < 0)
            break;

        box = &boxes[pick];
        count = box->count;
        box_range(entries, box->items, count, &ch);
        sorted = malloc((size_t)count * sizeof(plim_sort));
        if (sorted == NULL) {
            result = PLIM_ERR_NOMEM;
            break;
        }
        total = 0;
        for (k = 0; k < count; k++) {
            const plim_entry *e = &entries[box->items[k]];

            sorted[k].k = (long)bin_value(e->key, ch) * PLIM_BINS + e->key;
            sorted[k].index = box->items[k];
            total += e->n;
        }
        qsort(sorted, (size_t)count, sizeof(plim_sort), cmp_sort);

        /* The split: the first index whose running count reaches half, but never the last. */
        acc = 0;
        cut = 1;
        for (k = 0; k < count - 1; k++) {
            acc += entries[sorted[k].index].n;
            cut = k + 1;
            if (acc * 2 >= total)
                break;
        }

        left = malloc((size_t)cut * sizeof(int));
        right = malloc((size_t)(count - cut) * sizeof(int));
        if (left == NULL || right == NULL) {
            free(left);
            free(right);
            free(sorted);
            sorted = NULL;
            result = PLIM_ERR_NOMEM;
            break;
        }
        for (k = 0; k < cut; k++)
            left[k] = sorted[k].index;
        for (k = cut; k < count; k++)
            right[k - cut] = sorted[k].index;
        free(sorted);
        sorted = NULL;

        /* boxes.splice(pick, 1, left, right) */
        free(box->items);
        for (i2 = nboxes; i2 > pick + 1; i2--)
            boxes[i2] = boxes[i2 - 1];
        boxes[pick].items = left;
        boxes[pick].count = cut;
        boxes[pick + 1].items = right;
        boxes[pick + 1].count = count - cut;
        nboxes++;
    }

    if (result == PLIM_OK) {
        for (j = 0; j < nboxes; j++) {
            unsigned long n = 0;
            unsigned long rs = 0;
            unsigned long gs = 0;
            unsigned long bs = 0;
            int k;

            for (k = 0; k < boxes[j].count; k++) {
                const plim_entry *e = &entries[boxes[j].items[k]];

                n += e->n;
                rs += e->r;
                gs += e->g;
                bs += e->b;
            }
            /* (sum + n/2) / n, integer: the mean rounded half up. */
            rs = (rs + n / 2) / n;
            gs = (gs + n / 2) / n;
            bs = (bs + n / 2) / n;
            mk[j] = (rs << 16) | (gs << 8) | bs;
        }
        qsort(mk, (size_t)nboxes, sizeof(unsigned long), cmp_ulong);
        u = 0;
        for (j = 0; j < nboxes; j++) {
            if (u == 0 || mk[j] != mk[u - 1])
                mk[u++] = mk[j];
        }
        for (j = 0; j < (int)u; j++) {
            pal[3 * j] = (unsigned char)((mk[j] >> 16) & 255);
            pal[3 * j + 1] = (unsigned char)((mk[j] >> 8) & 255);
            pal[3 * j + 2] = (unsigned char)(mk[j] & 255);
        }
        result = (int)u;
    }

    for (j = 0; j < nboxes; j++)
        free(boxes[j].items);
    free(boxes);
    free(entries);
    free(sorted);
    return result;
}

/* Index of the nearest palette entry by squared RGB distance; the lowest index wins a tie. */
static int nearest(const unsigned char *pal, int count, int r, int g, int b)
{
    int best = 0;
    long best_d = 0;
    int i;

    for (i = 0; i < count; i++) {
        long dr = (long)pal[3 * i] - r;
        long dg = (long)pal[3 * i + 1] - g;
        long db = (long)pal[3 * i + 2] - b;
        long d = dr * dr + dg * dg + db * db;

        if (i == 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
}

/* Flatten onto white: (c*a + 255*(255-a) + 127) / 255, integer. */
static void flatten(const unsigned char *rgba, unsigned long npix,
                    unsigned char *rgb)
{
    unsigned long i;
    int c;

    for (i = 0; i < npix; i++) {
        int a = rgba[4 * i + 3];

        for (c = 0; c < 3; c++) {
            int v = rgba[4 * i + c];

            rgb[3 * i + c] = (unsigned char)(a == 255
                ? v
                : (v * a + 255 * (255 - a) + 127) / 255);
        }
    }
}

/*
 * Area-average shrink to fit max_w x max_h, never enlarging, never below 1x1,
 * as shrink() in convert.ts. Allocates *dst (dw x dh x 3).
 */
static int shrink(const unsigned char *src, int width, int height,
                  int max_w, int max_h, unsigned char **dst, int *dw_out,
                  int *dh_out)
{
    unsigned long uw = (unsigned long)width;
    unsigned long uh = (unsigned long)height;
    unsigned long dw = uw;
    unsigned long dh = uh;
    unsigned long dy;
    unsigned long dx;
    unsigned char *o;

    if (uw > (unsigned long)max_w || uh > (unsigned long)max_h) {
        if ((unsigned long)max_w * uh <= (unsigned long)max_h * uw) {
            dw = (unsigned long)max_w;
            dh = (uh * (unsigned long)max_w) / uw;
            if (dh < 1)
                dh = 1;
        } else {
            dh = (unsigned long)max_h;
            dw = (uw * (unsigned long)max_h) / uh;
            if (dw < 1)
                dw = 1;
        }
    }

    o = malloc((size_t)(dw * dh * 3));
    if (o == NULL)
        return PLIM_ERR_NOMEM;

    for (dy = 0; dy < dh; dy++) {
        unsigned long y0 = (dy * uh) / dh;
        unsigned long y1 = ((dy + 1) * uh) / dh;
        unsigned long x0;
        unsigned long x1;
        unsigned long y;
        unsigned long x;
        int c;

        if (y1 < y0 + 1)
            y1 = y0 + 1;
        for (dx = 0; dx < dw; dx++) {
            unsigned long n;

            x0 = (dx * uw) / dw;
            x1 = ((dx + 1) * uw) / dw;
            if (x1 < x0 + 1)
                x1 = x0 + 1;
            n = (y1 - y0) * (x1 - x0);
            for (c = 0; c < 3; c++) {
                unsigned long sum = 0;

                for (y = y0; y < y1; y++) {
                    for (x = x0; x < x1; x++)
                        sum += src[(y * uw + x) * 3 + c];
                }
                o[(dy * dw + dx) * 3 + c] =
                    (unsigned char)((sum + n / 2) / n);
            }
        }
    }

    *dst = o;
    *dw_out = (int)dw;
    *dh_out = (int)dh;
    return PLIM_OK;
}

int plim_encode(const unsigned char *rgba, int width, int height,
                int max_width, int max_height, int colours,
                unsigned char **out, long *out_len)
{
    unsigned long npix;
    int depth;
    unsigned char *flat;
    unsigned char *small;
    unsigned char palette[3 * 256];
    int sw;
    int sh;
    int ncol;
    long rb;
    long total;
    long o;
    unsigned char *blob;
    int status;
    long y;
    long x;

    if (out != NULL)
        *out = NULL;
    if (out_len != NULL)
        *out_len = 0;
    if (rgba == NULL || out == NULL || out_len == NULL)
        return PLIM_ERR_ARGS;
    if (max_width < 1 || max_width > PLIM_MAX_SIDE ||
        max_height < 1 || max_height > PLIM_MAX_SIDE)
        return PLIM_ERR_ARGS;
    if (colours == 16)
        depth = 4;
    else if (colours == 256)
        depth = 8;
    else
        return PLIM_ERR_ARGS;
    if (width < 1 || height < 1)
        return PLIM_ERR_INPUT;
    npix = (unsigned long)width * (unsigned long)height;
    if (npix > PLIM_MAX_SOURCE_PIXELS)
        return PLIM_ERR_INPUT;

    flat = malloc((size_t)(npix * 3));
    if (flat == NULL)
        return PLIM_ERR_NOMEM;
    flatten(rgba, npix, flat);

    status = shrink(flat, width, height, max_width, max_height, &small, &sw, &sh);
    free(flat);
    if (status != PLIM_OK)
        return status;

    ncol = build_palette(small, (unsigned long)sw * (unsigned long)sh,
                         colours, palette);
    if (ncol < 0) {
        free(small);
        return ncol;
    }

    /* Whole bytes per row, rounded up to even. */
    rb = ((long)sw * depth + 7) / 8;
    rb += rb % 2;
    total = PLIM_HEADER_BYTES + (long)ncol * 3 + (long)sh * rb;
    if ((unsigned long)total > PLIM_MAX_BLOB_BYTES) {
        free(small);
        return PLIM_ERR_TOO_LARGE;
    }

    blob = calloc((size_t)total, 1);
    if (blob == NULL) {
        free(small);
        return PLIM_ERR_NOMEM;
    }
    blob[0] = 'P';
    blob[1] = 'L';
    blob[2] = 'I';
    blob[3] = 'M';
    blob[4] = PLIM_VERSION;
    blob[5] = (unsigned char)depth;
    blob[6] = (unsigned char)((sw >> 8) & 255);
    blob[7] = (unsigned char)(sw & 255);
    blob[8] = (unsigned char)((sh >> 8) & 255);
    blob[9] = (unsigned char)(sh & 255);
    blob[10] = (unsigned char)((rb >> 8) & 255);
    blob[11] = (unsigned char)(rb & 255);
    blob[12] = (unsigned char)((ncol >> 8) & 255);
    blob[13] = (unsigned char)(ncol & 255);
    /* blob[14], blob[15] are reserved and stay 0. */

    o = PLIM_HEADER_BYTES;
    memcpy(blob + o, palette, (size_t)ncol * 3);
    o += (long)ncol * 3;

    for (y = 0; y < sh; y++) {
        for (x = 0; x < sw; x++) {
            long i = (y * sw + x) * 3;
            int idx = nearest(palette, ncol, small[i], small[i + 1], small[i + 2]);
            long base = o + y * rb;

            if (depth == 8)
                blob[base + x] = (unsigned char)idx;
            else if (x % 2 == 0)
                blob[base + x / 2] |= (unsigned char)(idx << 4);
            else
                blob[base + x / 2] |= (unsigned char)idx;
        }
    }

    free(small);
    *out = blob;
    *out_len = total;
    return PLIM_OK;
}
