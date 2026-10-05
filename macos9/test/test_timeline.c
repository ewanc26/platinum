/*
 * test_timeline.c -- host-side tests for timeline paging.
 *
 * The real bridge client is linked; only the Wolfram transport underneath it
 * is stubbed (wolfram_stub.c), so these tests see the exact request path the
 * client builds, cursor escaping included.
 *
 * This is a logic and dialect check on a modern compiler. It is not Classic
 * Mac OS 9 hardware validation.
 */

#include <stdio.h>
#include <string.h>

#include "timeline.h"
#include "wolfram_stub.h"

static int failures;
static int checks;
static char page[16384];

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

/* A page of `n` posts whose uris are at://p/<first>..<first+n-1>. */
static const char *make_page(int first, int n, const char *cursor)
{
    int i;
    char item[256];

    strcpy(page, "{\"posts\":[");
    for (i = 0; i < n; ++i) {
        sprintf(item,
                "%s{\"uri\":\"at://p/%d\",\"cid\":\"c%d\",\"author\":{\"did\":"
                "\"did:plc:a\",\"handle\":\"a.test\"},\"text\":\"post %d\"}",
                i == 0 ? "" : ",", first + i, first + i, first + i);
        strcat(page, item);
    }
    strcat(page, "]");
    if (cursor != NULL) {
        strcat(page, ",\"cursor\":\"");
        strcat(page, cursor);
        strcat(page, "\"");
    }
    strcat(page, "}");
    return page;
}

static void respond(int status, const char *body)
{
    fake_status = status;
    fake_http_status = status == WF_OK ? 200 : 500;
    fake_body = body;
    last_url[0] = '\0';
    last_method = 0;
}

static void test_paging(void)
{
    static platinum_timeline timeline;
    platinum_bridge_client *bridge;
    unsigned short dropped;
    wf_status status;

    bridge = platinum_bridge_client_new("https://bridge.example");
    check(bridge != NULL, "client created");
    platinum_timeline_init(&timeline);
    check(!platinum_timeline_has_older(&timeline), "nothing older before a load");

    respond(WF_OK, make_page(0, 20, "2026-10-05T00:00:00.000Z::bafy+/="));
    status = platinum_timeline_refresh(&timeline, bridge);
    check(status == WF_OK, "first page loads");
    check(timeline.count == 20, "first page has 20 rows");
    check(strstr(last_url, "cursor=") == NULL, "refresh sends no cursor");
    check(platinum_timeline_has_older(&timeline), "a cursor means an older page");

    respond(WF_OK, make_page(20, 20, "c2"));
    status = platinum_timeline_load_older(&timeline, bridge, &dropped);
    check(status == WF_OK, "second page loads");
    check(strstr(last_url,
                 "/v1/timeline?limit=20&cursor="
                 "2026-10-05T00%3A00%3A00.000Z%3A%3Abafy%2B%2F%3D") != NULL,
          "cursor is sent percent-encoded");
    check(timeline.count == 40 && dropped == 0, "second page appended, none dropped");
    check(strcmp(timeline.posts[20].uri, "at://p/20") == 0, "second page follows the first");

    respond(WF_OK, make_page(40, 20, "c3"));
    status = platinum_timeline_load_older(&timeline, bridge, &dropped);
    check(status == WF_OK && timeline.count == 40, "cap holds at 40 rows");
    check(dropped == 20, "the 20 newest rows were dropped");
    check(strcmp(timeline.posts[0].uri, "at://p/20") == 0, "front is now the second page");
    check(strcmp(timeline.posts[39].uri, "at://p/59") == 0, "back is the third page");

    respond(WF_ERR_HTTP, "{\"error\":\"upstream_error\"}");
    status = platinum_timeline_load_older(&timeline, bridge, &dropped);
    check(status != WF_OK, "failed page reports failure");
    check(timeline.count == 40 && dropped == 0, "failed page keeps what was loaded");
    check(strcmp(timeline.posts[0].uri, "at://p/20") == 0, "failed page leaves rows in place");
    check(strcmp(platinum_timeline_status(&timeline), "Could not load older posts.") == 0,
          "failed page says so");
    check(platinum_timeline_has_older(&timeline), "cursor survives a failed page");

    respond(WF_OK, "{\"posts\":{}}");
    status = platinum_timeline_load_older(&timeline, bridge, &dropped);
    check(status == WF_ERR_PARSE && timeline.count == 40 && dropped == 0,
          "malformed page drops nothing");

    respond(WF_OK, make_page(60, 5, NULL));
    status = platinum_timeline_load_older(&timeline, bridge, &dropped);
    check(status == WF_OK && dropped == 5 && timeline.count == 40, "short last page");
    check(!platinum_timeline_has_older(&timeline), "no cursor ends paging");

    respond(WF_OK, make_page(0, 1, "x"));
    status = platinum_timeline_load_older(&timeline, bridge, &dropped);
    check(status == WF_ERR_INVALID_ARG && last_method == 0,
          "no request is made when there is nothing older");

    platinum_bridge_client_free(bridge);
}

static void test_long_cursor(void)
{
    static platinum_timeline timeline;
    static char cursor[400];
    platinum_bridge_client *bridge;

    memset(cursor, 'a', 300);
    cursor[300] = '\0';
    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_timeline_init(&timeline);
    respond(WF_OK, make_page(0, 3, cursor));
    check(platinum_timeline_refresh(&timeline, bridge) == WF_OK, "page with long cursor loads");
    check(!platinum_timeline_has_older(&timeline),
          "a cursor too long to keep exactly ends paging instead of being cut");
    platinum_bridge_client_free(bridge);
}

static void test_escape(void)
{
    char out[8];

    check(platinum_bridge_query_escape("a-Z.0_~", out, sizeof(out)) == 7 &&
              strcmp(out, "a-Z.0_~") == 0,
          "unreserved characters pass through");
    check(platinum_bridge_query_escape("abcdefgh", out, sizeof(out)) == -1 &&
              out[0] == '\0',
          "overflow is refused, not truncated");
    check(platinum_bridge_query_escape(" &", out, sizeof(out)) == 6 &&
              strcmp(out, "%20%26") == 0,
          "reserved characters are encoded");
}

int main(void)
{
    test_paging();
    test_long_cursor();
    test_escape();
    if (failures != 0) {
        printf("test_timeline: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_timeline: all %d checks passed\n", checks);
    return 0;
}
