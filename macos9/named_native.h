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
#include "thread.h"

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

/* Open a saved feed's posts through app.bsky.feed.getFeed, replacing the
 * Thread window's list with the first 20-row page. Muted words are applied
 * best-effort, as on the timeline, so a page can come back short. */
wf_status platinum_named_feed_native(platinum_thread *thread,
                                     platinum_atproto_client *client,
                                     const char *uri, const char *heading);

/* Open an account's own posts through app.bsky.feed.getAuthorFeed. The actor
 * is a DID or handle; the bridge's posts_no_replies filter is kept. */
wf_status platinum_named_author_native(platinum_thread *thread,
                                       platinum_atproto_client *client,
                                       const char *actor, const char *heading);

/* The next page of that feed. WF_ERR_INVALID_ARG when there is no more. */
wf_status platinum_named_feed_more_native(platinum_thread *thread,
                                          platinum_atproto_client *client,
                                          unsigned short *dropped);

/* Open a list's members through app.bsky.graph.getList, replacing the People
 * window's rows with the first 20-row page. */
wf_status platinum_named_list_native(platinum_people *people,
                                     platinum_atproto_client *client,
                                     const char *uri, const char *heading);

/* The next page of members. WF_ERR_INVALID_ARG when there is no more. */
wf_status platinum_named_list_more_native(platinum_people *people,
                                          platinum_atproto_client *client,
                                          unsigned short *dropped);

#ifdef __cplusplus
}
#endif

#endif
