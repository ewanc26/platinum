/*
 * test_people.c -- host-side tests for the account lists (followers, following,
 * likes, reposts). Real people_feed.c and bridge client over the stubbed
 * Wolfram transport. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "named_native.h"
#include "people.h"
#include "words_native.h"
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

static const char *make_page(int first, int n, const char *cursor)
{
    int i;
    char item[200];

    strcpy(page, "{\"actors\":[");
    for (i = 0; i < n; ++i) {
        sprintf(item, "%s{\"did\":\"did:plc:%d\",\"handle\":\"u%d.test\",\"displayName\":\"User %d\"}",
                i ? "," : "", first + i, first + i, first + i);
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

static void respond(int status, long http, const char *body)
{
    fake_status = status;
    fake_http_status = http;
    fake_body = body;
    last_url[0] = '\0';
    last_method = 0;
}

static void test_list(void)
{
    static platinum_people people;
    platinum_bridge_client *bridge;
    unsigned short dropped;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_people_init(&people);

    respond(WF_OK, 200, make_page(0, 20, "c1"));
    check(platinum_people_load(&people, bridge, "/v1/follows", "actor", "bob.test",
                               "Following of @bob.test") == WF_OK, "first page loads");
    check(strstr(last_url, "/v1/follows?actor=bob.test") != NULL, "route and parameter");
    check(people.count == 20 && strcmp(people.items[0].name, "User 0") == 0 &&
              strcmp(people.items[0].handle, "@u0.test") == 0,
          "rows are name and @handle");
    check(strcmp(people.heading, "Following of @bob.test") == 0, "heading kept");
    check(platinum_people_has_more(&people), "a cursor means more");

    respond(WF_OK, 200, make_page(20, 20, "c2"));
    check(platinum_people_load_more(&people, bridge, &dropped) == WF_OK && people.count == 40 &&
              dropped == 0, "second page appended");
    check(strstr(last_url, "actor=bob.test&cursor=c1") != NULL, "cursor appended to the request");

    respond(WF_OK, 200, make_page(40, 20, NULL));
    check(platinum_people_load_more(&people, bridge, &dropped) == WF_OK && dropped == 20 &&
              people.count == 40 && strcmp(people.items[0].name, "User 20") == 0,
          "cap at 40, first rows dropped");
    check(!platinum_people_has_more(&people), "no cursor ends the list");

    respond(WF_OK, 200, make_page(0, 1, "x"));
    check(platinum_people_load_more(&people, bridge, &dropped) == WF_ERR_INVALID_ARG &&
              last_method == 0, "nothing more: no request");

    respond(WF_ERR_HTTP, 404, "{}");
    check(platinum_people_load(&people, bridge, "/v1/post/likes", "uri", "at://p/1", "Liked by") != WF_OK &&
              strcmp(people.status, "That account or post no longer exists.") == 0 &&
              people.count == 0,
          "a missing post says so");

    respond(WF_OK, 200, "{\"actors\":{}}");
    check(platinum_people_load(&people, bridge, "/v1/post/likes", "uri", "at://p/1", "Liked by") == WF_ERR_PARSE,
          "a malformed list is a parse error");

    respond(WF_OK, 200, "{\"actors\":[{\"handle\":\"nodid.test\"},{\"did\":\"did:plc:only\"}]}");
    platinum_people_load(&people, bridge, "/v1/post/likes", "uri", "at://p/1", "Liked by");
    check(people.count == 1 && strcmp(people.items[0].name, "did:plc:only") == 0 &&
              people.items[0].handle[0] == '\0',
          "an account without a did is skipped; one with only a did shows the did");

    last_method = 0;
    check(platinum_people_load(&people, bridge, "/v1/follows", "actor", "", "x") == WF_ERR_INVALID_ARG &&
              last_method == 0, "an empty value makes no request");
    platinum_bridge_client_free(bridge);
}

static void test_named(void)
{
    static platinum_people people;
    platinum_bridge_client *bridge;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_people_init(&people);

    respond(WF_OK, 200, "{\"items\":[{\"uri\":\"at://did:plc:a/app.bsky.feed.generator/g\",\"name\":\"Great\"},"
                        "{\"name\":\"no uri\"},{\"uri\":\"at://x\"}]}");
    check(platinum_people_load_kind(&people, bridge, PLATINUM_PEOPLE_FEEDS, "/v1/feeds",
                                    NULL, NULL, "Saved feeds") == WF_OK, "feeds load");
    check(strcmp(last_url, "https://bridge.example/v1/feeds") == 0, "no parameter when there is no key");
    check(people.count == 1 && strcmp(people.items[0].name, "Great") == 0 &&
              strcmp(people.items[0].uri, "at://did:plc:a/app.bsky.feed.generator/g") == 0,
          "a feed row carries its name and exact uri; rows missing either are skipped");
    check(!platinum_people_has_more(&people), "feeds have no more pages");
    check(platinum_people_selection(&people) == NULL, "nothing selected yet");
    people.selected = 0;
    check(platinum_people_selection(&people) == &people.items[0], "selection returns the row");
    people.selected = 5;
    check(platinum_people_selection(&people) == NULL, "an out-of-range selection is none");

    respond(WF_OK, 200, "{\"actors\":[]}");
    check(platinum_people_load_kind(&people, bridge, PLATINUM_PEOPLE_FEEDS, "/v1/feeds",
                                    NULL, NULL, "x") == WF_ERR_PARSE,
          "a feeds list must use items, not actors");
    check(platinum_people_load_kind(&people, bridge, 9, "/v1/feeds", NULL, NULL, "x") ==
              WF_ERR_INVALID_ARG, "an unknown kind is refused");
    check(platinum_people_load(&people, bridge, "/v1/follows", NULL, NULL, "x") ==
              WF_ERR_INVALID_ARG, "the account loader still needs a key");

    respond(WF_OK, 200, make_page(0, 2, NULL));
    platinum_people_load(&people, bridge, "/v1/follows", "actor", "bob.test", "x");
    check(strcmp(people.items[1].uri, "did:plc:1") == 0, "an account row carries its exact DID");
    check(people.selected == -1, "a fresh load clears the selection");
    platinum_bridge_client_free(bridge);
}

static void test_words(void)
{
    static platinum_people people;
    platinum_bridge_client *bridge;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_people_init(&people);
    respond(WF_OK, 200, "{\"words\":[{\"value\":\"spoilers\",\"targets\":[\"content\"]},"
                        "{\"targets\":[]},{\"value\":\"two words\"}]}");
    check(platinum_people_load_kind(&people, bridge, PLATINUM_PEOPLE_WORDS, "/v1/muted-words",
                                    NULL, NULL, "Muted words") == WF_OK, "muted words load");
    check(strcmp(last_url, "https://bridge.example/v1/muted-words") == 0, "requested without a parameter");
    check(people.count == 2 && strcmp(people.items[0].name, "spoilers") == 0 &&
              strcmp(people.items[1].uri, "two words") == 0,
          "each row carries its word; a row with no value is skipped");
    check(!platinum_people_has_more(&people), "words are not paged");

    respond(WF_OK, 200, "{\"words\":[]}");
    platinum_people_load_kind(&people, bridge, PLATINUM_PEOPLE_WORDS, "/v1/muted-words",
                              NULL, NULL, "Muted words");
    check(people.count == 0 && strcmp(people.status, "No muted words.") == 0, "empty says so");

    respond(WF_OK, 200, "{\"items\":[]}");
    check(platinum_people_load_kind(&people, bridge, PLATINUM_PEOPLE_WORDS, "/v1/muted-words",
                                    NULL, NULL, "x") == WF_ERR_PARSE,
          "a words list must use words, not items");
    platinum_bridge_client_free(bridge);
}

static const char *native_get_body;
static const char *native_put_body;

static void words_native_script(int call, int method, const char *url,
                                const char *body)
{
    (void)method;
    (void)url;
    (void)body;
    fake_http_status = 200;
    fake_status = WF_OK;
    fake_body = call == 1 ? native_get_body : native_put_body;
}

static platinum_atproto_client *words_client(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;

    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL)
        return NULL;
    memset(&session, 0, sizeof(session));
    strcpy(session.access_jwt, "access");
    strcpy(session.refresh_jwt, "refresh");
    strcpy(session.did, "did:plc:me");
    strcpy(session.handle, "me.test");
    strcpy(session.pds_url, "https://pds.example");
    if (platinum_atproto_client_set_session(client, &session) != WF_OK) {
        platinum_atproto_client_free(client);
        return NULL;
    }
    return client;
}

/* A getPreferences body with `count` content words. */
static const char *make_native_prefs(int count)
{
    static char body[4096];
    char item[64];
    int i;

    strcpy(body, "{\"preferences\":[{\"$type\":"
                 "\"app.bsky.actor.defs#mutedWordsPref\",\"items\":[");
    for (i = 0; i < count; i++) {
        sprintf(item, "%s{\"value\":\"w%d\",\"targets\":[\"content\"]}",
                i ? "," : "", i);
        strcat(body, item);
    }
    strcat(body, "]}]}");
    return body;
}

