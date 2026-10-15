/*
 * test_avatar_cache.c -- host-side tests for the avatar cache: the state of each
 * reference, what it does with an answer, and which slot gives way. The blob is
 * the 1x1 depth-8 vector from bridge/test/vectors/image/convert.json, so the
 * cache is tested against the same bytes as the parser. Not Classic Mac OS 9
 * validation; nothing is drawn here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avatar_cache.h"

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

/* The 1x1, depth-8 vector from bridge/test/vectors/image/convert.json. */
static const char *kGoodBlob = "504c494d010800010001000200010000ff7f7f0000";

static long unhex(const char *hex, unsigned char *out)
{
    long n = 0;

    while (hex[0] != '\0' && hex[1] != '\0') {
        unsigned int byte;

        sscanf(hex, "%2x", &byte);
        out[n++] = (unsigned char)byte;
        hex += 2;
    }
    return n;
}

/* A fresh malloc'd copy of the good blob, as the bridge client would hand over. */
static unsigned char *good_blob(long *length)
{
    unsigned char raw[64];
    unsigned char *copy;

    *length = unhex(kGoodBlob, raw);
    copy = malloc((size_t)*length);
    if (copy != NULL) {
        memcpy(copy, raw, (size_t)*length);
    }
    return copy;
}

static void ref_name(char *out, int n)
{
    sprintf(out, "avatar_thumbnail/plain/did:plc:%d/bafy%d@jpeg", n, n);
}

static void test_states(void)
{
    platinum_avatar_cache cache;
    unsigned char *blob;
    long length;

    platinum_avatar_cache_init(&cache);
    check(platinum_avatar_lookup(&cache, "avatar/x") == NULL, "an unknown ref is not ready");
    check(platinum_avatar_want(&cache, "avatar/x") == 1, "the first want fetches");
    check(platinum_avatar_want(&cache, "avatar/x") == 0, "a pending ref is not fetched twice");
    check(platinum_avatar_lookup(&cache, "avatar/x") == NULL, "a pending ref is not ready");

    blob = good_blob(&length);
    check(blob != NULL, "test blob allocated");
    check(platinum_avatar_fulfil(&cache, "avatar/x", blob, length) == PLATINUM_IMAGE_OK,
          "a good answer parses");
    {
        const platinum_image *image = platinum_avatar_lookup(&cache, "avatar/x");

        check(image != NULL, "a good answer makes the ref ready");
        check(image != NULL && image->width == 1 && image->height == 1, "the image is the one sent");
    }
    check(platinum_avatar_want(&cache, "avatar/x") == 0, "a ready ref is not fetched again");

    platinum_avatar_cache_dispose(&cache);
}

static void test_bad_answers(void)
{
    platinum_avatar_cache cache;
    unsigned char *junk;

    platinum_avatar_cache_init(&cache);

    /* A garbage answer fails the slot and frees the blob: no ready image, and no
     * second fetch for the same reference. */
    check(platinum_avatar_want(&cache, "avatar/bad") == 1, "a bad ref is wanted once");
    junk = malloc(4);
    memcpy(junk, "nope", 4);
    check(platinum_avatar_fulfil(&cache, "avatar/bad", junk, 4) != PLATINUM_IMAGE_OK,
          "garbage does not parse");
    check(platinum_avatar_lookup(&cache, "avatar/bad") == NULL, "a failed ref is not ready");
    check(platinum_avatar_want(&cache, "avatar/bad") == 0, "a failed ref is not refetched");

    /* A transport failure is the same: failed, not asked again. */
    check(platinum_avatar_want(&cache, "avatar/gone") == 1, "a gone ref is wanted once");
    platinum_avatar_fail(&cache, "avatar/gone");
    check(platinum_avatar_want(&cache, "avatar/gone") == 0, "a ref that failed to fetch is not refetched");

    /* An answer for a ref that was never asked for is refused, and its blob freed. */
    junk = malloc(4);
    memcpy(junk, "nope", 4);
    check(platinum_avatar_fulfil(&cache, "avatar/never", junk, 4) == -1,
          "an unasked answer is refused");

    platinum_avatar_cache_dispose(&cache);
}

static void test_refs_refused(void)
{
    platinum_avatar_cache cache;
    char long_ref[PLATINUM_MEDIA_REF_MAX + 8];

    platinum_avatar_cache_init(&cache);
    check(platinum_avatar_want(&cache, NULL) == 0, "no ref is refused");
    check(platinum_avatar_want(&cache, "") == 0, "an empty ref is refused");
    memset(long_ref, 'a', sizeof(long_ref) - 1);
    long_ref[sizeof(long_ref) - 1] = '\0';
    check(platinum_avatar_want(&cache, long_ref) == 0, "an over-long ref is refused");
    platinum_avatar_cache_dispose(&cache);
}

static void test_eviction(void)
{
    platinum_avatar_cache cache;
    char ref[PLATINUM_MEDIA_REF_MAX];
    int i;

    platinum_avatar_cache_init(&cache);

    /* Fill every slot with a ready avatar, in order. */
    for (i = 0; i < PLATINUM_AVATAR_SLOTS; i++) {
        long length;
        unsigned char *blob = good_blob(&length);

        ref_name(ref, i);
        platinum_avatar_want(&cache, ref);
        platinum_avatar_fulfil(&cache, ref, blob, length);
    }

    /* Using the first one makes the second the least recently used. */
    ref_name(ref, 0);
    check(platinum_avatar_lookup(&cache, ref) != NULL, "the first avatar is still there");

    /* A new reference takes the least recently used slot. */
    ref_name(ref, PLATINUM_AVATAR_SLOTS);
    check(platinum_avatar_want(&cache, ref) == 1, "a new ref gets a slot by eviction");
    ref_name(ref, 1);
    check(platinum_avatar_lookup(&cache, ref) == NULL, "the least recently used one went");
    ref_name(ref, 0);
    check(platinum_avatar_lookup(&cache, ref) != NULL, "a recently used one stayed");

    platinum_avatar_cache_dispose(&cache);
}

static void test_pending_never_evicted(void)
{
    platinum_avatar_cache cache;
    char ref[PLATINUM_MEDIA_REF_MAX];
    int i;

    platinum_avatar_cache_init(&cache);

    /* Every slot is fetching, so nothing can be given up for a new reference. */
    for (i = 0; i < PLATINUM_AVATAR_SLOTS; i++) {
        ref_name(ref, i);
        check(platinum_avatar_want(&cache, ref) == 1, "each pending ref gets a slot");
    }
    ref_name(ref, PLATINUM_AVATAR_SLOTS);
    check(platinum_avatar_want(&cache, ref) == 0, "with every slot fetching, nothing is evicted");

    /* An answer frees its slot for the next reference to use. */
    {
        long length;
        unsigned char *blob = good_blob(&length);

        ref_name(ref, 0);
        check(platinum_avatar_fulfil(&cache, ref, blob, length) == PLATINUM_IMAGE_OK,
              "one answer parses");
    }
    ref_name(ref, PLATINUM_AVATAR_SLOTS);
    check(platinum_avatar_want(&cache, ref) == 1, "a ready slot can now be given up");

    platinum_avatar_cache_dispose(&cache);
}

int main(void)
{
    test_states();
    test_bad_answers();
    test_refs_refused();
    test_eviction();
    test_pending_never_evicted();

    printf("avatar cache: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
