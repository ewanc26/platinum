#ifndef PLATINUM_ATPROTO_PREFS_H
#define PLATINUM_ATPROTO_PREFS_H

/*
 * atproto_prefs.h -- native account preferences.
 *
 * Reads app.bsky.actor.getPreferences and keeps the parts the native screens
 * need. Today that is only the muted-words list: the AppView never filters
 * for the client, so the timeline applies the words itself with
 * muted_words.h, the same C89 rules the bridge used server-side.
 *
 * Fetching is best-effort: a preferences failure leaves an empty list, so the
 * page is shown unfiltered rather than not at all. Parsing is pure, so it is
 * host-tested without transport.
 */

#include <stddef.h>

#include "atproto_client.h"
#include "muted_words.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse a getPreferences body into `list`.
 *
 * The body is `{"preferences":[...]}`. Only entries whose `$type` is
 * `app.bsky.actor.defs#mutedWordsPref` are read; unknown entries are skipped.
 * Each word needs `value` and `targets` (`content`, `tag`); `actorTarget`
 * `exclude-following` and `expiresAt` are optional. A word that does not fit
 * is dropped, the rest are kept. `now` is Unix seconds for expiry (0 expires
 * nothing). Returns WF_OK even when no mutedWordsPref is present (an empty
 * list); WF_ERR_PARSE when the body is not a preferences object.
 */
wf_status platinum_prefs_muted_parse(const char *body,
                                     platinum_muted_list *list,
                                     long now);

/*
 * Fetch getPreferences and parse it into `list` (with `now` for expiry).
 * On transport failure `list` is empty and the status is returned, so the
 * caller can show the page unfiltered.
 */
wf_status platinum_prefs_muted_load(platinum_atproto_client *client,
                                    platinum_muted_list *list,
                                    long now);

#ifdef __cplusplus
}
#endif

#endif
