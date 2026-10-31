#ifndef PLATINUM_ATPROTO_FEED_H
#define PLATINUM_ATPROTO_FEED_H

/*
 * atproto_feed.h -- native feed requests.
 *
 * This is the thin XRPC layer between the timeline view model and the session
 * client. It owns the AppView method names so no UI module has to, and it
 * returns the raw response: turning a postView into a row belongs to
 * `timeline.c`, which is also what the bridge path uses.
 */

#include <stddef.h>

#include "atproto_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Longest cursor this client will send. The AppView cursors in use are shorter;
 * a longer one is refused rather than truncated, because asking for the wrong
 * page is worse than ending paging. */
#define PLATINUM_ATPROTO_CURSOR_MAX 255

/*
 * Fetch one page of app.bsky.feed.getTimeline.
 *
 * `cursor` is NULL or empty for the first page. `limit` is 1 to 100; the caller
 * normally uses PLATINUM_TIMELINE_PAGE. On WF_OK `out` owns the body and must
 * be freed with wf_response_free. The response shape is
 * `{"cursor":"...","feed":[{"post":{...}}]}`.
 */
wf_status platinum_atproto_timeline_page(platinum_atproto_client *client,
                                        const char *cursor, size_t limit,
                                        wf_response *out);

#ifdef __cplusplus
}
#endif

#endif
