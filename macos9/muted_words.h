#ifndef PLATINUM_MUTED_WORDS_H
#define PLATINUM_MUTED_WORDS_H

/*
 * Muted-word matching for the account's `mutedWordsPref`.
 *
 * Wolfram owns these rules (wolfram/muted_words.h and moderation.h), but its
 * headers need C99 (`bool`, `int64_t`, `stdbool.h`), so the Mac target cannot
 * include them. This is a C89 port of the same rules, kept in step with the
 * header documented there:
 *
 *  - Matching is case-insensitive. A single word matches whole words, ignoring
 *    punctuation at their ends: "cat" matches "cat." and "(cat)", but not
 *    "cat's" (an apostrophe is part of a word) or "concatenate".
 *  - A phrase, a multi-character value with punctuation in it, and a
 *    one-character value match as a substring.
 *  - An empty value never matches.
 *  - `content` targets match the post text; `tag` targets match a hashtag
 *    exactly, case-insensitively, with or without a leading '#' on either side.
 *  - `exclude-following` words do not apply to accounts the viewer follows.
 *  - A word whose `expiresAt` has passed at `now` does not apply. An
 *    `expiresAt` that does not parse is treated as not expired, the safe
 *    direction for a mute, and `now` of 0 (clock unknown) expires nothing.
 *
 * No Toolbox and no allocation, so the rules are host-tested. Fetching lives
 * in atproto_prefs.h: the native timeline loads getPreferences there and
 * filters with this matcher. The muted-words screen still reads through the
 * bridge.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_MUTED_MAX 48
#define PLATINUM_MUTED_VALUE_MAX 64
#define PLATINUM_MUTED_EXPIRES_MAX 40

typedef struct platinum_muted_entry {
    char value[PLATINUM_MUTED_VALUE_MAX];
    int content;           /* applies to the post text */
    int tag;               /* applies to hashtags */
    int exclude_following; /* does not apply to followed accounts */
    char expires_at[PLATINUM_MUTED_EXPIRES_MAX]; /* lexicon datetime, "" none */
} platinum_muted_entry;

typedef struct platinum_muted_list {
    platinum_muted_entry words[PLATINUM_MUTED_MAX];
    int count;
    long now; /* Unix seconds when the list was loaded; 0 = clock unknown */
} platinum_muted_list;

void platinum_muted_clear(platinum_muted_list *list);

/* Add one word. An empty value, a full list or a NULL list is refused (0). A
 * word with neither target is content-only, which is the server's default.
 * `expires_at` may be NULL. Values too long for the entry are truncated. */
int platinum_muted_add(platinum_muted_list *list, const char *value,
                       int content, int tag, int exclude_following,
                       const char *expires_at);

/* True if any word mutes a post with this `text` and these `tags` (with or
 * without '#') at the list's `now`. `text` and `tags` may be NULL,
 * `tag_count` is then ignored. */
int platinum_muted_match(const platinum_muted_list *list, const char *text,
                         const char *const *tags, int tag_count,
                         int author_followed);

#ifdef __cplusplus
}
#endif

#endif /* PLATINUM_MUTED_WORDS_H */
