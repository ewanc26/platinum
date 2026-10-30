#include "profile_wolfram.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int
main(void)
{
    char did[] = "did:plc:abc";
    char handle[] = "alice.test";
    char name[] = "Alice";
    char desc[] = "Poet";
    char follow_uri[] = "at://did:plc:me/app.bsky.graph.follow/3k";
    char block_uri[] = "at://did:plc:me/app.bsky.graph.block/3k";
    wf_agent_profile src;
    platinum_profile out;
    char json[1024];
    char clipped[64];
    char big[4096];
    size_t i;

    memset(&src, 0, sizeof src);
    src.did = did;
    src.handle = handle;
    src.display_name = name;
    src.description = desc;
    src.followers_count = 120;
    src.follows_count = 80;
    src.posts_count = 456;
    src.following = follow_uri;
    src.muted = true;
    src.blocking = block_uri;

    /* Another account: viewer fields present; followedBy is not in Wolfram's struct. */
    CHECK(platinum_profile_from_wolfram(&src, 0, &out) == 0);
    CHECK(out.following == 1 && out.muted == 1 && out.blocking == 1);
    CHECK(out.followed_by == -1);
    platinum_profile_json(&out, json, sizeof json);
    CHECK(strstr(json, "\"followedBy\"") == NULL);
    CHECK(strstr(json, "\"following\":true") != NULL);
    CHECK(strcmp(json,
        "{\"did\":\"did:plc:abc\",\"handle\":\"alice.test\",\"displayName\":\"Alice\","
        "\"description\":\"Poet\",\"followersCount\":120,\"followsCount\":80,"
        "\"postsCount\":456,\"following\":true,\"muted\":true,\"blocking\":true}") == 0);

    /* Own profile: viewer fields omitted entirely. */
    CHECK(platinum_profile_from_wolfram(&src, 1, &out) == 0);
    platinum_profile_json(&out, json, sizeof json);
    CHECK(strstr(json, "following") == NULL);
    CHECK(strstr(json, "muted") == NULL);
    CHECK(strstr(json, "blocking") == NULL);

    /* Absent viewer URIs mean false, not omitted, for another account. */
    src.following = NULL;
    src.blocking = NULL;
    src.muted = false;
    CHECK(platinum_profile_from_wolfram(&src, 0, &out) == 0);
    CHECK(out.following == 0 && out.blocking == 0 && out.muted == 0);

    /* Absent handle and display name are omitted, not empty strings. */
    src.handle = NULL;
    src.display_name = NULL;
    CHECK(platinum_profile_from_wolfram(&src, 0, &out) == 0);
    platinum_profile_json(&out, json, sizeof json);
    CHECK(strstr(json, "handle") == NULL);
    CHECK(strstr(json, "displayName") == NULL);

    /* Missing did is refused. */
    src.did = NULL;
    CHECK(platinum_profile_from_wolfram(&src, 0, &out) == -1);

    /* Clip: 600 four-byte code points become exactly 511, never a split sequence. */
    {
        char *p = big;
        for (i = 0; i < 600; i++) {
            /* U+1F600, UTF-8 F0 9F 98 80 */
            *p++ = (char)0xF0; *p++ = (char)0x9F; *p++ = (char)0x98; *p++ = (char)0x80;
        }
        *p = '\0';
        CHECK(platinum_utf8_clip_codepoints(big, 511, clipped, sizeof clipped) == -1);
        {
            static char wide[2048 * 2];
            long n = platinum_utf8_clip_codepoints(big, 511, wide, sizeof wide);
            CHECK(n == 511 * 4);
            CHECK(strlen(wide) == (size_t)(511 * 4));
        }
    }
    /* Short text is copied whole. */
    CHECK(platinum_utf8_clip_codepoints("abc", 511, clipped, sizeof clipped) == 3);
    /* A multi-byte character is not cut in half at the byte limit. */
    CHECK(platinum_utf8_clip_codepoints("\xc3\xa9\xc3\xa9", 1, clipped, sizeof clipped) == 2);
    CHECK(strcmp(clipped, "\xc3\xa9") == 0);

    if (failures) {
        fprintf(stderr, "test_profile_wolfram: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_profile_wolfram: %d checks passed\n", checks);
    return 0;
}
