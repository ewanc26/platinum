#include "image_route.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../plim/plim.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *good_ref = "avatar/plain/did:plc:abc123/bafyreiabc@jpeg";

typedef struct fake {
    int calls;
    int mode;          /* 0 ok, 1 too large, 2 error */
} fake;

/* A 2x2 source: opaque red, opaque green, half-transparent blue, transparent. */
static platinum_image_result
fake_source(void *ctx, const char *ref, platinum_rgba *out)
{
    fake *f = (fake *) ctx;
    static const unsigned char px[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 128,   0, 0, 0, 0
    };
    (void) ref;
    f->calls++;
    if (f->mode == 1) return PLATINUM_IMAGE_TOO_LARGE;
    if (f->mode == 2) return PLATINUM_IMAGE_ERROR;
    out->pixels = malloc(sizeof px);
    memcpy(out->pixels, px, sizeof px);
    out->width = 2;
    out->height = 2;
    return PLATINUM_IMAGE_OK;
}

static void
test_refs(void)
{
    CHECK(platinum_valid_image_ref(good_ref) == 0);
    CHECK(platinum_valid_image_ref("feed_fullsize/plain/did:web:example.com/bafy1@png") == 0);
    CHECK(platinum_valid_image_ref("avatar_thumbnail/plain/did:plc:abc/bafy@jpeg") == 0);
    CHECK(platinum_valid_image_ref("banner/plain/did:plc:abc/bafy@jpeg") == -1);      /* not a kind */
    CHECK(platinum_valid_image_ref("avatar/plain/did:plc:abc/bafy@gif") == -1);       /* not jpeg or png */
    CHECK(platinum_valid_image_ref("avatar/plain/did:plc:abc/bafy@jpeg/x") == -1);
    CHECK(platinum_valid_image_ref("avatar/did:plc:abc/bafy@jpeg") == -1);           /* no plain/ */
    CHECK(platinum_valid_image_ref("avatar/plain/bob.test/bafy@jpeg") == -1);        /* not a DID */
    CHECK(platinum_valid_image_ref("avatar/plain/did:plc:abc/@jpeg") == -1);         /* no cid */
    CHECK(platinum_valid_image_ref(NULL) == -1);
}

static void
test_params(void)
{
    int w = 0;
    int c = 0;
    CHECK(platinum_parse_image_params(NULL, NULL, &w, &c) == 0 && w == 320 && c == 16);
    CHECK(platinum_parse_image_params("16", "8", &w, &c) == 0 && w == 16 && c == 256);
    CHECK(platinum_parse_image_params("320", "4", &w, &c) == 0 && w == 320 && c == 16);
    CHECK(platinum_parse_image_params("15", NULL, &w, &c) == -1);
    CHECK(platinum_parse_image_params("321", NULL, &w, &c) == -1);
    CHECK(platinum_parse_image_params("1000", NULL, &w, &c) == -1);
    CHECK(platinum_parse_image_params("", NULL, &w, &c) == -1);
    CHECK(platinum_parse_image_params("12a", NULL, &w, &c) == -1);
    CHECK(platinum_parse_image_params(NULL, "5", &w, &c) == -1);
}

static void
test_route(void)
{
    fake f;
    unsigned char *blob = NULL;
    long len = 0;
    unsigned char *direct = NULL;
    long direct_len = 0;
    unsigned char px[16];
    int st;
    static const unsigned char src[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 128,   0, 0, 0, 0
    };

    memset(&f, 0, sizeof f);
    st = platinum_route_image(&f, fake_source, good_ref, "16", "4", &blob, &len);
    CHECK(st == 200);
    CHECK(f.calls == 1);
    CHECK(blob != NULL && len > 0);
    /* The route's blob is the encoder's blob for the same source. */
    memcpy(px, src, sizeof px);
    CHECK(plim_encode(px, 2, 2, 16, 320, 16, &direct, &direct_len) == PLIM_OK);
    CHECK(direct_len == len && blob != NULL && direct != NULL && memcmp(blob, direct, (size_t) len) == 0);
    free(blob);
    free(direct);

    /* A bad ref or parameter is refused before any fetch. */
    memset(&f, 0, sizeof f);
    st = platinum_route_image(&f, fake_source, "banner/x", NULL, NULL, &blob, &len);
    CHECK(st == 400 && f.calls == 0 && blob == NULL);
    st = platinum_route_image(&f, fake_source, good_ref, "999", NULL, &blob, &len);
    CHECK(st == 400 && f.calls == 0);

    /* A source over the decoder's limit: 413. */
    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_image(&f, fake_source, good_ref, NULL, NULL, &blob, &len);
    CHECK(st == 413 && blob == NULL);

    /* A source that cannot be fetched or decoded: 502. */
    memset(&f, 0, sizeof f);
    f.mode = 2;
    st = platinum_route_image(&f, fake_source, good_ref, NULL, NULL, &blob, &len);
    CHECK(st == 502 && blob == NULL);
}

int
main(void)
{
    test_refs();
    test_params();
    test_route();
    if (failures) {
        fprintf(stderr, "test_image_route: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_image_route: %d checks passed\n", checks);
    return 0;
}
