#include "validate.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void
test_reply(void)
{
    char long_text[1200];
    char emoji[4100];
    size_t i;
    char *p;

    CHECK(platinum_valid_reply_text("hi") == 0);
    CHECK(platinum_valid_reply_text("  \t\n") == -1);
    CHECK(platinum_valid_reply_text("") == -1);
    CHECK(platinum_valid_reply_text(NULL) == -1);

    /* 1000 code points is accepted, 1001 is not; ASCII is one byte each. */
    memset(long_text, 'a', 1000);
    long_text[1000] = '\0';
    CHECK(platinum_valid_reply_text(long_text) == 0);
    memset(long_text, 'a', 1001);
    long_text[1001] = '\0';
    CHECK(platinum_valid_reply_text(long_text) == -1);

    /* Four-byte code points count once each, not four times. */
    p = emoji;
    for (i = 0; i < 1000; i++) {
        *p++ = (char)0xF0; *p++ = (char)0x9F; *p++ = (char)0x98; *p++ = (char)0x80;
    }
    *p = '\0';
    CHECK(platinum_utf8_length(emoji) == 1000);
    CHECK(platinum_valid_reply_text(emoji) == 0);
}

static void
test_post_ref(void)
{
    CHECK(platinum_valid_post_ref("at://did:plc:abc123/app.bsky.feed.post/3kxyz", "bafyreiabc") == 0);
    CHECK(platinum_valid_post_ref("at://did:web:example.com/app.bsky.feed.post/3k", "cid1") == 0);
    CHECK(platinum_valid_post_ref("https://did:plc:abc/app.bsky.feed.post/3k", "cid1") == -1);
    CHECK(platinum_valid_post_ref("at://did:plc:abc/app.bsky.feed.like/3k", "cid1") == -1);
    CHECK(platinum_valid_post_ref("at://did:plc:abc/app.bsky.feed.post/", "cid1") == -1);
    CHECK(platinum_valid_post_ref("at://did:plc:abc/app.bsky.feed.post/3k?x", "cid1") == -1);
    CHECK(platinum_valid_post_ref("at://did:plc:abc/app.bsky.feed.post/3k", "") == -1);
    CHECK(platinum_valid_post_ref("at://did:plc:abc/app.bsky.feed.post/3k", "cid-1") == -1);
    CHECK(platinum_valid_post_ref(NULL, "cid1") == -1);
}

static void
test_subject_did(void)
{
    CHECK(platinum_valid_subject_did("did:plc:abc") == 0);
    CHECK(platinum_valid_subject_did("alice.test") == -1);   /* a handle is not a DID here */
    CHECK(platinum_valid_subject_did("did:PLC:abc") == -1);
    CHECK(platinum_valid_subject_did("did:plc:") == -1);
    CHECK(platinum_valid_subject_did(NULL) == -1);
}

static void
test_query(void)
{
    char out[256];

    CHECK(platinum_valid_query("  welsh borders  ", out, sizeof out) == 0);
    CHECK(strcmp(out, "welsh borders") == 0);
    CHECK(platinum_valid_query("   ", out, sizeof out) == -1);
    CHECK(platinum_valid_query("a\tb", out, sizeof out) == -1);     /* a tab inside is a control character */
    CHECK(platinum_valid_query("a\x01" "b", out, sizeof out) == -1); /* control character */
    CHECK(platinum_valid_query("a\x7f" "b", out, sizeof out) == -1);
    CHECK(platinum_valid_query("abc", out, 3) == -1);                /* does not fit */
    CHECK(platinum_valid_query("abc", out, 4) == 0);
}

int
main(void)
{
    test_reply();
    test_post_ref();
    test_subject_did();
    test_query();
    if (failures) {
        fprintf(stderr, "test_validate: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_validate: %d checks passed\n", checks);
    return 0;
}
