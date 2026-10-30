/* The notifications route: limit and cursor rules, JSON, and failures. */
#include "notifications_route.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

enum { FAKE_PAGE = 0, FAKE_FAIL, FAKE_EMPTY, FAKE_HUGE };

typedef struct fake {
    int mode;
    int calls;
    int last_limit;
    int had_cursor;
    char last_cursor[256];
} fake;

static platinum_notifications_result
fake_fetch(void *ctx, int limit, const char *cursor, platinum_notifications *out)
{
    fake *f = (fake *) ctx;
    platinum_notification *item;

    f->calls++;
    f->last_limit = limit;
    f->had_cursor = cursor != NULL;
    if (cursor != NULL) strcpy(f->last_cursor, cursor);

    if (f->mode == FAKE_FAIL) return PLATINUM_NOTIFICATIONS_ERROR;
    if (f->mode == FAKE_EMPTY) return PLATINUM_NOTIFICATIONS_OK;
    if (f->mode == FAKE_HUGE) {
        out->item_count = PLATINUM_NOTIFICATIONS_MAX + 1;
        out->items = calloc(out->item_count, sizeof *out->items);
        return out->items != NULL ? PLATINUM_NOTIFICATIONS_OK
                                  : PLATINUM_NOTIFICATIONS_ERROR;
    }

    out->items = calloc(2, sizeof *out->items);
    if (out->items == NULL) return PLATINUM_NOTIFICATIONS_ERROR;
    out->item_count = 2;

    item = &out->items[0];
    strcpy(item->uri, "at://did:plc:abc/app.bsky.feed.post/3k1");
    strcpy(item->cid, "bafyone");
    strcpy(item->author.did, "did:plc:bob");
    strcpy(item->author.handle, "bob.test");
    strcpy(item->author.display_name, "Bob \"the builder\"\n");
    strcpy(item->author.avatar, "avatar_thumbnail/plain/did:plc:bob/bafyav@jpeg");
    strcpy(item->reason, "like");
    strcpy(item->indexed_at, "2026-10-01T10:00:00.000Z");
    item->is_read = 0;

    item = &out->items[1];
    strcpy(item->uri, "at://did:plc:abc/app.bsky.feed.post/3k2");
    strcpy(item->cid, "bafytwo");
    strcpy(item->author.did, "did:plc:carol");
    strcpy(item->reason, "follow");
    strcpy(item->indexed_at, "2026-10-01T09:00:00.000Z");
    item->is_read = 1;

    strcpy(out->cursor, "next-page");
    out->has_cursor = 1;
    return PLATINUM_NOTIFICATIONS_OK;
}

