/*
 * test_thread.c -- host-side tests for loading a thread. Links the real
 * thread_feed.c, timeline parser and bridge client over the stubbed Wolfram
 * transport. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "thread.h"
#include "wolfram_stub.h"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static void respond(int status, const char *body)
{
    fake_status = status;
    fake_http_status = status == WF_OK ? 200 : (status == WF_ERR_HTTP ? 404 : 500);
    fake_body = body;
    last_url[0] = '\0';
    last_method = 0;
}

#define POST(n, depth) \
    "{\"uri\":\"at://p/" n "\",\"cid\":\"c" n "\",\"author\":{\"did\":\"did:plc:a\"," \
    "\"handle\":\"a.test\"},\"text\":\"post " n "\",\"depth\":" depth "}"

static void test_load(void)
{
    static platinum_thread thread;
    platinum_bridge_client *bridge;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_thread_init(&thread);

    respond(WF_OK, "{\"posts\":[" POST("1", "-1") "," POST("2", "0") "," POST("3", "1")
                   "," POST("4", "2") "],\"truncated\":true}");
    check(platinum_thread_load(&thread, bridge, "at://did:plc:a/app.bsky.feed.post/x y")
              == WF_OK,
          "thread loads");
    check(strstr(last_url, "/v1/thread?uri=at%3A%2F%2Fdid%3Aplc%3Aa%2Fapp.bsky.feed.post%2Fx%20y")
              != NULL,
          "uri is percent-encoded into the query");
    check(thread.count == 4, "four posts");
    check(thread.items[0].depth == -1 && thread.items[3].depth == 2, "depths kept");
    check(thread.focus == 1 && thread.scroll_row == 1, "list starts at the post asked for");
    check(thread.truncated == 1 &&
              strcmp(thread.status, "Some replies are not shown.") == 0,
          "truncation is said in words");
    check(strcmp(thread.items[2].post.uri, "at://p/3") == 0, "post fields parsed");

    respond(WF_OK, "{\"posts\":[" POST("1", "99") "," POST("2", "0") ",{\"uri\":\"x\",\"depth\":0}]}");
    check(platinum_thread_load(&thread, bridge, "at://p/2") == WF_OK && thread.count == 1,
          "an out-of-range depth and a post missing fields are skipped");
    check(thread.truncated == 0 && thread.status[0] == '\0', "a clean thread has no status");

    respond(WF_OK, "{\"posts\":[]}");
    check(platinum_thread_load(&thread, bridge, "at://p/2") == WF_OK && thread.count == 0 &&
              strcmp(thread.status, "The thread is empty.") == 0,
          "an empty thread says so");

    respond(WF_OK, "{\"posts\":{}}");
    check(platinum_thread_load(&thread, bridge, "at://p/2") == WF_ERR_PARSE &&
              strcmp(thread.status, "The bridge returned an invalid thread.") == 0,
          "a malformed reply is a parse error");

    respond(WF_ERR_HTTP, "{\"error\":\"post_not_found\"}");
    check(platinum_thread_load(&thread, bridge, "at://p/2") != WF_OK &&
              strcmp(thread.status, "That post no longer exists.") == 0,
          "a 404 says the post is gone");

    last_method = 0;
    check(platinum_thread_load(&thread, bridge, "") == WF_ERR_INVALID_ARG && last_method == 0,
          "an empty uri makes no request");

    platinum_bridge_client_free(bridge);
}

static void test_bound(void)
{
    static platinum_thread thread;
    static char body[60000];
    platinum_bridge_client *bridge;
    int i;
    char item[320];

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_thread_init(&thread);
    strcpy(body, "{\"posts\":[");
    for (i = 0; i < 60; ++i) {
        sprintf(item, "%s{\"uri\":\"at://p/%d\",\"cid\":\"c\",\"author\":{\"did\":\"did:plc:a\"},"
                      "\"depth\":%d}", i ? "," : "", i, i == 0 ? 0 : 1);
        strcat(body, item);
    }
    strcat(body, "]}");
    respond(WF_OK, body);
    check(platinum_thread_load(&thread, bridge, "at://p/0") == WF_OK &&
              thread.count == PLATINUM_THREAD_MAX,
          "a reply with more than 40 posts is cut at the bound");
    platinum_bridge_client_free(bridge);
}

int main(void)
{
    test_load();
    test_bound();
    if (failures != 0) {
        printf("test_thread: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_thread: all %d checks passed\n", checks);
    return 0;
}
