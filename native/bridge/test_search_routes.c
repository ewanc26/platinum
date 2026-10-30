#include "search_routes.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

typedef struct fake {
    int calls;
    int last_count;
    int last_posts;
    int fail;
    char last_query[128];
} fake;

static platinum_search_result
fake_search(void *ctx, int posts, const char *query, int count, const char *cursor,
            char *body, size_t cap)
{
    fake *f = (fake *) ctx;
    (void) cursor;
    f->calls++;
    f->last_posts = posts;
    f->last_count = count;
    strcpy(f->last_query, query);
    if (f->fail) return PLATINUM_SEARCH_ERROR;
    if (cap > 16) strcpy(body, "{\"actors\":[]}");
    return PLATINUM_SEARCH_OK;
}

static void
test_limit(void)
{
    /* Each expected value was taken from Node's Number() and truncation, then clamping. */
    CHECK(platinum_query_limit(NULL) == 20);
    CHECK(platinum_query_limit("") == 1);
    CHECK(platinum_query_limit("   ") == 1);
    CHECK(platinum_query_limit("12") == 12);
    CHECK(platinum_query_limit(" 12 ") == 12);
    CHECK(platinum_query_limit("12.9") == 12);
    CHECK(platinum_query_limit("-3") == 1);
    CHECK(platinum_query_limit("0") == 1);
    CHECK(platinum_query_limit("1000") == 50);
    CHECK(platinum_query_limit("1e3") == 50);
    CHECK(platinum_query_limit("1e1") == 10);
    CHECK(platinum_query_limit("0x1F") == 31);
    CHECK(platinum_query_limit("0x100") == 50);
    CHECK(platinum_query_limit("abc") == 20);
    CHECK(platinum_query_limit("12abc") == 20);
    CHECK(platinum_query_limit("inf") == 20);
    CHECK(platinum_query_limit("Infinity") == 20);
    CHECK(platinum_query_limit("NaN") == 20);
}

static void
test_route(void)
{
    fake f;
    char body[128];
    int st;

    memset(&f, 0, sizeof f);
    st = platinum_route_search(&f, fake_search, 0, "  welsh borders ", "5", NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(f.last_posts == 0);
    CHECK(f.last_count == 5);
    CHECK(strcmp(f.last_query, "welsh borders") == 0);

    memset(&f, 0, sizeof f);
    st = platinum_route_search(&f, fake_search, 1, "rivers", NULL, NULL, body, sizeof body);
    CHECK(st == 200 && f.last_posts == 1 && f.last_count == 20);

    /* Missing or invalid query is refused before the search runs. */
    memset(&f, 0, sizeof f);
    st = platinum_route_search(&f, fake_search, 0, NULL, NULL, NULL, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);
    CHECK(strcmp(body, "{\"error\":\"invalid_query\",\"message\":\"q must be 1 to 100 characters.\"}") == 0);
    st = platinum_route_search(&f, fake_search, 0, "   ", NULL, NULL, body, sizeof body);
    CHECK(st == 400 && f.calls == 0);

    memset(&f, 0, sizeof f);
    f.fail = 1;
    st = platinum_route_search(&f, fake_search, 0, "rivers", NULL, NULL, body, sizeof body);
    CHECK(st == 500);
    CHECK(strstr(body, "bridge_error") != NULL);
}

int
main(void)
{
    test_limit();
    test_route();
    if (failures) {
        fprintf(stderr, "test_search_routes: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_search_routes: %d checks passed\n", checks);
    return 0;
}
