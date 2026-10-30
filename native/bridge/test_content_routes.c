#include "content_routes.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *good = "at://did:plc:me/app.bsky.feed.post/3kdel";

typedef struct fake {
    int calls;
    int mode;          /* 0 ok, 1 error, 2 not yours */
    size_t have;       /* list: how many entries the fake offers */
} fake;

static platinum_content_result
fake_list(void *ctx, platinum_muted_view *views, size_t max, size_t *count)
{
    fake *f = (fake *) ctx;
    size_t i;
    f->calls++;
    if (f->mode == 1) return PLATINUM_CONTENT_ERROR;
    *count = 0;
    for (i = 0; i < f->have && i < max; i++) {
        memset(&views[i], 0, sizeof views[i]);
        strcpy(views[i].value, i == 1 ? "say \"hi\"" : "spoilers");
        views[i].content = 1;
        views[i].tag = i == 0;
        (*count)++;
    }
    return PLATINUM_CONTENT_OK;
}

static platinum_content_result
fake_delete(void *ctx, const char *uri)
{
    fake *f = (fake *) ctx;
    (void) uri;
    f->calls++;
    if (f->mode == 1) return PLATINUM_CONTENT_ERROR;
    if (f->mode == 2) return PLATINUM_CONTENT_NOT_YOURS;
    return PLATINUM_CONTENT_OK;
}

static void
test_list(void)
{
    fake f;
    char body[512];
    int st;

    memset(&f, 0, sizeof f);
    f.have = 2;
    st = platinum_route_muted_list(&f, fake_list, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body,
        "{\"words\":[{\"value\":\"spoilers\",\"targets\":[\"content\",\"tag\"]},"
        "{\"value\":\"say \\\"hi\\\"\",\"targets\":[\"content\"]}]}") == 0);

    /* Empty list is still a 200 with an empty array. */
    memset(&f, 0, sizeof f);
    st = platinum_route_muted_list(&f, fake_list, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"words\":[]}") == 0);

    /* More than 100 stored words: only 100 are returned. */
    {
        static char big[65536];
        memset(&f, 0, sizeof f);
        f.have = 150;
        st = platinum_route_muted_list(&f, fake_list, big, sizeof big);
        CHECK(st == 200);
        {
            int n = 0;
            const char *p = big;
            while ((p = strstr(p, "\"value\"")) != NULL) { n++; p++; }
            CHECK(n == 100);
        }
    }

    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_muted_list(&f, fake_list, body, sizeof body);
    CHECK(st == 500);

    /* A body that does not fit is the generic 500, not a cut-off list. */
    memset(&f, 0, sizeof f);
    f.have = 2;
    st = platinum_route_muted_list(&f, fake_list, body, 30);
    CHECK(st == 500);
}

static void
test_delete(void)
{
    fake f;
    char body[256];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_post_delete(&f, fake_delete, good, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"at://did:plc:me/app.bsky.feed.post/3kdel\",\"deleted\":true}") == 0);

    memset(&f, 0, sizeof f);
    st = platinum_route_post_delete(&f, fake_delete, "https://example.com/post", body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_post_ref\"") != NULL);
    st = platinum_route_post_delete(&f, fake_delete, NULL, body, sizeof body);
    CHECK(st == 400);

    memset(&f, 0, sizeof f);
    f.mode = 2;
    st = platinum_route_post_delete(&f, fake_delete, good, body, sizeof body);
    CHECK(st == 403);
    CHECK(strcmp(body, "{\"error\":\"not_your_post\",\"message\":\"You can only delete your own posts.\"}") == 0);

    memset(&f, 0, sizeof f);
    f.mode = 1;
    st = platinum_route_post_delete(&f, fake_delete, good, body, sizeof body);
    CHECK(st == 500);
}

int
main(void)
{
    test_list();
    test_delete();
    if (failures) {
        fprintf(stderr, "test_content_routes: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_content_routes: %d checks passed\n", checks);
    return 0;
}
