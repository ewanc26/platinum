/*
 * The live muted-word routes: the list from the account's getPreferences, and
 * an add or remove written back through putPreferences.
 *
 * This reads the same mutedWordsPref that timeline_wolfram.c applies, so the
 * list a user edits is the list that filters their timeline. A write keeps the
 * other preferences the account has: the typed preferences struct round-trips
 * every known preference and keeps unknown ones verbatim.
 */
#ifndef PLATINUM_NATIVE_MUTED_LIVE_H
#define PLATINUM_NATIVE_MUTED_LIVE_H

#include "account_routes.h"
#include "content_routes.h"

/* platinum_muted_list_fn. ctx is a wf_agent *. At most 100 words, each with the
 * targets it applies to. */
platinum_content_result platinum_muted_list_live(void *ctx, platinum_muted_view *views,
                                                 size_t max, size_t *count);

/* platinum_word_fn. ctx is a wf_agent *; word is already trimmed and valid. Adds
 * for content and tags, for everyone, or removes every case-insensitive match.
 * Both are idempotent and write nothing when the list already agrees. */
platinum_account_result platinum_muted_set_live(void *ctx, const char *word, int on);

#endif
