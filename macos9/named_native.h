#ifndef PLATINUM_NAMED_NATIVE_H
#define PLATINUM_NAMED_NATIVE_H

/*
 * named_native.h -- Saved Feeds and My Lists fetched by the Mac itself.
 *
 * Both screens are the People window with named rows. Saved feeds come from
 * app.bsky.actor.getPreferences (savedFeedsPrefV2, or the older savedFeedsPref)
 * and are named with app.bsky.feed.getFeedGenerators; My Lists comes from
 * app.bsky.graph.getLists for the signed-in account. Neither list pages; the
 * bridge routes did not either. application_views.c chooses this path when
 * there is a native session and falls back to the bridge otherwise.
 */

#include "atproto_client.h"
#include "people.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fill the People window with the account's saved custom feeds, named by the
 * AppView. A failure leaves the list empty and says why in the status line. */
wf_status platinum_named_feeds_native(platinum_people *people,
                                      platinum_atproto_client *client,
                                      const char *heading);

/* Fill the People window with the account's own lists. Same failure rule. */
wf_status platinum_named_lists_native(platinum_people *people,
                                      platinum_atproto_client *client,
                                      const char *heading);

#ifdef __cplusplus
}
#endif

#endif
