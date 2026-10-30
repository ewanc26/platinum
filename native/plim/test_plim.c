/*
 * Feeds the converter vectors of bridge/test/vectors/image/convert.json to
 * plim_encode and checks the blob byte for byte. The inputs and expected bytes
 * below are copied from that file. Exits non-zero if any case fails.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plim.h"

struct vector {
    const char *name;
    int width;
    int height;
    int max_width;
    int max_height;
    int depth;
    const char *rgba;
    const char *blob;
};

static const struct vector vectors[] = {
    {
        "four colours, no change",
        2, 2, 320, 320, 4,
        "ff0000ff00ff00ff0000ffffffffffff",
        "504c494d0104000200020002000400000000ff00ff00ff0000ffffff21000300"
    },
    {
        "area average shrink, rounded half up",
        4, 4, 2, 2, 4,
        "000000ff010101ff000000ff000000ff010101ff010101ff000000ff020202ffffffffffffffffff000000ff000000fffffffffffefefeff010101ff010101ff",
        "504c494d010400020002000200020000010101ffffff00001000"
    },
    {
        "alpha is flattened onto white",
        1, 1, 320, 320, 4,
        "ff000080",
        "504c494d010400010001000200010000ff7f7f0000"
    },
    {
        "eight bit depth",
        1, 1, 320, 320, 8,
        "ff000080",
        "504c494d010800010001000200010000ff7f7f0000"
    }
};

#define VECTOR_COUNT ((int)(sizeof(vectors) / sizeof(vectors[0])))

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Decodes a hex string into a malloc'd buffer; *len receives the byte count. */
static unsigned char *from_hex(const char *hex, long *len)
{
    long n = (long)strlen(hex) / 2;
    unsigned char *buf = malloc((size_t)(n > 0 ? n : 1));
    long i;

    if (buf == NULL)
        return NULL;
    for (i = 0; i < n; i++) {
        int hi = hex_value(hex[2 * i]);
        int lo = hex_value(hex[2 * i + 1]);

        if (hi < 0 || lo < 0) {
            free(buf);
            return NULL;
        }
        buf[i] = (unsigned char)((hi << 4) | lo);
    }
    *len = n;
    return buf;
}

static int run_vector(const struct vector *v)
{
    long rgba_len = 0;
    long want_len = 0;
    unsigned char *rgba;
    unsigned char *want;
    unsigned char *got = NULL;
    long got_len = 0;
    int status;
    long i;
    int ok;

    rgba = from_hex(v->rgba, &rgba_len);
    want = from_hex(v->blob, &want_len);
    if (rgba == NULL || want == NULL) {
        printf("FAIL %s: bad fixture hex\n", v->name);
        free(rgba);
        free(want);
        return 0;
    }
    if (rgba_len != (long)v->width * v->height * 4) {
        printf("FAIL %s: fixture RGBA is %ld bytes, expected %ld\n", v->name,
               rgba_len, (long)v->width * v->height * 4);
        free(rgba);
        free(want);
        return 0;
    }

    status = plim_encode(rgba, v->width, v->height, v->max_width,
                         v->max_height, 1 << v->depth, &got, &got_len);
    ok = status == PLIM_OK && got_len == want_len;
    if (ok) {
        for (i = 0; i < want_len; i++) {
            if (got[i] != want[i]) {
                ok = 0;
                break;
            }
        }
    }

    if (ok) {
        printf("PASS %s (%ld bytes)\n", v->name, got_len);
    } else if (status != PLIM_OK) {
        printf("FAIL %s: plim_encode returned %d\n", v->name, status);
    } else if (got_len != want_len) {
        printf("FAIL %s: length %ld, expected %ld\n", v->name, got_len, want_len);
    } else {
        for (i = 0; i < want_len && got[i] == want[i]; i++)
            ;
        printf("FAIL %s: first differing byte at offset %ld: got %02x, expected %02x\n",
               v->name, i, got[i], want[i]);
    }

    free(got);
    free(rgba);
    free(want);
    return ok;
}

/* Calls that must be refused, without any output being produced. */
static int run_refusals(void)
{
    static const unsigned char px[4] = { 255, 0, 0, 255 };
    unsigned char *out = NULL;
    long out_len = 0;
    int failures = 0;

    if (plim_encode(px, 1, 1, 320, 320, 3, &out, &out_len) != PLIM_ERR_ARGS ||
        out != NULL) {
        printf("FAIL refusal: colours 3 was not rejected\n");
        failures++;
    }
    if (plim_encode(px, 1, 1, 0, 320, 16, &out, &out_len) != PLIM_ERR_ARGS) {
        printf("FAIL refusal: max_width 0 was not rejected\n");
        failures++;
    }
    if (plim_encode(px, 0, 1, 320, 320, 16, &out, &out_len) != PLIM_ERR_INPUT) {
        printf("FAIL refusal: zero width was not rejected\n");
        failures++;
    }
    if (failures == 0)
        printf("PASS refusals (3 bad calls rejected)\n");
    return failures == 0;
}

int main(void)
{
    int passed = 0;
    int failed = 0;
    int i;

    for (i = 0; i < VECTOR_COUNT; i++) {
        if (run_vector(&vectors[i]))
            passed++;
        else
            failed++;
    }
    if (run_refusals())
        passed++;
    else
        failed++;

    printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
