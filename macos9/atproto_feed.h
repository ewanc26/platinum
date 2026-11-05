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

/*
 * Fetch one page of app.bsky.notification.listNotifications.
 *
 * `cursor` is NULL or empty for the first page; `limit` is 1 to 100. The
 * response shape is `{"cursor":"...","notifications":[{"uri","cid",
 * "author","reason","isRead","indexedAt",...}]}`.
 */
wf_status platinum_atproto_notifications_page(
    platinum_atproto_client *client, const char *cursor, size_t limit,
    wf_response *out);

/*
 * Fetch the signed-in account's own lists: app.bsky.graph.getLists.
 *
 * `actor` is a DID or handle and `limit` is 1 to 100. The response shape is
 * `{"lists":[{"uri","name",...}],"cursor":"..."}`. The route this replaces
 * did not page, so the screen asks once.
 */
wf_status platinum_atproto_lists(platinum_atproto_client *client,
                                 const char *actor, size_t limit,
                                 wf_response *out);

/*
 * Name saved feed generators: app.bsky.feed.getFeedGenerators, with one
 * repeated `feeds` parameter per URI. The session layer accepts at most
 * PLATINUM_ATPROTO_PARAMS_MAX parameters, so a longer saved list is asked for
 * in batches. The response shape is `{"feeds":[{"uri","displayName",...}]}`
 * in the order asked.
 */
wf_status platinum_atproto_feed_generators(platinum_atproto_client *client,
                                          const char *const *uris,
                                          size_t count, wf_response *out);

/*
 * One page of a saved feed: app.bsky.feed.getFeed with the generator URI.
 * `cursor` is NULL or empty for the first page; `limit` is 1 to 100. The
 * response shape is `{"feed":[{"post":{...}}],"cursor":"..."}`.
 */
wf_status platinum_atproto_feed_page(platinum_atproto_client *client,
                                     const char *feed_uri, const char *cursor,
                                     size_t limit, wf_response *out);

/*
 * One page of a list's members: app.bsky.graph.getList with the list URI.
 * The response shape is `{"items":[{"subject":{...}}],"cursor":"..."}`.
 */
wf_status platinum_atproto_list_page(platinum_atproto_client *client,
                                     const char *list_uri, const char *cursor,
                                     size_t limit, wf_response *out);

/*
 * One page of an account's follows or followers:
 * app.bsky.graph.getFollows / getFollowers. `actor` is a DID or handle.
 * The response shapes are `{"follows":[profileView...],"cursor":"..."}`
 * and `{"followers":[profileView...],"cursor":"..."}`.
 */
wf_status platinum_atproto_follows_page(platinum_atproto_client *client,
                                       const char *actor, const char *cursor,
                                       size_t limit, wf_response *out);
wf_status platinum_atproto_followers_page(platinum_atproto_client *client,
                                         const char *actor, const char *cursor,
                                         size_t limit, wf_response *out);

/* One page of an account's own posts: app.bsky.feed.getAuthorFeed, with the
 * bridge's `posts_no_replies` filter. The response shape is
 * `{"feed":[{"post":{...}}],"cursor":"..."}`. */
wf_status platinum_atproto_author_page(platinum_atproto_client *client,
                                       const char *actor, const char *cursor,
                                       size_t limit, wf_response *out);

/* One page of who liked or reposted a post: app.bsky.feed.getLikes and
 * getRepostedBy. The shapes are `{"likes":[{"actor":profileView}],
 * "cursor":"..."}` and `{"repostedBy":[profileView],"cursor":"..."}`. */
wf_status platinum_atproto_likes_page(platinum_atproto_client *client,
                                      const char *post_uri,
                                      const char *cursor, size_t limit,
                                      wf_response *out);
wf_status platinum_atproto_reposts_page(platinum_atproto_client *client,
                                        const char *post_uri,
                                        const char *cursor, size_t limit,
                                        wf_response *out);

/*
 * Create or delete the signed-in user's like or repost for a post.
 *
 * record_uri is required when deleting and must be the URI of the signed-in
 * account's record in the expected collection. On create, the raw
 * createRecord response is returned in out so the caller can retain its URI.
 */
wf_status platinum_atproto_feed_engagement(
    platinum_atproto_client *client, const char *post_uri,
    const char *post_cid, int repost, int on, const char *record_uri,
    wf_response *out);

/* Mark notifications seen through app.bsky.notification.updateSeen. `seen_at`
 * is an RFC 3339 timestamp as the service sent it, never the Mac clock. */
wf_status platinum_atproto_notification_seen(
    platinum_atproto_client *client, const char *seen_at, wf_response *out);

#ifdef __cplusplus
}
#endif

#endif
