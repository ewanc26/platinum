#include "profile.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void
blank(platinum_profile *p)
{
    memset(p, 0, sizeof *p);
    p->following = -1;
    p->followed_by = -1;
    p->muted = -1;
    p->blocking = -1;
}

static void
test_actor(void)
{
    char out[300];

    /* DIDs pass unchanged. */
    CHECK(platinum_valid_actor("did:plc:abc123", out, sizeof out) == 0 && strcmp(out, "did:plc:abc123") == 0);
    CHECK(platinum_valid_actor("did:web:example.com", out, sizeof out) == 0);
    /* Handles pass, lowercased. */
    CHECK(platinum_valid_actor("Alice.Example.social", out, sizeof out) == 0 && strcmp(out, "alice.example.social") == 0);
    CHECK(platinum_valid_actor("a.b", out, sizeof out) == 0);
    /* Rejections. */
    CHECK(platinum_valid_actor("", out, sizeof out) == -1);
    CHECK(platinum_valid_actor("nodots", out, sizeof out) == -1);
    CHECK(platinum_valid_actor("-bad.example", out, sizeof out) == -1);
    CHECK(platinum_valid_actor("bad-.example", out, sizeof out) == -1);
    CHECK(platinum_valid_actor("ok.1abc", out, sizeof out) == -1);   /* TLD must start with a letter */
    CHECK(platinum_valid_actor("ok..example", out, sizeof out) == -1);
    CHECK(platinum_valid_actor("did:PLC:abc", out, sizeof out) == -1); /* method is lowercase only */
    CHECK(platinum_valid_actor("did:plc:", out, sizeof out) == -1);
    CHECK(platinum_valid_actor("did:plc:a b", out, sizeof out) == -1);
    CHECK(platinum_valid_actor("a.example?x=1", out, sizeof out) == -1);
    CHECK(platinum_valid_actor(NULL, out, sizeof out) == -1);
    /* Length cap: 253 bytes at most, and must fit the buffer. */
    CHECK(platinum_valid_actor("a.b", out, 3) == -1);
    CHECK(platinum_valid_actor("a.b", out, 4) == 0);
}

static void
test_minimal(void)
{
    platinum_profile p;
    char buf[256];
    int n;

    blank(&p);
    strcpy(p.did, "did:plc:abc");
    n = platinum_profile_json(&p, buf, sizeof buf);
    CHECK(n == (int)strlen(buf));
    CHECK(strcmp(buf, "{\"did\":\"did:plc:abc\"}") == 0);
}

static void
test_full(void)
{
    platinum_profile p;
    char buf[1024];

    blank(&p);
    strcpy(p.did, "did:plc:abc");
    p.has_handle = 1;
    strcpy(p.handle, "alice.test");
    p.has_display_name = 1;
    strcpy(p.display_name, "Alice");
    p.has_description = 1;
    strcpy(p.description, "Poet \"quoted\"\nline");
    p.has_followers_count = 1;
    p.followers_count = 120;
    p.has_follows_count = 1;
    p.follows_count = 80;
    p.has_posts_count = 1;
    p.posts_count = 456;
    p.following = 1;
    p.followed_by = 0;
    p.muted = 0;
    p.blocking = 0;
    platinum_profile_json(&p, buf, sizeof buf);
    CHECK(strcmp(buf,
        "{\"did\":\"did:plc:abc\",\"handle\":\"alice.test\",\"displayName\":\"Alice\","
        "\"description\":\"Poet \\\"quoted\\\"\\nline\",\"followersCount\":120,"
        "\"followsCount\":80,\"postsCount\":456,\"following\":true,\"followedBy\":false,"
        "\"muted\":false,\"blocking\":false}") == 0);
}

static void
test_own_profile_omits_viewer_fields(void)
{
    platinum_profile p;
    char buf[256];

    blank(&p);
    strcpy(p.did, "did:plc:me");
    p.has_handle = 1;
    strcpy(p.handle, "me.test");
    platinum_profile_json(&p, buf, sizeof buf);
    CHECK(strstr(buf, "following") == NULL);
    CHECK(strstr(buf, "muted") == NULL);
    CHECK(strcmp(buf, "{\"did\":\"did:plc:me\",\"handle\":\"me.test\"}") == 0);
}

static void
test_control_and_overflow(void)
{
    platinum_profile p;
    char buf[64];
    char tiny[8];

    blank(&p);
    strcpy(p.did, "did:plc:x");
    p.has_description = 1;
    p.description[0] = 'a';
    p.description[1] = '\t';
    p.description[2] = 'b';
    p.description[3] = '\0';
    platinum_profile_json(&p, buf, sizeof buf);
    CHECK(strstr(buf, "\"description\":\"a\\tb\"") != NULL);
    /* Other control characters use the \u00XX form. */
    p.description[1] = '\x01';
    platinum_profile_json(&p, buf, sizeof buf);
    CHECK(strstr(buf, "\"a\\u0001b\"") != NULL);

    /* Too small: -1, and the buffer is left empty. */
    CHECK(platinum_profile_json(&p, tiny, sizeof tiny) == -1);
    CHECK(tiny[0] == '\0');
    CHECK(platinum_profile_json(NULL, buf, sizeof buf) == -1);
}

int
main(void)
{
    test_actor();
    test_minimal();
    test_full();
    test_own_profile_omits_viewer_fields();
    test_control_and_overflow();
    if (failures) {
        fprintf(stderr, "test_profile: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_profile: %d checks passed\n", checks);
    return 0;
}