static void test_words_native(void)
{
    static platinum_people people;
    platinum_muted_list list;
    platinum_atproto_client *client;
    char value[16];
    int i;

    client = words_client();
    if (client == NULL) {
        check(0, "the test client could be built");
        return;
    }
    platinum_people_init(&people);

    /* The pure fill. */
    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoilers", 1, 0, 0, NULL);
    platinum_muted_add(&list, "two words", 1, 0, 0, NULL);
    platinum_muted_add(&list, "\xE2\x98\x95", 1, 0, 0, NULL); /* not MacRoman */
    check(platinum_people_set_words(&people, &list, "Muted words") == WF_OK,
          "the native list fills the window");
    check(people.kind == PLATINUM_PEOPLE_WORDS && people.count == 2,
          "a word that cannot be shown is left out");
    check(strcmp(people.items[0].name, "spoilers") == 0 &&
              strcmp(people.items[0].uri, "spoilers") == 0,
          "a row shows its word and keeps it exactly");
    check(strcmp(people.items[1].uri, "two words") == 0,
          "the exact UTF-8 is what a click removes");
    check(!platinum_people_has_more(&people), "the native list is not paged");
    check(platinum_people_set_words(&people, NULL, "x") == WF_ERR_INVALID_ARG,
          "a NULL list is refused");

    platinum_muted_clear(&list);
    for (i = 0; i < PLATINUM_MUTED_MAX; i++) {
        sprintf(value, "w%d", i);
        platinum_muted_add(&list, value, 1, 0, 0, NULL);
    }
    platinum_people_set_words(&people, &list, "Muted words");
    check(list.count == PLATINUM_MUTED_MAX &&
              people.count == PLATINUM_PEOPLE_MAX,
          "the window shows at most its row cap");

    platinum_muted_clear(&list);
    platinum_people_set_words(&people, &list, "Muted words");
    check(people.count == 0 &&
              strcmp(people.status, "No muted words.") == 0,
          "an empty native list says so");

    /* The screen over the transport. */
    native_get_body =
        "{\"preferences\":[{\"$type\":"
        "\"app.bsky.actor.defs#mutedWordsPref\",\"items\":["
        "{\"value\":\"spoilers\",\"targets\":[\"content\"]}]}]}";
    native_put_body = "{}";
    fake_script = words_native_script;
    fake_call_count = 0;
    check(platinum_words_show_native(&people, client) == WF_OK,
          "the words screen loads natively");
    check(strcmp(last_nsid, "app.bsky.actor.getPreferences") == 0,
          "show reads getPreferences");
    check(people.count == 1 && strcmp(people.items[0].name, "spoilers") == 0,
          "the list lands in the window");
    check(strstr(people.heading, "Remove Muted Word") != NULL,
          "the heading explains removal");
    check(people.loading == 0, "the load finished");

    /* Add: read, change, write the whole list back. */
    fake_call_count = 0;
    check(platinum_words_add_native(client, "fresh") == WF_OK,
          "a word is added natively");
    check(fake_call_count == 2, "add reads then writes");
    check(strcmp(last_nsid, "app.bsky.actor.putPreferences") == 0,
          "add ends at putPreferences");
    check(strstr(last_body, "\"value\":\"spoilers\"") != NULL &&
              strstr(last_body, "\"value\":\"fresh\"") != NULL,
          "add writes the whole list");

    /* A full list is refused without a write. */
    native_get_body = make_native_prefs(PLATINUM_MUTED_MAX);
    fake_call_count = 0;
    check(platinum_words_add_native(client, "fresh") == WF_ERR_STATE,
          "a full list is refused");
    check(fake_call_count == 1, "no write happens when the list is full");

    /* Remove: read, drop the word, write the rest back. */
    native_get_body =
        "{\"preferences\":[{\"$type\":"
        "\"app.bsky.actor.defs#mutedWordsPref\",\"items\":["
        "{\"value\":\"spoilers\",\"targets\":[\"content\"]},"
        "{\"value\":\"fresh\",\"targets\":[\"content\"]}]}]}";
    fake_call_count = 0;
    check(platinum_words_remove_native(client, "spoilers") == WF_OK,
          "a word is removed natively");
    check(fake_call_count == 2, "remove reads then writes");
    check(strstr(last_body, "spoilers") == NULL &&
              strstr(last_body, "\"value\":\"fresh\"") != NULL,
          "remove writes the list without the word");

    fake_call_count = 0;
    check(platinum_words_remove_native(client, "absent") == WF_ERR_NOT_FOUND,
          "an absent word is not found");
    check(fake_call_count == 1, "no write happens when nothing is removed");

    /* A failed read is reported, not swallowed. */
    fake_script = NULL;
    respond(WF_ERR_NETWORK, 0, "");
    check(platinum_words_show_native(&people, client) == WF_ERR_NETWORK,
          "a failed load is returned");
    check(strcmp(people.status, "The words could not be loaded.") == 0,
          "the status says so");

    platinum_atproto_client_free(client);
}

