/*
 * test_image_encode.c -- host-side tests for the image encoder. The cases between
 * the VECTOR_CASES markers are the converter vectors in
 * bridge/test/vectors/image/convert.json (tools/check-image-vectors.py keeps them
 * identical). The hand-derived cases are worked out from docs/IMAGES.md, not
 * from running the encoder, so they can catch an encoder that agrees with
 * itself and not with the format. Every output is read back with
 * platinum_image_parse. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_blob.h"
#include "image_encode.h"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

/* VECTOR_CASES_BEGIN */
static const char *kCases[] = {
    "2 2 320 320 4 ff0000ff00ff00ff0000ffffffffffff 504c494d0104000200020002000400000000ff00ff00ff0000ffffff21000300",
    "4 4 2 2 4 000000ff010101ff000000ff000000ff010101ff010101ff000000ff020202ffffffffffffffffff000000ff000000fffffffffffefefeff010101ff010101ff 504c494d010400020002000200020000010101ffffff00001000",
    "1 1 320 320 4 ff000080 504c494d010400010001000200010000ff7f7f0000",
    "1 1 320 320 8 ff000080 504c494d010800010001000200010000ff7f7f0000",
};
/* VECTOR_CASES_END */

static unsigned char pixels[4096];
static unsigned char expected[4096];
static unsigned char out[PLATINUM_IMAGE_MAX_BYTES];
static unsigned char big[PLATINUM_IMAGE_MAX_SIDE * PLATINUM_IMAGE_MAX_SIDE * 4];

static long unhex(const char *hex, unsigned char *dest)
{
    long n = 0;

    while (hex[0] != '\0' && hex[1] != '\0') {
        unsigned int byte;

        sscanf(hex, "%2x", &byte);
        dest[n++] = (unsigned char)byte;
        hex += 2;
    }
    return n;
}

/* Encode and check the bytes against `hex`, and read the result back. */
static void expect_blob(const char *what, const unsigned char *rgba, long width,
                        long height, int max_w, int max_h, int depth,
                        const char *hex)
{
    long want = unhex(hex, expected);
    long length = -1;
    platinum_image image;
    int status = platinum_image_encode(rgba, width, height, max_w, max_h, depth,
                                       out, (long)sizeof(out), &length);

    check(status == PLATINUM_ENCODE_OK, what);
    check(length == want && memcmp(out, expected, (size_t)want) == 0, what);
    check(platinum_image_parse(out, length, &image) == PLATINUM_IMAGE_OK, what);
}

static void test_vectors(void)
{
    size_t i;
    platinum_image image;

    for (i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
        long width;
        long height;
        int max_w;
        int max_h;
        int depth;
        char rgba_hex[600];
        char blob_hex[600];
        char what[64];
        long rgba_len;
        long want;
        long length = -1;
        int status;

        sscanf(kCases[i], "%ld %ld %d %d %d %599s %599s", &width, &height,
               &max_w, &max_h, &depth, rgba_hex, blob_hex);
        rgba_len = unhex(rgba_hex, pixels);
        want = unhex(blob_hex, expected);
        sprintf(what, "vector %d", (int)i);
        check(rgba_len == width * height * 4, what);
        status = platinum_image_encode(pixels, width, height, max_w, max_h, depth,
                                       out, (long)sizeof(out), &length);
        check(status == PLATINUM_ENCODE_OK && length == want &&
                  memcmp(out, expected, (size_t)want) == 0,
              what);
        check(platinum_image_parse(out, length, &image) == PLATINUM_IMAGE_OK, what);
    }
}

static void test_hand_derived(void)
{
    /*
     * Uneven ratio. Three grey pixels 0, 1, 2 shrink to two (max width 2). The
     * height is 1, so the scale is 2/3 and the destination is 2 x 1. Destination
     * 0 averages source [0,1) = 0. Destination 1 averages source [1,3) = (1+2+1)/2
     * = 2 (half up). The palette is 0 and 2, so indices are 0 and 1, packed
     * high nibble first in one byte with a zero pad byte to make rowBytes 2.
     *   header: PLIM, version 1, depth 4, 2 x 1, rowBytes 2, 2 colours, reserved 0
     *   palette: 000000 020202
     *   pixels: 0x01 (index 0, then index 1), 0x00 pad
     */
    expect_blob("uneven-ratio shrink of three pixels", (const unsigned char *)
                "\x00\x00\x00\xff\x01\x01\x01\xff\x02\x02\x02\xff",
                3, 1, 2, 320, 4,
                "504c494d" "01" "04" "0002" "0001" "0002" "0002" "0000"
                "000000" "020202" "0100");

    /*
     * Odd width at depth 4. Three pixels 10, 20, 20 with no shrink. The palette
     * is 10 and 20, so indices are 0, 1, 1. Row bytes are 2 (12 bits round up to
     * 2 bytes, already even). Byte 0 holds pixel 0 high and pixel 1 low: 0x01.
     * Byte 1 holds pixel 2 high and a zero low nibble pad: 0x10.
     */
    expect_blob("odd width packs the last nibble and pads with zero",
                (const unsigned char *)
                "\x0a\x0a\x0a\xff\x14\x14\x14\xff\x14\x14\x14\xff",
                3, 1, 320, 320, 4,
                "504c494d" "01" "04" "0003" "0001" "0002" "0002" "0000"
                "0a0a0a" "141414" "0110");
}

