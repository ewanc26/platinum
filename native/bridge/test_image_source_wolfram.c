/* Decodes a real PNG with Wolfram's decoder and checks the pixels it gives the route. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_source_wolfram.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static unsigned char *
read_file(const char *path, long *len)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    *len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t) *len);
    if (buf != NULL && fread(buf, 1, (size_t) *len, f) != (size_t) *len) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

int
main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "fixtures/rgba-2x2.png";
    long len = 0;
    unsigned char *bytes = read_file(path, &len);
    platinum_rgba img;
    platinum_image_result r;

    CHECK(bytes != NULL);
    if (bytes == NULL) {
        fprintf(stderr, "cannot read %s\n", path);
        return 1;
    }
    r = platinum_image_from_bytes(bytes, (size_t) len, &img);
    CHECK(r == PLATINUM_IMAGE_OK);
    CHECK(img.width == 2 && img.height == 2);
    if (r == PLATINUM_IMAGE_OK) {
        /* Straight alpha, as the file stores it: opaque red, opaque green, semi-transparent blue. */
        CHECK(img.pixels[0] == 255 && img.pixels[1] == 0 && img.pixels[2] == 0 && img.pixels[3] == 255);
        CHECK(img.pixels[4] == 0 && img.pixels[5] == 255 && img.pixels[6] == 0 && img.pixels[7] == 255);
        CHECK(img.pixels[8] == 0 && img.pixels[9] == 0 && img.pixels[10] == 255 && img.pixels[11] == 128);
        free(img.pixels);
    }

    /* A source one pixel over Wolfram's 2048-pixel limit is too large, not a decode error. */
    {
        long wlen = 0;
        unsigned char *wide = read_file("fixtures/rgba-2049x1.png", &wlen);
        platinum_rgba big;
        CHECK(wide != NULL);
        if (wide != NULL) {
            CHECK(platinum_image_from_bytes(wide, (size_t) wlen, &big) == PLATINUM_IMAGE_TOO_LARGE);
            CHECK(big.pixels == NULL);
            free(wide);
        }
    }

    /* Bytes that are not an image are an error, not an empty image. */
    {
        static const char junk[] = "not an image at all";
        platinum_rgba bad;
        CHECK(platinum_image_from_bytes(junk, sizeof junk, &bad) == PLATINUM_IMAGE_ERROR);
        CHECK(bad.pixels == NULL);
    }
    free(bytes);

    if (failures) {
        fprintf(stderr, "test_image_source_wolfram: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_image_source_wolfram: %d checks passed\n", checks);
    return 0;
}