static const char *named_prefs_body;
static const char *named_generators_body;
static const char *named_generators_body2;

static void named_feeds_script(int call, int method, const char *url,
                               const char *body)
{
    (void)method;
    (void)url;
    (void)body;
    fake_status = WF_OK;
    fake_http_status = 200;
    if (call == 1)
        fake_body = named_prefs_body;
    else if (call == 2)
        fake_body = named_generators_body;
    else
        fake_body = named_generators_body2;
}

static void test_named_native(void)
{
    static platinum_people people;
    static char nine_prefs[4096];
    static char page_one[2048];
    static char page_two[512];
    static char item[192];
    platinum_atproto_client *client;
    platinum_atproto_client *blank;
    int i;

    client = words_client();
    if (client == NULL) {
        check(0, "the test client could be built");
        return;
    }
    platinum_people_init(&people);

    /* Saved feeds: the preference, then one getFeedGenerators for the names. */
    named_prefs_body =
        "{\"preferences\":[{\"$type\":\"app.bsky.actor.defs#savedFeedsPrefV2\","
        "\"items\":["
        "{\"type\":\"timeline\",\"value\":\"following\",\"pinned\":true},"
        "{\"type\":\"list\","
        "\"value\":\"at://did:plc:a/app.bsky.graph.list/l\"},"
        "{\"type\":\"feed\","
        "\"value\":\"at://did:plc:a/app.bsky.feed.generator/g1\","
        "\"pinned\":true},"
        "{\"type\":\"feed\","
        "\"value\":\"at://did:plc:a/app.bsky.feed.generator/g2\"}"
        "]}]}";
    named_generators_body =
        "{\"feeds\":["
        "{\"uri\":\"at://did:plc:a/app.bsky.feed.generator/g1\","
        "\"displayName\":\"Great feed\"},"
        "{\"uri\":\"at://did:plc:a/app.bsky.feed.generator/g2\","
        "\"displayName\":\"Other feed\"}]}";
    named_generators_body2 = NULL;
    fake_script = named_feeds_script;
    fake_call_count = 0;
    check(platinum_named_feeds_native(&people, client,
                                      "Saved feeds (click one)") == WF_OK,
          "saved feeds load natively");
    check(fake_call_count == 2, "prefs then generators");
    check(strcmp(last_nsid, "app.bsky.feed.getFeedGenerators") == 0,
          "names come from getFeedGenerators");
    check(last_param_count == 2 &&
              strcmp(last_param_names[0], "feeds") == 0 &&
              strcmp(last_param_values[0],
                     "at://did:plc:a/app.bsky.feed.generator/g1") == 0 &&
              strcmp(last_param_values[1],
                     "at://did:plc:a/app.bsky.feed.generator/g2") == 0,
          "the saved URIs are the repeated feeds parameter");
    check(people.kind == PLATINUM_PEOPLE_FEEDS && people.count == 2 &&
              strcmp(people.items[0].name, "Great feed") == 0 &&
              strcmp(people.items[0].uri,
                     "at://did:plc:a/app.bsky.feed.generator/g1") == 0 &&
              strcmp(people.items[1].name, "Other feed") == 0,
          "rows carry the display name and exact URI, in saved order");
    check(strcmp(people.heading, "Saved feeds (click one)") == 0,
          "the heading is kept");
    check(!platinum_people_has_more(&people), "saved feeds do not page");
    check(people.loading == 0, "the load finished");

    /* Nine feeds take two calls: one query carries at most eight params. */
    strcpy(nine_prefs,
           "{\"preferences\":[{\"$type\":"
           "\"app.bsky.actor.defs#savedFeedsPrefV2\",\"items\":[");
    for (i = 0; i < 9; i++) {
        sprintf(item,
                "%s{\"type\":\"feed\",\"value\":"
                "\"at://did:plc:a/app.bsky.feed.generator/f%d\"}",
                i ? "," : "", i);
        strcat(nine_prefs, item);
    }
    strcat(nine_prefs, "]}]}");
    strcpy(page_one, "{\"feeds\":[");
    for (i = 0; i < 8; i++) {
        sprintf(item,
                "%s{\"uri\":\"at://did:plc:a/app.bsky.feed.generator/f%d\","
                "\"displayName\":\"Feed %d\"}",
                i ? "," : "", i, i);
        strcat(page_one, item);
    }
    strcat(page_one, "]}");
    strcpy(page_two,
           "{\"feeds\":[{\"uri\":"
           "\"at://did:plc:a/app.bsky.feed.generator/f8\","
           "\"displayName\":\"Feed 8\"}]}");

    named_prefs_body = nine_prefs;
    named_generators_body = page_one;
    named_generators_body2 = page_two;
    fake_call_count = 0;
    check(platinum_named_feeds_native(&people, client, "Saved feeds") == WF_OK,
          "nine saved feeds load natively");
    check(fake_call_count == 3 && last_param_count == 1,
          "the names are asked for in batches of eight");
    check(people.count == 9 &&
              strcmp(people.items[0].name, "Feed 0") == 0 &&
              strcmp(people.items[8].name, "Feed 8") == 0,
          "every saved feed is named, in order");

    named_prefs_body = "{\"preferences\":[]}";
    fake_call_count = 0;
    check(platinum_named_feeds_native(&people, client, "x") == WF_OK &&
              people.count == 0 &&
              strcmp(people.status, "Nobody to show.") == 0,
          "no saved feeds says so");
    check(fake_call_count == 1, "no generator call without feeds");

    named_prefs_body = "{\"preferences\":{}}";
    check(platinum_named_feeds_native(&people, client, "x") == WF_ERR_PARSE &&
              strcmp(people.status,
                     "The server returned an invalid list.") == 0,
          "a malformed preferences body is a parse error");

    fake_script = NULL;
    respond(WF_ERR_NETWORK, 0, "");
    check(platinum_named_feeds_native(&people, client, "x") == WF_ERR_NETWORK &&
              strcmp(people.status,
                     "The saved feeds could not be loaded.") == 0,
          "a failed saved-feeds load is reported");

    blank = platinum_atproto_client_new("https://pds.example");
    check(blank != NULL &&
              platinum_named_feeds_native(&people, blank, "x") == WF_ERR_AUTH &&
              strcmp(people.status, "Session expired. Sign in again.") == 0,
          "a client with no session is an auth failure");
    platinum_atproto_client_free(blank);

    /* My Lists: one getLists for the signed-in DID. */
    fake_script = NULL;
    respond(WF_OK, 200,
            "{\"lists\":["
            "{\"uri\":\"at://did:plc:me/app.bsky.graph.list/1\","
            "\"name\":\"Friends\"},"
            "{\"uri\":\"at://did:plc:me/app.bsky.graph.list/2\","
            "\"name\":\"Reading\"},"
            "{\"name\":\"no uri\"}]}");
    check(platinum_named_lists_native(&people, client,
                                      "My lists (click one)") == WF_OK,
          "lists load natively");
    check(strcmp(last_nsid, "app.bsky.graph.getLists") == 0,
          "getLists is the call");
    check(last_param_count == 2 &&
              strcmp(last_param_names[0], "actor") == 0 &&
              strcmp(last_param_values[0], "did:plc:me") == 0 &&
              strcmp(last_param_names[1], "limit") == 0 &&
              strcmp(last_param_values[1], "50") == 0,
          "the signed-in DID and the bridge's limit are asked for");
    check(people.kind == PLATINUM_PEOPLE_LISTS && people.count == 2 &&
              strcmp(people.items[0].name, "Friends") == 0 &&
              strcmp(people.items[0].uri,
                     "at://did:plc:me/app.bsky.graph.list/1") == 0 &&
              strcmp(people.items[1].name, "Reading") == 0,
          "list rows carry name and exact URI; a row without one is skipped");
    check(!platinum_people_has_more(&people), "lists do not page");

    respond(WF_OK, 200, "{\"lists\":[]}");
    check(platinum_named_lists_native(&people, client, "x") == WF_OK &&
              people.count == 0 &&
              strcmp(people.status, "Nobody to show.") == 0,
          "no lists says so");

    respond(WF_OK, 200, "{\"lists\":{}}");
    check(platinum_named_lists_native(&people, client, "x") == WF_ERR_PARSE &&
              strcmp(people.status,
                     "The server returned an invalid list.") == 0,
          "a malformed lists body is a parse error");

    respond(WF_ERR_NETWORK, 0, "");
    check(platinum_named_lists_native(&people, client, "x") == WF_ERR_NETWORK &&
              strcmp(people.status, "The lists could not be loaded.") == 0,
          "a failed lists load is reported");

    check(platinum_named_feeds_native(&people, NULL, "x") ==
              WF_ERR_INVALID_ARG, "a NULL client is refused");
    check(platinum_named_lists_native(NULL, client, "x") ==
              WF_ERR_INVALID_ARG, "a NULL list is refused");

    platinum_atproto_client_free(client);
}

