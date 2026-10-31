/*
 * test_muted_words.c -- host-side tests for the native muted-word rules.
 * Pure logic; no Toolbox, no transport. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "muted_words.h"

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

static int matches_text(const char *value, const char *text)
{
    platinum_muted_list list;
    platinum_muted_clear(&list);
    platinum_muted_add(&list, value, 1, 0, 0, NULL);
    return platinum_muted_match(&list, text, NULL, 0, 0);
}

static int matches_tag(const char *value, const char *tag)
{
    platinum_muted_list list;
    const char *tags[1];
    tags[0] = tag;
    platinum_muted_clear(&list);
    platinum_muted_add(&list, value, 0, 1, 0, NULL);
    return platinum_muted_match(&list, "", tags, 1, 0);
}

int main(void)
{
    platinum_muted_list list;
    const char *tags[2];

    /* Whole words, case-insensitively, with punctuation at their ends. */
    check(matches_text("cat", "The cat sat."), "a plain word matches a word");
    check(matches_text("cat", "a CAT!"), "matching is case-insensitive");
    check(matches_text("cat", "(cat)"), "brackets are boundaries");
    check(!matches_text("cat", "concatenate"), "a word does not match inside a word");
    check(!matches_text("cat", "cat's"), "an apostrophe keeps the word going");
    check(!matches_text("cat", "cats"), "a plural is a different word");

    /* Phrases, punctuation and one-character values are substrings. */
    check(matches_text("black cat", "my black cat sleeps"), "a phrase matches as a substring");
    check(matches_text("BLACK CAT", "a black cat"), "a phrase is case-insensitive");
    check(matches_text("c++", "we write c++ here"), "punctuation makes a substring value");
    check(matches_text("x", "a box"), "one character matches as a substring");

    /* Empty values and empty lists never match. */
    platinum_muted_clear(&list);
    check(!platinum_muted_add(&list, "", 1, 0, 0, NULL), "an empty value is refused");
    check(list.count == 0, "a refused value is not stored");
    check(!platinum_muted_match(&list, "anything", NULL, 0, 0), "an empty list matches nothing");
    check(!matches_text("cat", ""), "an empty text does not match a content word");

    /* Targets. */
    check(!matches_tag("spoiler", "spoilers"), "a tag matches exactly, not by prefix");
    check(matches_tag("spoiler", "#spoiler"), "a tag matches with a leading hash");
    check(matches_tag("#spoiler", "spoiler"), "the value's hash is optional");
    check(matches_tag("#Spoiler", "#SPOILER"), "tags are case-insensitive");
    check(!matches_tag("spoiler", "spoiler-two"), "a tag is not a substring match");

    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 1, 0, 0, NULL);
    tags[0] = "spoiler";
    check(!platinum_muted_match(&list, "", tags, 1, 0),
          "a content word does not mute a tag");

    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 0, 1, 0, NULL);
    check(!platinum_muted_match(&list, "a spoiler in the text", NULL, 0, 0),
          "a tag word does not mute the text");

    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 1, 1, 0, NULL);
    check(platinum_muted_match(&list, "", tags, 1, 0),
          "a word with both targets matches the tag");
    check(platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "a word with both targets matches the text");

    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 0, 0, 0, NULL);
    check(platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "neither target means content-only");
    check(!platinum_muted_match(&list, "", tags, 1, 0),
          "neither target does not mute tags");

    /* exclude-following. */
    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 1, 0, 1, NULL);
    check(platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "an exclude-following word mutes a stranger");
    check(!platinum_muted_match(&list, "spoiler", NULL, 0, 1),
          "an exclude-following word skips a followed account");

    /* Expiry, judged at the list's now rather than the Mac clock. */
    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 1, 0, 0, "1970-01-01T00:16:40Z");
    list.now = 1000;
    check(!platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "a word expiring at now is expired");
    list.now = 999;
    check(platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "a word expiring later still applies");
    list.now = 0;
    check(platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "an unknown clock expires nothing");

    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 1, 0, 0, "not a datetime");
    list.now = 2000000000L;
    check(platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "an unparsable expiry is treated as not expired");

    platinum_muted_clear(&list);
    platinum_muted_add(&list, "spoiler", 1, 0, 0, "2026-10-31T00:00:00Z");
    list.now = 1794009600L; /* after 2026-10-31T00:00:00Z */
    check(!platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "a date expiry in the past mutes");
    list.now = 1793000000L; /* before it */
    check(platinum_muted_match(&list, "spoiler", NULL, 0, 0),
          "a date expiry in the future does not mute");

    /* Bounds and nulls. */
    platinum_muted_clear(&list);
    check(!platinum_muted_match(NULL, "spoiler", NULL, 0, 0), "a NULL list matches nothing");
    check(!platinum_muted_match(&list, NULL, NULL, 0, 0), "a NULL text is safe");
    platinum_muted_add(&list, "tag", 0, 1, 0, NULL);
    check(!platinum_muted_match(&list, "", NULL, 0, 0), "a tag word with no tags is safe");

    platinum_muted_clear(&list);
    {
        int i, added = 0;
        char value[16];
        for (i = 0; i < PLATINUM_MUTED_MAX + 1; i++) {
            sprintf(value, "word%d", i);
            added += platinum_muted_add(&list, value, 1, 0, 0, NULL);
        }
        check(added == PLATINUM_MUTED_MAX, "the list stops at its capacity");
        check(list.count == PLATINUM_MUTED_MAX, "the count matches the capacity");
    }

    platinum_muted_clear(&list);
    check(list.count == 0, "clear empties the list");

    if (failures != 0) {
        printf("test_muted_words: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_muted_words: all %d checks passed\n", checks);
    return 0;
}
