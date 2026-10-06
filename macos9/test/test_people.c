/*
 * test_people.c -- host-side tests for the account lists (followers, following,
 * likes, reposts). Real people_feed.c and bridge client over the stubbed
 * Wolfram transport. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "people.h"
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

int main(void)
{
    test_words();
    test_named();
    test_list();
    if (failures != 0) {
        printf("test_people: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_people: all %d checks passed\n", checks);
    return 0;
}
