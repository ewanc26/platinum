#ifndef PLATINUM_WORDS_NATIVE_H
#define PLATINUM_WORDS_NATIVE_H

/*
 * words_native.h -- the muted-words screen's native path.
 *
 * Kept apart from people_feed.c so the bridge path and its tests do not pull
 * in the session stack. The screen reads the list with getPreferences and
 * writes it back with putPreferences: load, change, save. `now` is 0 here, so
 * expired words are still listed and still round-trip on a save; expiry only
 * matters when the timeline filters.
 */

#include "atproto_client.h"
#include "muted_words.h"
#include "people.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fill the People window's words list from getPreferences. The window itself
 * is the caller's business; only the model rows and status change. */
wf_status platinum_words_show_native(platinum_people *people,
                                     platinum_atproto_client *client);

/* Add `word` (content-only, the lexicon default) through load-add-save.
 * WF_ERR_STATE when the list is full, WF_ERR_INVALID_ARG for an empty word. */
wf_status platinum_words_add_native(platinum_atproto_client *client,
                                    const char *word);

/* Remove the word `value`, exactly as the list shows it, through
 * load-remove-save. WF_ERR_NOT_FOUND when it is not in the list. */
wf_status platinum_words_remove_native(platinum_atproto_client *client,
                                       const char *value);

#ifdef __cplusplus
}
#endif

#endif
