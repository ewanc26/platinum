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

static const char *page_of(int first, int n, const char *cursor)
{
    static char body[20000];
    char item[260];
    int i;

    strcpy(body, "{\"posts\":[");
    for (i = 0; i < n; ++i) {
        sprintf(item, "%s{\"uri\":\"at://p/%d\",\"cid\":\"c\",\"author\":{\"did\":\"did:plc:a\"},\"text\":\"t%d\"}",
                i ? "," : "", first + i, first + i);
        strcat(body, item);
    }
    strcat(body, "]");
    if (cursor != NULL) {
        strcat(body, ",\"cursor\":\"");
        strcat(body, cursor);
        strcat(body, "\"");
    }
    strcat(body, "}");
    return body;
}

static void test_list(void)
{
    static platinum_thread thread;
    platinum_bridge_client *bridge;
    unsigned short dropped;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_thread_init(&thread);
    check(!platinum_thread_has_more(&thread), "a thread never pages");

    respond(WF_OK, page_of(0, 20, "c1"));
    check(platinum_thread_load_list(&thread, bridge, "/v1/author-feed", "actor", "bob.test",
                                    "Posts by this author") == WF_OK, "list loads");
    check(strstr(last_url, "/v1/author-feed?actor=bob.test") != NULL, "route and parameter");
    check(thread.count == 20 && thread.items[0].depth == 0 && thread.scroll_row == 0, "20 posts, no depths");
    check(strcmp(thread.heading, "Posts by this author") == 0, "heading kept");
    check(platinum_thread_has_more(&thread), "a cursor means more");

    respond(WF_OK, page_of(20, 20, "c2"));
    check(platinum_thread_load_more(&thread, bridge, &dropped) == WF_OK && thread.count == 40 && dropped == 0,
          "second page appended");
    check(strstr(last_url, "actor=bob.test&cursor=c1") != NULL, "cursor in the request");

    respond(WF_OK, page_of(40, 20, NULL));
    check(platinum_thread_load_more(&thread, bridge, &dropped) == WF_OK && dropped == 20 &&
              thread.count == 40 && strcmp(thread.items[0].post.uri, "at://p/20") == 0,
          "cap at 40, first rows dropped");
    check(!platinum_thread_has_more(&thread), "no cursor ends the list");

    respond(WF_ERR_HTTP, "{}");
    fake_http_status = 500;
    check(platinum_thread_load_list(&thread, bridge, "/v1/feed", "uri", "at://x", "Feed") != WF_OK &&
              strcmp(thread.status, "The posts could not be loaded.") == 0,
          "failure says so");

    respond(WF_OK, page_of(0, 20, "c9"));
    platinum_thread_load_list(&thread, bridge, "/v1/feed", "uri", "at://x", "Feed");
    respond(WF_OK, "{\"posts\":{}}");
    check(platinum_thread_load_more(&thread, bridge, &dropped) == WF_ERR_PARSE &&
              thread.count == 20 && dropped == 0,
          "a malformed page leaves the loaded posts alone");

    last_method = 0;
    check(platinum_thread_load_list(&thread, bridge, "/v1/feed", "uri", "", "x") == WF_ERR_INVALID_ARG &&
              last_method == 0, "an empty value makes no request");
    platinum_bridge_client_free(bridge);
}

int main(void)
{
    test_list();
    test_load();
    test_bound();
    if (failures != 0) {
        printf("test_thread: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_thread: all %d checks passed\n", checks);
    return 0;
}