static char feed_bodies[3][16384];
static char list_bodies[3][16384];
static const char *feed_pages[3];
static const char *list_pages[3];
static const char *feed_prefs;
static const char *feed_muted_prefs;

static const char *make_feed_page(char *body, int first, int count,
                                  const char *cursor)
{
    char item[512];
    int i;

    strcpy(body, "{\"feed\":[");
    for (i = 0; i < count; i++) {
        sprintf(item,
                "%s{\"post\":{\"uri\":"
                "\"at://did:plc:a/app.bsky.feed.post/%d\","
                "\"cid\":\"c%d\",\"author\":{\"did\":\"did:plc:a\","
                "\"handle\":\"a.test\",\"displayName\":\"A\"},"
                "\"record\":{\"text\":\"post %d\","
                "\"createdAt\":\"2026-01-01T12:00:00Z\"}}}",
                i ? "," : "", first + i, first + i, first + i);
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

static const char *make_list_page(char *body, int first, int count,
                                  const char *cursor)
{
    char item[512];
    int i;

    strcpy(body, "{\"items\":[");
    for (i = 0; i < count; i++) {
        sprintf(item,
                "%s{\"uri\":\"at://did:plc:a/app.bsky.graph.list/x\","
                "\"subject\":{\"did\":\"did:plc:%d\","
                "\"handle\":\"u%d.test\",\"displayName\":\"User %d\"}}",
                i ? "," : "", first + i, first + i, first + i);
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

static void feed_contents_script(int call, int method, const char *url,
                                 const char *body)
{
    (void)method;
    (void)url;
    (void)body;
    fake_status = WF_OK;
    fake_http_status = 200;
    if (call == 1 && feed_muted_prefs != NULL)
        fake_body = feed_muted_prefs;
    else if (call % 2 == 1)
        fake_body = feed_prefs;
    else
        fake_body = feed_pages[(call / 2) - 1];
}

static void list_contents_script(int call, int method, const char *url,
                                 const char *body)
{
    (void)method;
    (void)url;
    (void)body;
    fake_status = WF_OK;
    fake_http_status = 200;
    fake_body = list_pages[call - 1];
}

static void test_named_contents(void)
{
    static platinum_thread thread;
    static platinum_people people;
    platinum_atproto_client *client;
    platinum_atproto_client *blank;
    unsigned short dropped;
    int calls;

    client = words_client();
    if (client == NULL) {
        check(0, "the test client could be built");
        return;
    }
    platinum_thread_init(&thread);
    platinum_people_init(&people);

    /* A saved feed: prefs first (best-effort filtering), then getFeed. */
    feed_prefs = "{\"preferences\":[]}";
    feed_muted_prefs = NULL;
    feed_pages[0] = make_feed_page(feed_bodies[0], 0, 20, "f1");
    feed_pages[1] = make_feed_page(feed_bodies[1], 20, 20, "f2");
    feed_pages[2] = make_feed_page(feed_bodies[2], 40, 20, NULL);
    fake_script = feed_contents_script;
    fake_call_count = 0;
    check(platinum_named_feed_native(&thread, client,
                                     "at://did:plc:me/app.bsky.feed.generator/g",
                                     "Great feed") == WF_OK,
          "a saved feed loads natively");
    check(fake_call_count == 2, "prefs then the feed page");
    check(strcmp(last_nsid, "app.bsky.feed.getFeed") == 0,
          "getFeed is the call");
    check(last_param_count == 2 &&
              strcmp(last_param_names[0], "feed") == 0 &&
              strcmp(last_param_values[0],
                     "at://did:plc:me/app.bsky.feed.generator/g") == 0 &&
              strcmp(last_param_names[1], "limit") == 0 &&
              strcmp(last_param_values[1], "20") == 0,
          "the feed URI and page size are asked for");
    check(thread.native_feed == 1 && thread.count == 20 &&
              strcmp(thread.items[0].post.author, "A") == 0 &&
              strcmp(thread.items[0].post.line1, "post 0") == 0 &&
              thread.items[0].depth == 0 &&
              strcmp(thread.heading, "Great feed") == 0,
          "feed posts fill the list with no depths");
    check(platinum_thread_has_more(&thread), "a feed cursor means more");

    check(platinum_named_feed_more_native(&thread, client, &dropped) == WF_OK &&
              dropped == 0 && thread.count == 40 &&
              strcmp(thread.items[39].post.line1, "post 39") == 0,
          "the next feed page is appended");
    check(last_param_count == 3 &&
              strcmp(last_param_names[2], "cursor") == 0 &&
              strcmp(last_param_values[2], "f1") == 0,
          "the cursor is sent with the next page");

    check(platinum_named_feed_more_native(&thread, client, &dropped) == WF_OK &&
              dropped == 20 && thread.count == 40 &&
              strcmp(thread.items[0].post.line1, "post 20") == 0,
          "the feed cap drops rows from the front");
    check(!platinum_thread_has_more(&thread), "no feed cursor ends the list");

    /* Muted words skip a post, so a page can come back short. */
    feed_muted_prefs =
        "{\"preferences\":[{\"$type\":"
        "\"app.bsky.actor.defs#mutedWordsPref\",\"items\":["
        "{\"value\":\"spoiler\",\"targets\":[\"content\"]}]}]}";
    feed_pages[0] =
        "{\"feed\":["
        "{\"post\":{\"uri\":\"at://did:plc:a/app.bsky.feed.post/1\","
        "\"cid\":\"c1\",\"author\":{\"did\":\"did:plc:a\","
        "\"handle\":\"a.test\"},\"record\":{\"text\":\"a spoiler here\","
        "\"createdAt\":\"2026-01-01T12:00:00Z\"}}},"
        "{\"post\":{\"uri\":\"at://did:plc:a/app.bsky.feed.post/2\","
        "\"cid\":\"c2\",\"author\":{\"did\":\"did:plc:a\","
        "\"handle\":\"a.test\"},\"record\":{\"text\":\"fine\","
        "\"createdAt\":\"2026-01-01T12:00:00Z\"}}}]}";
    fake_call_count = 0;
    check(platinum_named_feed_native(&thread, client,
                                     "at://did:plc:me/app.bsky.feed.generator/g",
                                     "Filtered") == WF_OK &&
              thread.count == 1 &&
              strcmp(thread.items[0].post.line1, "fine") == 0,
          "a muted-word match is left out of a native feed page");
    check(thread.status[0] == '\0', "a short feed page is not an error");
    feed_muted_prefs = NULL;

    feed_pages[0] = "{\"feed\":[]}";
    fake_call_count = 0;
    check(platinum_named_feed_native(&thread, client,
                                     "at://did:plc:me/app.bsky.feed.generator/g",
                                     "x") == WF_OK &&
              thread.count == 0 &&
              strcmp(thread.status, "No posts to show.") == 0,
          "an empty feed says so");

    feed_pages[0] = "{\"feed\":{}}";
    fake_call_count = 0;
    check(platinum_named_feed_native(&thread, client,
                                     "at://did:plc:me/app.bsky.feed.generator/g",
                                     "x") == WF_ERR_PARSE &&
              strcmp(thread.status,
                     "The server returned an invalid list.") == 0,
          "a malformed feed page is a parse error");

    fake_script = NULL;
    respond(WF_ERR_NETWORK, 0, "");
    check(platinum_named_feed_native(&thread, client,
                                     "at://did:plc:me/app.bsky.feed.generator/g",
                                     "x") == WF_ERR_NETWORK &&
              strcmp(thread.status, "The feed could not be loaded.") == 0,
          "a failed feed page is reported");
    check(platinum_named_feed_native(&thread, client, "not an at uri", "x") ==
              WF_ERR_INVALID_ARG, "a feed must be an AT URI");

    blank = platinum_atproto_client_new("https://pds.example");
    check(blank != NULL &&
              platinum_named_feed_native(
                  &thread, blank,
                  "at://did:plc:me/app.bsky.feed.generator/g", "x") ==
                  WF_ERR_AUTH &&
              strcmp(thread.status, "Session expired. Sign in again.") == 0,
          "a client with no session is an auth failure on a feed");
    platinum_atproto_client_free(blank);

    /* A list's members: one getList page at a time. */
    list_pages[0] = make_list_page(list_bodies[0], 0, 20, "l1");
    list_pages[1] = make_list_page(list_bodies[1], 20, 20, "l2");
    list_pages[2] = make_list_page(list_bodies[2], 40, 20, NULL);
    fake_script = list_contents_script;
    fake_call_count = 0;
    check(platinum_named_list_native(
              &people, client, "at://did:plc:a/app.bsky.graph.list/x",
              "Friends") == WF_OK,
          "a list's members load natively");
    check(strcmp(last_nsid, "app.bsky.graph.getList") == 0,
          "getList is the call");
    check(last_param_count == 2 &&
              strcmp(last_param_names[0], "list") == 0 &&
              strcmp(last_param_values[0],
                     "at://did:plc:a/app.bsky.graph.list/x") == 0 &&
              strcmp(last_param_names[1], "limit") == 0 &&
              strcmp(last_param_values[1], "20") == 0,
          "the list URI and page size are asked for");
    check(people.native_list == 1 &&
              people.kind == PLATINUM_PEOPLE_ACCOUNTS && people.count == 20 &&
              strcmp(people.items[0].name, "User 0") == 0 &&
              strcmp(people.items[0].handle, "@u0.test") == 0 &&
              strcmp(people.items[0].uri, "did:plc:0") == 0 &&
              strcmp(people.heading, "Friends") == 0,
          "members fill the People list with their DIDs");
    check(platinum_people_has_more(&people), "a members cursor means more");

    check(platinum_named_list_more_native(&people, client, &dropped) == WF_OK &&
              dropped == 0 && people.count == 40 &&
              strcmp(people.items[39].name, "User 39") == 0,
          "the next members page is appended");
    check(last_param_count == 3 &&
              strcmp(last_param_names[2], "cursor") == 0 &&
              strcmp(last_param_values[2], "l1") == 0,
          "the members cursor is sent with the next page");

    check(platinum_named_list_more_native(&people, client, &dropped) == WF_OK &&
              dropped == 20 && people.count == 40 &&
              strcmp(people.items[0].name, "User 20") == 0,
          "the members cap drops rows from the front");
    check(!platinum_people_has_more(&people), "no members cursor ends the list");
    calls = fake_call_count;
    check(platinum_named_list_more_native(&people, client, &dropped) ==
              WF_ERR_INVALID_ARG &&
              fake_call_count == calls,
          "nothing more makes no request");

    list_pages[0] = "{\"items\":[]}";
    fake_call_count = 0;
    check(platinum_named_list_native(
              &people, client, "at://did:plc:a/app.bsky.graph.list/x",
              "x") == WF_OK &&
              people.count == 0 &&
              strcmp(people.status, "Nobody to show.") == 0,
          "an empty list says so");

    list_pages[0] = "{\"items\":{}}";
    fake_call_count = 0;
    check(platinum_named_list_native(
              &people, client, "at://did:plc:a/app.bsky.graph.list/x",
              "x") == WF_ERR_PARSE &&
              strcmp(people.status,
                     "The server returned an invalid list.") == 0,
          "a malformed members page is a parse error");

    fake_script = NULL;
    respond(WF_ERR_NETWORK, 0, "");
    check(platinum_named_list_native(
              &people, client, "at://did:plc:a/app.bsky.graph.list/x",
              "x") == WF_ERR_NETWORK &&
              strcmp(people.status, "The list could not be loaded.") == 0,
          "a failed members page is reported");
    check(platinum_named_list_native(&people, client, "not an at uri", "x") ==
              WF_ERR_INVALID_ARG, "a list must be an AT URI");

    blank = platinum_atproto_client_new("https://pds.example");
    check(blank != NULL &&
              platinum_named_list_native(
                  &people, blank,
                  "at://did:plc:a/app.bsky.graph.list/x", "x") ==
                  WF_ERR_AUTH &&
              strcmp(people.status, "Session expired. Sign in again.") == 0,
          "a client with no session is an auth failure on members");
    platinum_atproto_client_free(blank);

    check(platinum_named_feed_native(NULL, client,
                                     "at://did:plc:me/app.bsky.feed.generator/g",
                                     "x") == WF_ERR_INVALID_ARG,
          "a NULL thread is refused");
    check(platinum_named_list_native(NULL, client,
                                     "at://did:plc:a/app.bsky.graph.list/x",
                                     "x") == WF_ERR_INVALID_ARG,
          "a NULL people list is refused");

    platinum_atproto_client_free(client);
}

static char follow_bodies[3][16384];
static const char *follow_pages[3];

static const char *make_follow_page(char *body, const char *member, int first,
                                    int count, const char *cursor)
{
    char item[256];
    int i;

    strcpy(body, "{\"");
    strcat(body, member);
    strcat(body, "\":[");
    for (i = 0; i < count; ++i) {
        sprintf(item,
                "%s{\"did\":\"did:plc:%d\",\"handle\":\"u%d.test\","
                "\"displayName\":\"User %d\"}",
                i ? "," : "", first + i, first + i, first + i);
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

static void follows_script(int call, int method, const char *url,
                           const char *body)
{
    (void)method;
    (void)url;
    (void)body;
    fake_status = WF_OK;
    fake_http_status = 200;
    fake_body = follow_pages[call - 1];
}

static void test_follows_native(void)
{
    static platinum_people people;
    platinum_atproto_client *client;
    platinum_atproto_client *blank;
    unsigned short dropped;
    int calls;

    client = words_client();
    if (client == NULL) {
        check(0, "the test client could be built");
        return;
    }
    platinum_people_init(&people);

    follow_pages[0] = make_follow_page(follow_bodies[0], "follows", 0, 20, "f1");
    follow_pages[1] = make_follow_page(follow_bodies[1], "follows", 20, 20, "f2");
    follow_pages[2] = make_follow_page(follow_bodies[2], "follows", 40, 20, NULL);
    fake_script = follows_script;
    fake_call_count = 0;
    check(platinum_people_follows_native(&people, client, "did:plc:me",
                                         "Following") == WF_OK,
          "follows load natively");
    check(strcmp(last_nsid, "app.bsky.graph.getFollows") == 0,
          "getFollows is the call");
    check(last_param_count == 2 &&
              strcmp(last_param_names[0], "actor") == 0 &&
              strcmp(last_param_values[0], "did:plc:me") == 0 &&
              strcmp(last_param_names[1], "limit") == 0 &&
              strcmp(last_param_values[1], "20") == 0,
          "the actor and page size are asked for");
    check(people.native_follows == 1 && people.native_list == 0 &&
              people.count == 20 &&
              strcmp(people.items[0].name, "User 0") == 0 &&
              strcmp(people.items[0].handle, "@u0.test") == 0 &&
              strcmp(people.items[0].uri, "did:plc:0") == 0 &&
              strcmp(people.heading, "Following") == 0,
          "follows fill the People list with their DIDs");
    check(platinum_people_has_more(&people), "a follows cursor means more");

    check(platinum_people_follows_more_native(&people, client, &dropped) ==
              WF_OK && dropped == 0 && people.count == 40 &&
              strcmp(people.items[39].name, "User 39") == 0,
          "the next follows page is appended");
    check(last_param_count == 3 &&
              strcmp(last_param_names[2], "cursor") == 0 &&
              strcmp(last_param_values[2], "f1") == 0,
          "the follows cursor is sent with the next page");

    check(platinum_people_follows_more_native(&people, client, &dropped) ==
              WF_OK && dropped == 20 && people.count == 40 &&
              strcmp(people.items[0].name, "User 20") == 0,
          "the follows cap drops rows from the front");
    check(!platinum_people_has_more(&people), "no follows cursor ends the list");
    calls = fake_call_count;
    check(platinum_people_follows_more_native(&people, client, &dropped) ==
              WF_ERR_INVALID_ARG && fake_call_count == calls,
          "nothing more makes no request");

    follow_pages[0] = make_follow_page(follow_bodies[0], "followers", 0, 20, NULL);
    fake_call_count = 0;
    check(platinum_people_followers_native(&people, client, "me.test",
                                           "Followers") == WF_OK &&
              people.native_follows == 2 && people.count == 20 &&
              strcmp(people.items[0].name, "User 0") == 0 &&
              strcmp(people.items[0].uri, "did:plc:0") == 0,
          "followers load natively from a handle");
    check(strcmp(last_nsid, "app.bsky.graph.getFollowers") == 0,
          "getFollowers is the call");
    check(last_param_count == 2 && strcmp(last_param_names[0], "actor") == 0 &&
              strcmp(last_param_values[0], "me.test") == 0,
          "the handle is the actor");
    check(!platinum_people_has_more(&people), "no followers cursor ends it");

    follow_pages[0] = "{\"follows\":[]}";
    fake_call_count = 0;
    check(platinum_people_follows_native(&people, client, "did:plc:me", "x") ==
              WF_OK && people.count == 0 &&
              strcmp(people.status, "Nobody to show.") == 0,
          "an empty follows page says so");

    follow_pages[0] = "{\"follows\":{}}";
    fake_call_count = 0;
    check(platinum_people_follows_native(&people, client, "did:plc:me", "x") ==
              WF_ERR_PARSE &&
              strcmp(people.status,
                     "The server returned an invalid list.") == 0,
          "a malformed follows page is a parse error");

    fake_script = NULL;
    respond(WF_ERR_NETWORK, 0, "");
    check(platinum_people_follows_native(&people, client, "did:plc:me", "x") ==
              WF_ERR_NETWORK &&
              strcmp(people.status, "The list could not be loaded.") == 0,
          "a failed follows page is reported");
    check(platinum_people_follows_native(&people, client, "not an actor", "x") ==
              WF_ERR_INVALID_ARG, "an actor must be a DID or handle");
    check(platinum_people_follows_native(NULL, client, "did:plc:me", "x") ==
              WF_ERR_INVALID_ARG, "a NULL people list is refused");

    blank = platinum_atproto_client_new("https://pds.example");
    check(blank != NULL &&
              platinum_people_follows_native(&people, blank, "did:plc:me",
                                             "x") == WF_ERR_AUTH &&
              strcmp(people.status, "Session expired. Sign in again.") == 0,
          "a client with no session is an auth failure on follows");
    platinum_atproto_client_free(blank);

    platinum_atproto_client_free(client);
}

int main(void)
{
    test_words();
    test_words_native();
    test_named();
    test_named_native();
    test_named_contents();
    test_follows_native();
    test_list();
    if (failures != 0) {
        printf("test_people: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_people: all %d checks passed\n", checks);
    return 0;
}
