/* The thread route: the flat, bounded shape the Mac's Thread window parses, and
 * its status decisions, with no network. */
#include <stdio.h>
#include <string.h>

#include "thread_route.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static platinum_thread_result
thread_ok(void *ctx, const char *uri, platinum_thread_entry *entries, size_t max,
          size_t *count, int *truncated)
{
    (void) ctx;
    (void) uri;
    if (max < 3) return PLATINUM_THREAD_ERROR;
    memset(entries, 0, max * sizeof *entries);
    {
        int i;
        static const char *const uris[3] = {
            "at://did:plc:a/app.bsky.feed.post/3kparent",
            "at://did:plc:a/app.bsky.feed.post/3kroot",
            "at://did:plc:b/app.bsky.feed.post/3kreply"
        };
        static const char *const texts[3] = { "parent post", "root post", "a reply" };
        static const int depths[3] = { -1, 0, 1 };

        for (i = 0; i < 3; i++) {
            strcpy(entries[i].post.uri, uris[i]);
            strcpy(entries[i].post.text, texts[i]);
            strcpy(entries[i].post.author.did, "did:plc:a");
            strcpy(entries[i].post.created_at, "2026-10-01T10:00:00.000Z");
            entries[i].depth = depths[i];
        }
    }
    *count = 3;
    *truncated = 0;
    return PLATINUM_THREAD_OK;
}

static platinum_thread_result
thread_truncated(void *ctx, const char *uri, platinum_thread_entry *entries, size_t max,
                 size_t *count, int *truncated)
{
    platinum_thread_result r = thread_ok(ctx, uri, entries, max, count, truncated);
    *truncated = 1;
    return r;
}

static platinum_thread_result
thread_missing(void *ctx, const char *uri, platinum_thread_entry *entries, size_t max,
               size_t *count, int *truncated)
{
    (void) ctx;
    (void) uri;
    (void) entries;
    (void) max;
    *count = 0;
    *truncated = 0;
    return PLATINUM_THREAD_NOT_FOUND;
}

static platinum_thread_result
thread_broken(void *ctx, const char *uri, platinum_thread_entry *entries, size_t max,
              size_t *count, int *truncated)
{
    (void) ctx;
    (void) uri;
    (void) entries;
    (void) max;
    *count = 0;
    *truncated = 0;
    return PLATINUM_THREAD_ERROR;
}

int
main(void)
{
    static const char uri[] = "at://did:plc:a/app.bsky.feed.post/3kroot";
    char body[16384];
    int status;

    status = platinum_route_thread_view(NULL, thread_ok, uri, body, sizeof body);
    CHECK(status == 200);
    CHECK(strstr(body, "\"text\":\"parent post\"") != NULL);
    CHECK(strstr(body, "\"depth\":-1") != NULL);
    CHECK(strstr(body, "\"depth\":0") != NULL);
    CHECK(strstr(body, "\"depth\":1") != NULL);
    CHECK(strstr(body, "\"truncated\":false") != NULL);
    /* Ancestors first, then the post, then replies. */
    CHECK(strstr(body, "parent post") < strstr(body, "root post"));
    CHECK(strstr(body, "root post") < strstr(body, "a reply"));

    status = platinum_route_thread_view(NULL, thread_truncated, uri, body, sizeof body);
    CHECK(status == 200);
    CHECK(strstr(body, "\"truncated\":true") != NULL);

    status = platinum_route_thread_view(NULL, thread_missing, uri, body, sizeof body);
    CHECK(status == 404);
    CHECK(strstr(body, "post_not_found") != NULL);

    status = platinum_route_thread_view(NULL, thread_broken, uri, body, sizeof body);
    CHECK(status == 500);
    CHECK(strstr(body, "bridge_error") != NULL);

    status = platinum_route_thread_view(NULL, thread_ok, NULL, body, sizeof body);
    CHECK(status == 400);
    CHECK(strstr(body, "invalid_post_ref") != NULL);

    status = platinum_route_thread_view(NULL, thread_ok, "at://did:plc:a/app.bsky.feed.generator/x",
                                        body, sizeof body);
    CHECK(status == 400);

    if (failures) {
        fprintf(stderr, "test_thread_route: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_thread_route: %d checks passed\n", checks);
    return 0;
}
