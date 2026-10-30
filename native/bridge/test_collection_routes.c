#include "collection_routes.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

typedef struct fake {
    int calls;
    int mode;        /* 0 ok, 1 not found, 2 error */
    int kind;
    char key[256];
    int count;
} fake;

static platinum_collection_result
fake_fn(void *ctx, int kind, const char *key, int count, const char *cursor, char *body, size_t cap)
{
    fake *f = (fake *) ctx;
    (void) cursor;
    f->calls++;
    f->kind = kind;
    f->count = count;
    strcpy(f->key, key);
    if (f->mode == 1) return PLATINUM_COLLECTION_NOT_FOUND;
    if (f->mode == 2) return PLATINUM_COLLECTION_ERROR;
    if (cap > 8) strcpy(body, "{}");
    return PLATINUM_COLLECTION_OK;
}

static void
test_collection_uri(void)
{
    CHECK(platinum_valid_collection_uri("at://did:plc:abc/app.bsky.feed.generator/cats", "app.bsky.feed.generator") == 0);
    CHECK(platinum_valid_collection_uri("at://did:web:example.com/app.bsky.graph.list/friends.v2", "app.bsky.graph.list") == 0);
    /* The wrong collection, a missing rkey, and bad characters are refused. */
    CHECK(platinum_valid_collection_uri("at://did:plc:abc/app.bsky.graph.list/x", "app.bsky.feed.generator") == -1);
    CHECK(platinum_valid_collection_uri("at://did:plc:abc/app.bsky.feed.generator/", "app.bsky.feed.generator") == -1);
    CHECK(platinum_valid_collection_uri("at://did:plc:abc/app.bsky.feed.generator/a b", "app.bsky.feed.generator") == -1);
    CHECK(platinum_valid_collection_uri("https://did:plc:abc/app.bsky.feed.generator/x", "app.bsky.feed.generator") == -1);
    CHECK(platinum_valid_collection_uri("at://alice.test/app.bsky.feed.generator/x", "app.bsky.feed.generator") == -1);
    CHECK(platinum_valid_collection_uri(NULL, "app.bsky.feed.generator") == -1);
}

static void
test_feed_and_list(void)
{
    fake f;
    char body[256];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_collection(&f, fake_fn, 1, "at://did:plc:abc/app.bsky.feed.generator/cats", "7", NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(f.kind == 0 && f.count == 7);
    CHECK(strcmp(f.key, "at://did:plc:abc/app.bsky.feed.generator/cats") == 0);

    memset(&f, 0, sizeof f);
    st = platinum_route_collection(&f, fake_fn, 0, "at://did:plc:abc/app.bsky.graph.list/friends", NULL, NULL, body, sizeof body);
    CHECK(st == 200 && f.kind == 1 && f.count == 20);

    memset(&f, 0, sizeof f);
    st = platinum_route_collection(&f, fake_fn, 1, "not a uri", NULL, NULL, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_uri\"") != NULL);

    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_collection(&f, fake_fn, 1, "at://did:plc:abc/app.bsky.feed.generator/cats", NULL, NULL, body, sizeof body);
    CHECK(st == 404);
    CHECK(strcmp(body, "{\"error\":\"not_found\",\"message\":\"No such feed or list.\"}") == 0);
}

static void
test_actor_graph(void)
{
    fake f;
    char body[256];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_actor_graph(&f, fake_fn, 2, "Alice.Test", "3", NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(f.kind == 2 && f.count == 3);
    CHECK(strcmp(f.key, "alice.test") == 0);

    memset(&f, 0, sizeof f);
    st = platinum_route_actor_graph(&f, fake_fn, 0, "no spaces here", NULL, NULL, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_actor\"") != NULL);
    st = platinum_route_actor_graph(&f, fake_fn, 0, NULL, NULL, NULL, body, sizeof body);
    CHECK(st == 400);

    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_actor_graph(&f, fake_fn, 1, "did:plc:abc", NULL, NULL, body, sizeof body);
    CHECK(st == 404);
    CHECK(strstr(body, "\"error\":\"actor_not_found\"") != NULL);
}

static void
test_engagement(void)
{
    fake f;
    char body[256];
    int st;
    const char *post = "at://did:plc:abc/app.bsky.feed.post/3k";

    memset(&f, 0, sizeof f);
    st = platinum_route_engagement(&f, fake_fn, 1, post, "5", NULL, body, sizeof body);
    CHECK(st == 200 && f.kind == 1 && f.count == 5);

    memset(&f, 0, sizeof f);
    st = platinum_route_engagement(&f, fake_fn, 0, "at://did:plc:abc/app.bsky.feed.like/3k", NULL, NULL, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_post_ref\"") != NULL);

    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_engagement(&f, fake_fn, 0, post, NULL, NULL, body, sizeof body);
    CHECK(st == 404);
    CHECK(strstr(body, "\"error\":\"post_not_found\"") != NULL);

    memset(&f, 0, sizeof f);
    f.mode = 2;
    st = platinum_route_engagement(&f, fake_fn, 0, post, NULL, NULL, body, sizeof body);
    CHECK(st == 500);
}

static void
test_thread(void)
{
    fake f;
    char body[256];
    int st;
    const char *post = "at://did:plc:abc/app.bsky.feed.post/3k";

    memset(&f, 0, sizeof f);
    st = platinum_route_thread(&f, fake_fn, post, body, sizeof body);
    CHECK(st == 200 && f.kind == 2 && f.count == 20);
    CHECK(strcmp(f.key, post) == 0);

    memset(&f, 0, sizeof f);
    st = platinum_route_thread(&f, fake_fn, "https://example.com/p", body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_post_ref\"") != NULL);

    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_thread(&f, fake_fn, post, body, sizeof body);
    CHECK(st == 404);
    CHECK(strstr(body, "\"error\":\"post_not_found\"") != NULL);
}

int
main(void)
{
    test_thread();
    test_collection_uri();
    test_feed_and_list();
    test_actor_graph();
    test_engagement();
    if (failures) {
        fprintf(stderr, "test_collection_routes: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_collection_routes: %d checks passed\n", checks);
    return 0;
}