static void
test_page(void)
{
    fake f;
    char body[4096];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_notifications(&f, fake_fetch, NULL, NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(f.calls == 1);
    CHECK(f.last_limit == 20);
    CHECK(!f.had_cursor);
    CHECK(strncmp(body, "{\"notifications\":[", 18) == 0);
    CHECK(strstr(body, "\"uri\":\"at://did:plc:abc/app.bsky.feed.post/3k1\"") != NULL);
    CHECK(strstr(body, "\"cid\":\"bafyone\"") != NULL);
    CHECK(strstr(body, "\"author\":{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\",") != NULL);
    CHECK(strstr(body, "\"displayName\":\"Bob \\\"the builder\\\"\\n\"") != NULL);
    CHECK(strstr(body, "\"avatar\":\"avatar_thumbnail/plain/did:plc:bob/bafyav@jpeg\"") != NULL);
    CHECK(strstr(body, "\"reason\":\"like\"") != NULL);
    CHECK(strstr(body, "\"indexedAt\":\"2026-10-01T10:00:00.000Z\"") != NULL);
    CHECK(strstr(body, "\"isRead\":false") != NULL);
    CHECK(strstr(body, "\"cursor\":\"next-page\"") != NULL);
    CHECK(body[strlen(body) - 1] == '}');

    /* The second notification has no optional author fields; they are left out
     * rather than sent empty, as api.ts does. */
    CHECK(strstr(body, "{\"did\":\"did:plc:carol\"}") != NULL);
    CHECK(strstr(body, "\"reason\":\"follow\"") != NULL);
    CHECK(strstr(body, "\"isRead\":true") != NULL);

    /* limit and cursor are the caller's, passed through. */
    memset(&f, 0, sizeof f);
    st = platinum_route_notifications(&f, fake_fetch, "5", "abc", body, sizeof body);
    CHECK(st == 200);
    CHECK(f.last_limit == 5);
    CHECK(f.had_cursor && strcmp(f.last_cursor, "abc") == 0);

    /* The same clamping as server.ts: over the top is 50, below is 1, and a
     * value that is not a number is the default. */
    memset(&f, 0, sizeof f);
    st = platinum_route_notifications(&f, fake_fetch, "500", NULL, body, sizeof body);
    CHECK(st == 200 && f.last_limit == 50);
    memset(&f, 0, sizeof f);
    st = platinum_route_notifications(&f, fake_fetch, "0", NULL, body, sizeof body);
    CHECK(st == 200 && f.last_limit == 1);
    memset(&f, 0, sizeof f);
    st = platinum_route_notifications(&f, fake_fetch, "abc", NULL, body, sizeof body);
    CHECK(st == 200 && f.last_limit == 20);

    /* An empty page is a notifications array and nothing else. */
    memset(&f, 0, sizeof f);
    f.mode = FAKE_EMPTY;
    st = platinum_route_notifications(&f, fake_fetch, NULL, NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"notifications\":[]}") == 0);
}

static void
test_errors(void)
{
    fake f;
    char body[4096];
    char cursor[300];
    int st;

    memset(&f, 0, sizeof f);
    f.mode = FAKE_FAIL;
    st = platinum_route_notifications(&f, fake_fetch, NULL, NULL, body, sizeof body);
    CHECK(st == 500);
    CHECK(strcmp(body, "{\"error\":\"bridge_error\",\"message\":\"The backend could not complete the request.\"}") == 0);

    /* A cursor that cannot be echoed back is refused before the fetch. */
    memset(&f, 0, sizeof f);
    memset(cursor, 'a', sizeof cursor - 1);
    cursor[sizeof cursor - 1] = '\0';
    st = platinum_route_notifications(&f, fake_fetch, NULL, cursor, body, sizeof body);
    CHECK(st == 400);
    CHECK(f.calls == 0);
    CHECK(strstr(body, "invalid_cursor") != NULL);

    /* A page too large for the response buffer is a 500, not a cut body. */
    memset(&f, 0, sizeof f);
    st = platinum_route_notifications(&f, fake_fetch, NULL, NULL, body, 128);
    CHECK(st == 500);
    CHECK(strstr(body, "bridge_error") != NULL);

    /* A fetch that fills more than one page can hold is refused. */
    memset(&f, 0, sizeof f);
    f.mode = FAKE_HUGE;
    st = platinum_route_notifications(&f, fake_fetch, NULL, NULL, body, sizeof body);
    CHECK(st == 500);

    memset(&f, 0, sizeof f);
    CHECK(platinum_route_notifications(&f, NULL, NULL, NULL, body, sizeof body) == 500);
    CHECK(platinum_route_notifications(&f, fake_fetch, NULL, NULL, NULL, 0) == 500);
    CHECK(f.calls == 0);
}

static void
test_free(void)
{
    platinum_notifications page;
    fake f;

    memset(&page, 0, sizeof page);
    memset(&f, 0, sizeof f);
    CHECK(fake_fetch(&f, 20, NULL, &page) == PLATINUM_NOTIFICATIONS_OK);
    CHECK(page.items != NULL && page.item_count == 2);
    platinum_notifications_free(&page);
    CHECK(page.items == NULL && page.item_count == 0);
    platinum_notifications_free(&page);
    platinum_notifications_free(NULL);
}

int
main(void)
{
    test_page();
    test_errors();
    test_free();
    if (failures) {
        fprintf(stderr, "test_notifications_route: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_notifications_route: %d checks passed\n", checks);
    return 0;
}
