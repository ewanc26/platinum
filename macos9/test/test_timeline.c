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

#include "atproto_client.h"
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

static const char *make_native_page(int first, int n, const char *cursor)
{
    int i;
    char item[512];

    strcpy(page, "{\"feed\":[");
    for (i = 0; i < n; ++i) {
        sprintf(item,
                "%s{\"post\":{\"uri\":\"at://did:plc:a/app.bsky.feed.post/post-%d\",\"cid\":\"c%d\","
                "\"author\":{\"did\":\"did:plc:a\",\"handle\":\"a.test\"},"
                "\"record\":{\"text\":\"post %d\","
                "\"createdAt\":\"2026-10-05T12:34:00.000Z\"},"
                "\"likeCount\":4,\"repostCount\":2,"
                "\"viewer\":{\"like\":\"at://did:plc:a/app.bsky.feed.like/like-old\"}}}",
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

static platinum_atproto_client *make_native_client(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;

    client = platinum_atproto_client_new("https://pds.example");
    check(client != NULL, "native client created");
    if (client == NULL)
        return NULL;

    memset(&session, 0, sizeof(session));
    strcpy(session.access_jwt, "access");
    strcpy(session.refresh_jwt, "refresh");
    strcpy(session.did, "did:plc:a");
    strcpy(session.handle, "a.test");
    strcpy(session.pds_url, "https://pds.example");
    check(platinum_atproto_client_set_session(client, &session) == WF_OK,
          "native session installed");
    return client;
}

static void test_native_timeline(void)
{
    static platinum_timeline timeline;
    platinum_atproto_client *client;
    unsigned short dropped;
    int calls;

    client = make_native_client();
    if (client == NULL)
        return;

    platinum_timeline_init(&timeline);

    respond(WF_OK, make_native_page(0, 20, "n1"));
    check(platinum_timeline_refresh_native(&timeline, client) == WF_OK,
          "native first page loads");
    check(timeline.count == 20, "native first page has 20 rows");
    check(strcmp(last_nsid, "app.bsky.feed.getTimeline") == 0,
          "native refresh calls getTimeline");
    check(last_param_count == 1 &&
              strcmp(last_param_names[0], "limit") == 0 &&
              strcmp(last_param_values[0], "20") == 0,
          "native refresh sends only the page limit");
    check(strcmp(last_auth, "access") == 0,
          "native refresh carries the access token");
    check(strcmp(timeline.posts[0].time, "12:34") == 0,
          "native view reads record.createdAt");
    check(strcmp(timeline.posts[0].line1, "post 0") == 0,
          "native view reads record.text");
    check(timeline.posts[0].liked == 1,
          "native viewer.like sets the like flag");
    check(strcmp(timeline.posts[0].like_uri,
                 "at://did:plc:a/app.bsky.feed.like/like-old") == 0,
          "native viewer.like URI is retained for undo");
    check(timeline.posts[0].reposted == 0,
          "an absent viewer.repost is not a repost");
    check(timeline.posts[0].like_count == 4, "native counts are read");

    /* Undo an AppView-reported like by deleting that exact repo record. */
    respond(WF_OK, "{}");
    check(platinum_timeline_set_engagement_native(&timeline, client, 0, 0, 0)
              == WF_OK,
          "native unlike succeeds");
    check(strcmp(last_nsid, "com.atproto.repo.deleteRecord") == 0 &&
              last_method == 2,
          "native unlike uses deleteRecord");
    check(strcmp(last_body,
                 "{\"repo\":\"did:plc:a\",\"collection\":\"app.bsky.feed.like\","
                 "\"rkey\":\"like-old\"}") == 0,
          "native unlike deletes the retained record key");
    check(timeline.posts[0].liked == 0 &&
              timeline.posts[0].like_count == 3 &&
              timeline.posts[0].like_uri[0] == '\0',
          "native unlike updates the row and clears its record URI");

    /* A new like is a repo record with a UTC timestamp and post subject. */
    respond(WF_OK,
            "{\"uri\":\"at://did:plc:a/app.bsky.feed.like/like-new\","
            "\"cid\":\"bafylikenew\"}");
    check(platinum_timeline_set_engagement_native(&timeline, client, 0, 0, 1)
              == WF_OK,
          "native like succeeds");
    check(strcmp(last_nsid, "com.atproto.repo.createRecord") == 0 &&
              last_method == 2,
          "native like uses createRecord");
    check(strstr(last_body,
                 "\"collection\":\"app.bsky.feed.like\"") != NULL &&
              strstr(last_body,
                 "\"subject\":{\"uri\":\"at://did:plc:a/app.bsky.feed.post/post-0\","
                 "\"cid\":\"c0\"}") != NULL,
          "native like sends the right collection and subject");
    check(strstr(last_body, "\"createdAt\":\"") != NULL &&
              strstr(last_body, "Z\"}}") != NULL,
          "native like timestamp is UTC RFC 3339");
    check(timeline.posts[0].liked == 1 &&
              timeline.posts[0].like_count == 4 &&
              strcmp(timeline.posts[0].like_uri,
                     "at://did:plc:a/app.bsky.feed.like/like-new") == 0,
          "native like retains the createRecord URI and updates the row");

    calls = fake_call_count;
    check(platinum_timeline_set_engagement_native(&timeline, client, 0, 0, 1)
              == WF_OK &&
              fake_call_count == calls,
          "repeating the current like state does not create another record");

    /* An interaction URI from another repo must never be used for deletion. */
    strcpy(timeline.posts[0].like_uri,
           "at://did:plc:other/app.bsky.feed.like/not-mine");
    calls = fake_call_count;
    check(platinum_timeline_set_engagement_native(&timeline, client, 0, 0, 0)
              == WF_ERR_NOT_FOUND &&
              fake_call_count == calls &&
              timeline.posts[0].liked == 1,
          "native unlike refuses a record owned by another repo");
    strcpy(timeline.posts[0].like_uri,
           "at://did:plc:a/app.bsky.feed.like/like-new");

    /* A server response must also return a record in this account's repo. */
    respond(WF_OK,
            "{\"uri\":\"at://did:plc:other/app.bsky.feed.repost/not-mine\","
            "\"cid\":\"bafyforeign\"}");
    check(platinum_timeline_set_engagement_native(&timeline, client, 0, 1, 1)
              == WF_ERR_PARSE &&
              timeline.posts[0].reposted == 0 &&
              timeline.posts[0].repost_count == 2,
          "native repost rejects a createRecord URI from another repo");

    /* Reposts use their own collection and can be undone the same way. */
    respond(WF_OK,
            "{\"uri\":\"at://did:plc:a/app.bsky.feed.repost/repost-new\","
            "\"cid\":\"bafyrepostnew\"}");
    check(platinum_timeline_set_engagement_native(&timeline, client, 0, 1, 1)
              == WF_OK,
          "native repost succeeds");
    check(strcmp(last_nsid, "com.atproto.repo.createRecord") == 0 &&
              strstr(last_body,
                     "\"collection\":\"app.bsky.feed.repost\"") != NULL,
          "native repost uses its own record collection");
    check(timeline.posts[0].reposted == 1 &&
              timeline.posts[0].repost_count == 3 &&
              strcmp(timeline.posts[0].repost_uri,
                     "at://did:plc:a/app.bsky.feed.repost/repost-new") == 0,
          "native repost retains its record URI and updates the row");

    respond(WF_OK, "{}");
    check(platinum_timeline_set_engagement_native(&timeline, client, 0, 1, 0)
              == WF_OK,
          "native undo repost succeeds");
    check(strcmp(last_nsid, "com.atproto.repo.deleteRecord") == 0 &&
              strcmp(last_body,
                     "{\"repo\":\"did:plc:a\",\"collection\":\"app.bsky.feed.repost\","
                     "\"rkey\":\"repost-new\"}") == 0,
          "native undo repost deletes the retained record");
    check(timeline.posts[0].reposted == 0 &&
              timeline.posts[0].repost_count == 2 &&
              timeline.posts[0].repost_uri[0] == '\0',
          "native undo repost clears local state");

    check(platinum_timeline_has_older(&timeline),
          "native cursor allows an older page");

    respond(WF_OK, make_native_page(20, 20, NULL));
    check(platinum_timeline_load_older_native(&timeline, client, &dropped) ==
              WF_OK,
          "native second page loads");
    check(last_param_count == 2 &&
              strcmp(last_param_names[1], "cursor") == 0 &&
              strcmp(last_param_values[1], "n1") == 0,
          "native page sends the cursor");
    check(timeline.count == 40 && dropped == 0,
          "native second page appended, none dropped");
    check(!platinum_timeline_has_older(&timeline),
          "native page without a cursor ends paging");

    respond(WF_ERR_HTTP, "{\"error\":\"upstream_error\"}");
    check(platinum_timeline_refresh_native(&timeline, client) != WF_OK,
          "native failure is reported");
    check(timeline.count == 40, "native failure keeps the loaded rows");
    check(strcmp(platinum_timeline_status(&timeline),
                 "Timeline refresh failed.") == 0,
          "native failure says so");

    respond(WF_OK, "{\"feed\":{}}");
    check(platinum_timeline_refresh_native(&timeline, client) == WF_ERR_PARSE,
          "a feed that is not an array is refused");

    respond(WF_ERR_HTTP, "{\"error\":\"InvalidToken\"}");
    fake_http_status = 403;
    check(platinum_timeline_refresh_native(&timeline, client) == WF_ERR_AUTH,
          "native auth failure is reported");
    check(strcmp(platinum_timeline_status(&timeline),
                 "Session expired. Sign in again.") == 0,
          "native auth failure asks for sign-in");

    respond(WF_OK, "{\"feed\":[]}");
    check(platinum_timeline_refresh_native(&timeline, client) == WF_OK &&
              timeline.count == 0,
          "native empty feed is an empty timeline");
    check(strcmp(platinum_timeline_status(&timeline),
                 "The timeline is empty.") == 0,
          "native empty feed says so");

    respond(WF_OK,
            "{\"feed\":[{\"post\":{\"uri\":\"at://p/9\","
            "\"cid\":\"c9\",\"author\":{\"did\":\"did:plc:a\"},"
            "\"viewer\":{}}}]}");
    check(platinum_timeline_refresh_native(&timeline, client) == WF_OK &&
              timeline.count == 1,
          "a view without a record still shows");
    check(timeline.posts[0].liked == 0 && timeline.posts[0].reposted == 0,
          "an empty viewer is neither liked nor reposted");
    check(timeline.posts[0].line1[0] == '\0',
          "a view without a record has no text");

    platinum_atproto_client_free(client);
}

static const char *muted_prefs_body;
static const char *muted_feed_body;

static void muted_script(int call, int method, const char *url,
                         const char *body)
{
    (void)method;
    (void)url;
    (void)body;
    fake_http_status = 200;
    fake_status = WF_OK;
    if (call == 1)
        fake_body = muted_prefs_body;
    else
        fake_body = muted_feed_body;
}

static void test_native_muted(void)
{
    static platinum_timeline timeline;
    static char muted_page[2048];
    platinum_atproto_client *client;

    client = make_native_client();
    if (client == NULL)
        return;
    platinum_timeline_init(&timeline);

    muted_prefs_body =
        "{\"preferences\":["
        "{\"$type\":\"app.bsky.actor.defs#mutedWordsPref\",\"items\":["
        "{\"value\":\"spoiler\",\"targets\":[\"content\"]},"
        "{\"value\":\"finale\",\"targets\":[\"tag\"]}"
        "]}]}";
    strcpy(muted_page, "{\"feed\":[");
    strcat(muted_page,
           "{\"post\":{\"uri\":\"at://did:plc:a/app.bsky.feed.post/m1\","
           "\"cid\":\"c1\","
           "\"author\":{\"did\":\"did:plc:b\"},"
           "\"record\":{\"text\":\"a spoiler here\"},"
           "\"viewer\":{}}},");
    strcat(muted_page,
           "{\"post\":{\"uri\":\"at://did:plc:a/app.bsky.feed.post/m2\","
           "\"cid\":\"c2\","
           "\"author\":{\"did\":\"did:plc:b\"},"
           "\"record\":{\"text\":\"hello\","
           "\"facets\":[{\"features\":["
           "{\"$type\":\"app.bsky.richtext.facet#tag\","
           "\"tag\":\"finale\"}]}]},"
           "\"viewer\":{}}},");
    strcat(muted_page,
           "{\"post\":{\"uri\":\"at://did:plc:a/app.bsky.feed.post/keep\","
           "\"cid\":\"c3\","
           "\"author\":{\"did\":\"did:plc:b\"},"
           "\"record\":{\"text\":\"a clean post\"},"
           "\"viewer\":{}}}]}");
    muted_feed_body = muted_page;
    fake_script = muted_script;
    fake_call_count = 0;
    check(platinum_timeline_refresh_native(&timeline, client) == WF_OK,
          "muted native refresh loads");
    check(fake_call_count == 2, "prefs and timeline are both fetched");
    check(timeline.count == 1, "muted posts are skipped");
    if (timeline.count == 1)
        check(strstr(timeline.posts[0].uri, "/keep") != NULL,
              "the clean post is kept");
    fake_script = NULL;

    /* A preferences failure shows the page unfiltered. */
    muted_prefs_body = "{\"oops\"}";
    muted_feed_body =
        "{\"feed\":[{\"post\":{\"uri\":\"at://did:plc:a/" 
        "app.bsky.feed.post/p1\",\"cid\":\"c1\","
        "\"author\":{\"did\":\"did:plc:a\"},"
        "\"record\":{\"text\":\"a spoiler here\"}}}]}";
    fake_script = muted_script;
    fake_call_count = 0;
    check(platinum_timeline_refresh_native(&timeline, client) == WF_OK,
          "prefs failure still loads");
    check(timeline.count == 1, "prefs failure shows the page unfiltered");
    fake_script = NULL;

    platinum_atproto_client_free(client);
}

static void test_engagement(void)
{
    static platinum_timeline timeline;
    platinum_bridge_client *bridge;
    wf_status status;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_timeline_init(&timeline);
    respond(WF_OK,
            "{\"posts\":[{\"uri\":\"at://p/1\",\"cid\":\"c1\",\"author\":"
            "{\"did\":\"did:plc:a\"},\"likeCount\":4,\"repostCount\":2,"
            "\"liked\":true,\"reposted\":\"true\"}]}");
    check(platinum_timeline_refresh(&timeline, bridge) == WF_OK, "engagement page loads");
    check(timeline.posts[0].liked == 1, "liked flag is read");
    check(timeline.posts[0].reposted == 0, "a string \"true\" is not a boolean");

    respond(WF_OK, "{\"uri\":\"at://p/1\",\"on\":false,\"count\":3}");
    status = platinum_timeline_set_engagement(&timeline, bridge, 0, 0, 0);
    check(status == WF_OK, "unlike succeeds");
    check(last_method == 2 && strstr(last_url, "/v1/like") != NULL, "unlike posts to /v1/like");
    check(strcmp(last_body, "{\"uri\":\"at://p/1\",\"cid\":\"c1\",\"on\":false}") == 0,
          "body carries uri, cid and on");
    check(timeline.posts[0].liked == 0 && timeline.posts[0].like_count == 3,
          "row follows the bridge's answer");

    respond(WF_OK, "{\"uri\":\"at://p/1\",\"on\":true,\"count\":3}");
    status = platinum_timeline_set_engagement(&timeline, bridge, 0, 1, 1);
    check(status == WF_OK && strstr(last_url, "/v1/repost") != NULL, "repost posts to /v1/repost");
    check(timeline.posts[0].reposted == 1 && timeline.posts[0].repost_count == 3,
          "repost state and count updated");

    respond(WF_ERR_HTTP, "{\"error\":\"upstream_error\"}");
    status = platinum_timeline_set_engagement(&timeline, bridge, 0, 0, 1);
    check(status != WF_OK && timeline.posts[0].liked == 0 &&
              timeline.posts[0].like_count == 3,
          "a failed like changes nothing");
    check(strcmp(platinum_timeline_status(&timeline), "The like did not go through.") == 0,
          "a failed like says so");

    respond(WF_OK, "{\"on\":true}");
    status = platinum_timeline_set_engagement(&timeline, bridge, 0, 0, 1);
    check(status == WF_ERR_PARSE && timeline.posts[0].liked == 0,
          "a reply without a count is not trusted");

    respond(WF_OK, "{\"on\":true,\"count\":-1}");
    check(platinum_timeline_set_engagement(&timeline, bridge, 0, 0, 1) == WF_ERR_PARSE,
          "a negative count is refused");

    last_method = 0;
    check(platinum_timeline_set_engagement(&timeline, bridge, 5, 0, 1) == WF_ERR_INVALID_ARG &&
              last_method == 0,
          "out-of-range row makes no request");

    platinum_bridge_client_free(bridge);
}

int main(void)
{
    test_paging();
    test_long_cursor();
    test_escape();
    test_engagement();
    test_native_timeline();
    test_native_muted();
    fake_script = NULL;
    if (failures != 0) {
        printf("test_timeline: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_timeline: all %d checks passed\n", checks);
    return 0;
}
