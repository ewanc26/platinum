#include "account_routes.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

typedef struct fake {
    int calls;
    int fail;
    char last[256];
    int last_on;
} fake;

static platinum_account_result
fake_seen(void *ctx, const char *seen_at)
{
    fake *f = (fake *) ctx;
    f->calls++;
    strcpy(f->last, seen_at);
    return f->fail ? PLATINUM_ACCOUNT_ERROR : PLATINUM_ACCOUNT_OK;
}

static platinum_account_result
fake_word(void *ctx, const char *word, int on)
{
    fake *f = (fake *) ctx;
    f->calls++;
    strcpy(f->last, word);
    f->last_on = on;
    return f->fail ? PLATINUM_ACCOUNT_ERROR : PLATINUM_ACCOUNT_OK;
}

#define NOW 1792000000000.0

static void
test_seen(void)
{
    fake f;
    char body[256];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_seen(&f, fake_seen, "2026-10-15T12:00:00Z", 2000000000000.0, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"seenAt\":\"2026-10-15T12:00:00.000Z\"}") == 0);
    CHECK(strcmp(f.last, "2026-10-15T12:00:00.000Z") == 0);

    /* A future time is stored as now. */
    memset(&f, 0, sizeof f);
    st = platinum_route_seen(&f, fake_seen, "2099-01-01T00:00:00Z", NOW, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(f.last, "2026-10-14T17:46:40.000Z") == 0);

    /* Invalid, or missing, is refused before the operation runs. */
    memset(&f, 0, sizeof f);
    st = platinum_route_seen(&f, fake_seen, "yesterday", NOW, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strcmp(body, "{\"error\":\"invalid_seen_at\",\"message\":\"seenAt must be an ISO 8601 timestamp.\"}") == 0);
    st = platinum_route_seen(&f, fake_seen, NULL, NOW, body, sizeof body);
    CHECK(st == 400);

    memset(&f, 0, sizeof f);
    f.fail = 1;
    st = platinum_route_seen(&f, fake_seen, "2026-10-15T12:00:00Z", NOW, body, sizeof body);
    CHECK(st == 500);
}

static void
test_muted_set(void)
{
    fake f;
    char body[256];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_muted_set(&f, fake_word, "  spoilers ", 1, 1, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"value\":\"spoilers\",\"on\":true}") == 0);
    CHECK(strcmp(f.last, "spoilers") == 0 && f.last_on == 1);

    /* A quote in the word is escaped, so the body stays valid JSON. */
    memset(&f, 0, sizeof f);
    st = platinum_route_muted_set(&f, fake_word, "say \"hi\"", 1, 0, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"value\":\"say \\\"hi\\\"\",\"on\":false}") == 0);

    memset(&f, 0, sizeof f);
    st = platinum_route_muted_set(&f, fake_word, "   ", 1, 1, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_word\"") != NULL);
    st = platinum_route_muted_set(&f, fake_word, "ok", 0, 1, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);

    memset(&f, 0, sizeof f);
    f.fail = 1;
    st = platinum_route_muted_set(&f, fake_word, "ok", 1, 1, body, sizeof body);
    CHECK(st == 500);

    /* A body too small for the answer becomes the generic 500. */
    memset(&f, 0, sizeof f);
    st = platinum_route_muted_set(&f, fake_word, "ok", 1, 1, body, 20);
    CHECK(st == 500);
}

int
main(void)
{
    test_seen();
    test_muted_set();
    if (failures) {
        fprintf(stderr, "test_account_routes: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_account_routes: %d checks passed\n", checks);
    return 0;
}