static void test_colour_cap(void)
{
    static unsigned char seventeen[17 * 4];
    static unsigned char sixteen[16 * 4];
    long length = -1;
    platinum_image image;
    int i;

    for (i = 0; i < 17; i++) {
        seventeen[i * 4] = (unsigned char)(i * 15);
        seventeen[i * 4 + 1] = 0;
        seventeen[i * 4 + 2] = 0;
        seventeen[i * 4 + 3] = 255;
    }
    check(platinum_image_encode(seventeen, 17, 1, 320, 320, 4, out,
                                (long)sizeof(out), &length) ==
              PLATINUM_ENCODE_TOO_MANY_COLOURS && length == 0,
          "17 colours at depth 4 is refused, not approximated");
    check(platinum_image_encode(seventeen, 17, 1, 320, 320, 8, out,
                                (long)sizeof(out), &length) == PLATINUM_ENCODE_OK,
          "17 colours at depth 8 fits");

    for (i = 0; i < 16; i++) {
        sixteen[i * 4] = (unsigned char)(i * 15);
        sixteen[i * 4 + 1] = 0;
        sixteen[i * 4 + 2] = 0;
        sixteen[i * 4 + 3] = 255;
    }
    length = -1;
    check(platinum_image_encode(sixteen, 16, 1, 320, 320, 4, out,
                                (long)sizeof(out), &length) == PLATINUM_ENCODE_OK &&
              length == 16 + 48 + 8 &&
              platinum_image_parse(out, length, &image) == PLATINUM_IMAGE_OK &&
              image.colours == 16,
          "16 colours at depth 4 fits exactly");
}

static void test_sizes(void)
{
    long length = -1;
    int x;
    int y;

    for (y = 0; y < 320; y++) {
        for (x = 0; x < 320; x++) {
            unsigned char *p = big + (y * 320 + x) * 4;

            p[0] = (unsigned char)((x & 3) * 85);
            p[1] = (unsigned char)((y & 3) * 85);
            p[2] = 0;
            p[3] = 255;
        }
    }
    check(platinum_image_encode(big, 320, 320, 320, 320, 4, out, (long)sizeof(out),
                                &length) == PLATINUM_ENCODE_OK &&
              length == 51264,
          "320 x 320 at depth 4 with 16 colours is exactly 51,264 bytes");

    for (y = 0; y < 320; y++) {
        for (x = 0; x < 320; x++) {
            unsigned char *p = big + (y * 320 + x) * 4;

            p[0] = (unsigned char)((x & 15) * 16);
            p[1] = (unsigned char)((y & 15) * 16);
            p[2] = 0;
            p[3] = 255;
        }
    }
    length = -1;
    check(platinum_image_encode(big, 320, 320, 320, 320, 8, out, (long)sizeof(out),
                                &length) == PLATINUM_ENCODE_TOO_LARGE && length == 0,
          "320 x 320 at depth 8 with 256 colours is over the limit");

}

static void test_buffer(void)
{
    long length = -1;
    long want;
    static const char *vector0 =
        "504c494d0104000200020002000400000000ff00ff00ff0000ffffff21000300";
    unsigned char rgba[16];
    int status;

    want = unhex(vector0, expected);
    memset(rgba, 0, sizeof(rgba));
    rgba[0] = 0xff; rgba[3] = 0xff;
    rgba[4] = 0x00; rgba[5] = 0xff; rgba[6] = 0x00; rgba[7] = 0xff;
    rgba[8] = 0x00; rgba[9] = 0x00; rgba[10] = 0xff; rgba[11] = 0xff;
    rgba[12] = 0xff; rgba[13] = 0xff; rgba[14] = 0xff; rgba[15] = 0xff;

    status = platinum_image_encode(rgba, 2, 2, 320, 320, 4, out, want - 1, &length);
    check(status == PLATINUM_ENCODE_BUFFER_SMALL && length == want,
          "a short buffer reports the length the blob needs");
    length = -1;
    status = platinum_image_encode(rgba, 2, 2, 320, 320, 4, out, want, &length);
    check(status == PLATINUM_ENCODE_OK && length == want &&
              memcmp(out, expected, (size_t)want) == 0,
          "a buffer of exactly the blob length is enough");
}

static void test_refusals(void)
{
    static unsigned char one[4];
    long length = 99;

    one[0] = 0; one[1] = 0; one[2] = 0; one[3] = 255;
    check(platinum_image_encode(NULL, 1, 1, 320, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG && length == 0,
          "NULL pixels");
    check(platinum_image_encode(one, 1, 1, 320, 320, 4, NULL, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "NULL output");
    check(platinum_image_encode(one, 1, 1, 320, 320, 4, out, 80000L, NULL) ==
              PLATINUM_ENCODE_BAD_ARG,
          "NULL length");
    check(platinum_image_encode(one, 0, 1, 320, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "zero width");
    check(platinum_image_encode(one, 1, 0, 320, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "zero height");
    check(platinum_image_encode(one, 3000, 4001, 320, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "more than 12 million source pixels, refused before any read");
    check(platinum_image_encode(one, 1, 1, 320, 320, 5, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "depth 5");
    check(platinum_image_encode(one, 1, 1, 320, 320, 1, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "depth 1");
    check(platinum_image_encode(one, 1, 1, 0, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "max width 0");
    check(platinum_image_encode(one, 1, 1, 321, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "max width 321");
    check(platinum_image_encode(one, 1, 1, 320, 0, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "max height 0");
    check(platinum_image_encode(one, 1, 1, 320, 321, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "max height 321");
    check(platinum_image_encode(one, 1, 1, 320, 320, 4, out, -1L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "negative buffer size");
}

int main(void)
{
    test_vectors();
    test_hand_derived();
    test_colour_cap();
    test_sizes();
    test_buffer();
    test_refusals();
    if (failures != 0) {
        printf("test_image_encode: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_image_encode: all %d checks passed\n", checks);
    return 0;
}
