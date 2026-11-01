/*
 * test_atproto_prefs.c -- host-side tests for the native preferences read.
 * Pure JSON parsing; no transport, no Toolbox. Not Classic Mac OS 9
 * validation.
 */

#include <stdio.h>
#include <string.h>

#include "atproto_prefs.h"

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

    if (failures != 0) {
        printf("test_atproto_prefs: %d of %d checks failed\n", failures,
               checks);
        return 1;
    }
    printf("test_atproto_prefs: all %d checks passed\n", checks);
    return 0;
}
