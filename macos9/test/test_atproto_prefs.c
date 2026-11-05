/*
 * test_atproto_prefs.c -- host-side tests for the native preferences read.
 * JSON parsing and the putPreferences write, the latter over the stubbed
 * Wolfram transport. No Toolbox. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "atproto_prefs.h"

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

static void prefs_save_script(int call, int method, const char *url,
                              const char *body)
{
    (void)call;
    (void)method;
    (void)url;
    (void)body;
    fake_http_status = 200;
    fake_status = WF_OK;
    fake_body = "{}";
}

static platinum_atproto_client *prefs_client(void)
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

static void test_write(void)
{
    static char body[2048];
    platinum_muted_list list;
    platinum_muted_list round;

    platinum_muted_clear(&list);
    check(platinum_muted_add(&list, "spoiler", 1, 0, 0, NULL) == 1,
          "content word added");
    check(platinum_muted_add(&list, "finale", 0, 1, 1,
                             "2030-01-01T00:00:00.000Z") == 1,
          "tag word with actor target and expiry added");
    check(platinum_muted_add(&list, "neither", 0, 0, 0, NULL) == 1,
          "word with no target added");
    check(platinum_muted_add(&list, "quote\"word\\", 1, 0, 0, NULL) == 1,
          "value with a quote and a backslash added");

    check(platinum_prefs_muted_write(&list, body, sizeof(body)) == WF_OK,
          "the list serialises");
    check(strstr(body,
          "\"$type\":\"app.bsky.actor.defs#mutedWordsPref\"") != NULL,
          "the body is one mutedWordsPref item");
    check(strstr(body, "\"value\":\"spoiler\",\"targets\":[\"content\"]")
              != NULL,
          "a content word is written with its target");
    check(strstr(body,
          "\"value\":\"finale\",\"targets\":[\"tag\"],"
          "\"actorTarget\":\"exclude-following\","
          "\"expiresAt\":\"2030-01-01T00:00:00.000Z\"") != NULL,
          "actor target and expiry are written");
    check(strstr(body, "\"value\":\"neither\",\"targets\":[\"content\"]")
              != NULL,
          "a word with no target defaults to content");
    check(strstr(body, "quote\\\"word\\\\") != NULL,
          "quotes and backslashes are escaped");

    check(platinum_prefs_muted_parse(body, &round, 1000) == WF_OK,
          "the written body parses back");
    check(round.count == 4, "the round trip keeps every word");
    check(strcmp(round.words[1].value, "finale") == 0 &&
              round.words[1].tag == 1 && round.words[1].content == 0 &&
              round.words[1].exclude_following == 1 &&
              strcmp(round.words[1].expires_at,
                     "2030-01-01T00:00:00.000Z") == 0,
          "the tag word round trips");
    check(strcmp(round.words[2].value, "neither") == 0 &&
              round.words[2].content == 1 && round.words[2].tag == 0,
          "the content-only default round trips");
    check(strcmp(round.words[3].value, "quote\"word\\") == 0,
          "the escaped value round trips");

    platinum_muted_clear(&list);
    check(platinum_prefs_muted_write(&list, body, sizeof(body)) == WF_OK,
          "an empty list serialises");
    check(strcmp(body,
          "{\"preferences\":[{\"$type\":"
          "\"app.bsky.actor.defs#mutedWordsPref\","
          "\"items\":[]}]}") == 0,
          "an empty list writes an empty item");

    check(platinum_muted_add(&list, "word", 1, 0, 0, NULL) == 1,
          "one word added");
    check(platinum_prefs_muted_write(&list, body, 16) == WF_ERR_ALLOC,
          "a body that does not fit is refused");
    check(platinum_prefs_muted_write(NULL, body, sizeof(body))
              == WF_ERR_INVALID_ARG,
          "a NULL list is refused");
}

static void test_save(void)
{
    platinum_muted_list list;
    platinum_atproto_client *client;

    client = prefs_client();
    if (client == NULL) {
        check(0, "the test client could be built");
        return;
    }

    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 1, 0, 0, NULL);
    fake_script = prefs_save_script;
    check(platinum_prefs_muted_save(client, &list) == WF_OK,
          "the list saves");
    check(strcmp(last_nsid, "app.bsky.actor.putPreferences") == 0,
          "save calls putPreferences");
    check(last_method == 2, "save posts a body");
    check(strstr(last_body, "\"value\":\"spoiler\"") != NULL,
          "save sends the list");
    fake_script = NULL;

    check(platinum_prefs_muted_save(client, NULL) == WF_ERR_INVALID_ARG,
          "a NULL list is refused on save");
    check(platinum_prefs_muted_save(NULL, &list) == WF_ERR_INVALID_ARG,
          "a NULL client is refused on save");

    platinum_atproto_client_free(client);
}

static void test_saved_feeds(void)
{
    static platinum_saved_feeds feeds;
    static char many[8192];
    static char item[160];
    int i;

    check(platinum_prefs_saved_feeds_parse(
              "{\"preferences\":[{\"$type\":"
              "\"app.bsky.actor.defs#savedFeedsPrefV2\",\"items\":["
              "{\"type\":\"timeline\",\"value\":\"following\","
              "\"pinned\":true},"
              "{\"type\":\"feed\","
              "\"value\":\"at://did:plc:a/app.bsky.feed.generator/one\","
              "\"pinned\":true},"
              "{\"type\":\"list\","
              "\"value\":\"at://did:plc:a/app.bsky.graph.list/l\"},"
              "{\"type\":\"feed\","
              "\"value\":\"at://did:plc:a/app.bsky.feed.generator/two\"},"
              "{\"type\":\"feed\",\"value\":\"https://not.at/uri\"},"
              "{\"type\":\"feed\","
              "\"value\":\"at://did:plc:a/app.bsky.graph.list/l\"}"
              "]}]}",
              &feeds) == WF_OK, "v2 saved feeds parse");
    check(feeds.count == 2 &&
              strcmp(feeds.uris[0],
                     "at://did:plc:a/app.bsky.feed.generator/one") == 0 &&
              strcmp(feeds.uris[1],
                     "at://did:plc:a/app.bsky.feed.generator/two") == 0,
          "only feed generators are kept, in order");

    check(platinum_prefs_saved_feeds_parse(
              "{\"preferences\":[{\"$type\":"
              "\"app.bsky.actor.defs#savedFeedsPref\",\"saved\":["
              "\"at://did:plc:a/app.bsky.feed.generator/one\","
              "\"at://did:plc:a/app.bsky.graph.list/l\"],"
              "\"pinned\":["
              "\"at://did:plc:a/app.bsky.feed.generator/two\","
              "\"at://did:plc:a/app.bsky.feed.generator/one\"]}]}",
              &feeds) == WF_OK, "legacy saved feeds parse");
    check(feeds.count == 2 &&
              strcmp(feeds.uris[0],
                     "at://did:plc:a/app.bsky.feed.generator/one") == 0 &&
              strcmp(feeds.uris[1],
                     "at://did:plc:a/app.bsky.feed.generator/two") == 0,
          "saved and pinned merge without repeating a feed");

    check(platinum_prefs_saved_feeds_parse(
              "{\"preferences\":["
              "{\"$type\":\"app.bsky.actor.defs#savedFeedsPrefV2\","
              "\"items\":[]},"
              "{\"$type\":\"app.bsky.actor.defs#savedFeedsPref\","
              "\"saved\":["
              "\"at://did:plc:a/app.bsky.feed.generator/old\"]}]}",
              &feeds) == WF_OK && feeds.count == 0,
          "an empty v2 preference wins over the old one");

    check(platinum_prefs_saved_feeds_parse("{\"preferences\":[]}", &feeds)
              == WF_OK && feeds.count == 0,
          "no saved-feeds preference is an empty list");
    check(platinum_prefs_saved_feeds_parse("{\"preferences\":{}}", &feeds)
              == WF_ERR_PARSE,
          "a non-array preferences body is refused");
    check(platinum_prefs_saved_feeds_parse(NULL, &feeds)
              == WF_ERR_INVALID_ARG, "a NULL body is refused");
    check(platinum_prefs_saved_feeds_parse("{\"preferences\":[]}", NULL)
              == WF_ERR_INVALID_ARG, "a NULL list is refused");

    strcpy(many, "{\"preferences\":[{\"$type\":"
                 "\"app.bsky.actor.defs#savedFeedsPrefV2\",\"items\":[");
    for (i = 0; i < PLATINUM_SAVED_FEED_MAX + 1; i++) {
        sprintf(item,
                "%s{\"type\":\"feed\",\"value\":"
                "\"at://did:plc:a/app.bsky.feed.generator/f%d\"}",
                i ? "," : "", i);
        strcat(many, item);
    }
    strcat(many, "]}]}");
    check(platinum_prefs_saved_feeds_parse(many, &feeds) == WF_OK &&
              feeds.count == PLATINUM_SAVED_FEED_MAX &&
              strcmp(feeds.uris[PLATINUM_SAVED_FEED_MAX - 1],
                     "at://did:plc:a/app.bsky.feed.generator/f24") == 0,
          "at most 25 saved feeds are kept");
}

int main(void)
{
    platinum_muted_list list;

    /* One content word and one tag word, with expiry in the future. */
    check(platinum_prefs_muted_parse(
        "{\"preferences\":["
        "{\"$type\":\"app.bsky.actor.defs#mutedWordsPref\",\"items\":["
        "{\"value\":\"spoiler\",\"targets\":[\"content\"]},"
        "{\"value\":\"finale\",\"targets\":[\"tag\"],"
        "\"actorTarget\":\"exclude-following\","
        "\"expiresAt\":\"2030-01-01T00:00:00.000Z\"}"
        "]}]}",
        &list, 1000) == WF_OK, "prefs with two words parse");
    check(list.count == 2, "both words are kept");
    check(platinum_muted_match(&list, "a spoiler here", NULL, 0, 0),
          "content word mutes the text");
    check(platinum_muted_match(&list, "a spoiler here", NULL, 0, 1),
          "content word without exclude-following still mutes a follow");
    {
        const char *tags[1];
        tags[0] = "finale";
        check(platinum_muted_match(&list, "", tags, 1, 0),
              "tag word mutes the tag");
        check(!platinum_muted_match(&list, "", tags, 1, 1),
              "exclude-following skips a followed author");
    }

    /* Expired words are stored but never match. */
    check(platinum_prefs_muted_parse(
        "{\"preferences\":["
        "{\"$type\":\"app.bsky.actor.defs#mutedWordsPref\",\"items\":["
        "{\"value\":\"old\",\"targets\":[\"content\"],"
        "\"expiresAt\":\"1970-01-01T00:00:01Z\"}"
        "]}]}",
        &list, 1000) == WF_OK, "expired prefs parse");
    check(list.count == 1, "expired word is stored");
    check(!platinum_muted_match(&list, "old news", NULL, 0, 0),
          "expired word does not mute");

    /* Unknown preference types are skipped; neither target is content-only. */
    check(platinum_prefs_muted_parse(
        "{\"preferences\":["
        "{\"$type\":\"app.bsky.actor.defs#adultContentPref\","
        "\"enabled\":true},"
        "{\"$type\":\"app.bsky.actor.defs#mutedWordsPref\",\"items\":["
        "{\"value\":\"both\",\"targets\":[]}"
        "]}]}",
        &list, 0) == WF_OK, "unknown prefs are skipped");
    check(list.count == 1, "default word is kept");
    check(platinum_muted_match(&list, "both here", NULL, 0, 0),
          "neither target means content-only");

    /* Empty values are refused; a missing list is an empty mute set. */
    check(platinum_prefs_muted_parse(
        "{\"preferences\":["
        "{\"$type\":\"app.bsky.actor.defs#mutedWordsPref\",\"items\":["
        "{\"value\":\"\",\"targets\":[\"content\"]}"
        "]}]}",
        &list, 0) == WF_OK, "empty value parses");
    check(list.count == 0, "empty value is refused");
    check(platinum_prefs_muted_parse("{\"preferences\":[]}", &list, 0)
              == WF_OK && list.count == 0,
          "no mutedWordsPref is an empty list");
    check(platinum_prefs_muted_parse("{\"feed\":[]}", &list, 0)
              == WF_ERR_PARSE,
          "a non-preferences body is refused");
    check(platinum_prefs_muted_parse(NULL, &list, 0)
              == WF_ERR_INVALID_ARG,
          "NULL body is refused");

    test_saved_feeds();
    test_write();
    test_save();

    if (failures != 0) {
        printf("test_atproto_prefs: %d of %d checks failed\n", failures,
               checks);
        return 1;
    }
    printf("test_atproto_prefs: all %d checks passed\n", checks);
    return 0;
}
