#include "write_routes.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *good_uri = "at://did:plc:abc/app.bsky.feed.post/3kxyz";

typedef struct fake {
    int calls;
    int mode;       /* 0 ok, 1 error, 2 not found */
    int post_gone;  /* toggle: post no longer exists, so on comes back false */
    int last_kind;
    int last_on;
} fake;

static platinum_write_result
fake_toggle(void *ctx, int kind, const char *uri, const char *cid, int on, int *out_on, long *out_count)
{
    fake *f = (fake *) ctx;
    (void) uri;
    (void) cid;
    f->calls++;
    f->last_kind = kind;
    f->last_on = on;
    if (f->mode == 1) return PLATINUM_WRITE_ERROR;
    *out_on = f->post_gone ? 0 : on;
    *out_count = 7;
    return PLATINUM_WRITE_OK;
}

static platinum_write_result
fake_subject(void *ctx, int kind, const char *did, int on)
{
    fake *f = (fake *) ctx;
    (void) kind;
    (void) did;
    (void) on;
    f->calls++;
    if (f->mode == 1) return PLATINUM_WRITE_ERROR;
    if (f->mode == 2) return PLATINUM_WRITE_NOT_FOUND;
    return PLATINUM_WRITE_OK;
}

static void
test_toggle(void)
{
    fake f;
    char body[256];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_toggle(&f, fake_toggle, 0, good_uri, "bafyabc", 1, 1, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"at://did:plc:abc/app.bsky.feed.post/3kxyz\",\"on\":true,\"count\":7}") == 0);
    CHECK(f.last_kind == 0 && f.last_on == 1);

    /* Repost kind is passed through. */
    memset(&f, 0, sizeof f);
    st = platinum_route_toggle(&f, fake_toggle, 1, good_uri, "bafyabc", 1, 0, body, sizeof body);
    CHECK(st == 200);
    CHECK(f.last_kind == 1 && f.last_on == 0);
    CHECK(strstr(body, "\"on\":false") != NULL);

    /* Bad input is refused before any call. */
    memset(&f, 0, sizeof f);
    st = platinum_route_toggle(&f, fake_toggle, 0, good_uri, "bafyabc", 0, 1, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_post_ref\"") != NULL);
    st = platinum_route_toggle(&f, fake_toggle, 0, "https://nope", "bafyabc", 1, 1, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);

    /* Liking a post that is gone: 404, as the TypeScript does. */
    memset(&f, 0, sizeof f);
    f.post_gone = 1;
    st = platinum_route_toggle(&f, fake_toggle, 0, good_uri, "bafyabc", 1, 1, body, sizeof body);
    CHECK(st == 404);
    CHECK(strcmp(body, "{\"error\":\"post_not_found\",\"message\":\"The post no longer exists.\"}") == 0);

    /* Unliking a gone post is not an error: the result is simply off. */
    memset(&f, 0, sizeof f);
    f.post_gone = 1;
    st = platinum_route_toggle(&f, fake_toggle, 0, good_uri, "bafyabc", 1, 0, body, sizeof body);
    CHECK(st == 200);

    /* Operation failure: 500. */
    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_toggle(&f, fake_toggle, 0, good_uri, "bafyabc", 1, 1, body, sizeof body);
    CHECK(st == 500);
}

static void
test_subject(void)
{
    fake f;
    char body[256];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_subject(&f, fake_subject, 0, "did:plc:bob", 1, 1, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"did\":\"did:plc:bob\",\"on\":true}") == 0);

    memset(&f, 0, sizeof f);
    st = platinum_route_subject(&f, fake_subject, 1, "did:plc:bob", 1, 0, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"did\":\"did:plc:bob\",\"on\":false}") == 0);

    /* A handle is not accepted here: the write routes take a DID. */
    memset(&f, 0, sizeof f);
    st = platinum_route_subject(&f, fake_subject, 0, "bob.test", 1, 1, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_actor\"") != NULL);
    st = platinum_route_subject(&f, fake_subject, 0, NULL, 1, 1, body, sizeof body);
    CHECK(st == 400);
    st = platinum_route_subject(&f, fake_subject, 0, "did:plc:bob", 0, 1, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);

    /* Unknown account: 404 actor_not_found. */
    memset(&f, 0, sizeof f);
    f.mode = 2;
    st = platinum_route_subject(&f, fake_subject, 2, "did:plc:bob", 1, 1, body, sizeof body);
    CHECK(st == 404);
    CHECK(strcmp(body, "{\"error\":\"actor_not_found\",\"message\":\"No such account.\"}") == 0);

    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_subject(&f, fake_subject, 0, "did:plc:bob", 1, 1, body, sizeof body);
    CHECK(st == 500);

    /* A body too small for the answer becomes the generic 500. */
    memset(&f, 0, sizeof f);
    st = platinum_route_subject(&f, fake_subject, 0, "did:plc:bob", 1, 1, body, 20);
    CHECK(st == 500);
}

int
main(void)
{
    test_toggle();
    test_subject();
    if (failures) {
        fprintf(stderr, "test_write_routes: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_write_routes: %d checks passed\n", checks);
    return 0;
}
