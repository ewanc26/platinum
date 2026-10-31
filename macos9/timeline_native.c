/*
 * timeline_native.c -- the timeline's native page fetch.
 *
 * Kept apart from timeline.c so the bridge path, the UI tests and the offline
 * fixture do not pull in the session and bootstrap stack just by compiling the
 * row model. It writes the same rows, uses the same commit rules and speaks
 * through atproto_feed, which owns the AppView method name.
 */

#include "timeline.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "atproto_url.h"

#include "atproto_feed.h"

static void timeline_native_set_status(platinum_timeline *timeline,
                                       const char *status)
{
    long length;

    if (timeline == NULL)
        return;

    timeline->status[0] = '\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_TIMELINE_STATUS_MAX)
        length = PLATINUM_TIMELINE_STATUS_MAX;

    memcpy(timeline->status, status, (size_t)length);
    timeline->status[length] = '\0';
}

/*
 * One native page, with the same commit rules as the bridge page: the response
 * is validated before any row is dropped, and a failure leaves the loaded rows
 * and cursor alone.
 */
static wf_status timeline_fetch_native(platinum_timeline *timeline,
                                       platinum_atproto_client *client,
                                       int append,
                                       unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json feed;
    platinum_json item;
    platinum_json post_view;
    long index;
    long available;
    long parsed;
    long drop;
    char cursor[PLATINUM_TIMELINE_CURSOR_MAX + 1];
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;

    timeline->loading = 1;
    timeline_native_set_status(timeline, append ? "Loading older posts..."
                                         : "Loading timeline...");
    memset(&response, 0, sizeof(response));

    status = platinum_atproto_timeline_page(
        client, append ? timeline->cursor : NULL, PLATINUM_TIMELINE_PAGE,
        &response);
    if (status != WF_OK) {
        timeline->loading = 0;
        if (status == WF_ERR_AUTH)
            timeline_native_set_status(timeline,
                                "Session expired. Sign in again.");
        else
            timeline_native_set_status(timeline, append
                                    ? "Could not load older posts."
                                    : "Timeline refresh failed.");
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "feed", &feed) != WF_OK ||
        platinum_json_count(feed, &available) != WF_OK) {
        timeline->loading = 0;
        timeline_native_set_status(timeline,
                            "The server returned an invalid timeline.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    /* Only now that the page is known good is it safe to replace the rows: a
     * failed refresh must not blank a timeline the reader is looking at. */
    if (!append) {
        timeline->count = 0;
        timeline->cursor[0] = '\0';
        timeline->capped = 0;
    }

    parsed = (available > PLATINUM_TIMELINE_PAGE) ? PLATINUM_TIMELINE_PAGE
                                                  : available;

    drop = (long)timeline->count + parsed - PLATINUM_TIMELINE_MAX_POSTS;
    if (drop > (long)timeline->count)
        drop = (long)timeline->count;
    if (drop > 0) {
        memmove(&timeline->posts[0], &timeline->posts[drop],
                (size_t)((long)timeline->count - drop) *
                    sizeof(timeline->posts[0]));
        timeline->count = (unsigned short)((long)timeline->count - drop);
        timeline->capped = 1;
        if (dropped != NULL)
            *dropped = (unsigned short)drop;
    }

    for (index = 0; index < parsed &&
                    timeline->count < PLATINUM_TIMELINE_MAX_POSTS; ++index) {
        if (platinum_json_element(feed, index, &item) != WF_OK)
            continue;
        if (platinum_json_member(item, "post", &post_view) != WF_OK)
            continue;
        if (platinum_timeline_parse_view(&timeline->posts[timeline->count],
                                         post_view))
            ++timeline->count;
    }

    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) == WF_OK)
        strcpy(timeline->cursor, cursor);
    else
        timeline->cursor[0] = '\0';

    wf_response_free(&response);

    timeline->loading = 0;
    if (timeline->count == 0)
        timeline_native_set_status(timeline, "The timeline is empty.");
    else
        timeline_native_set_status(timeline, NULL);

    return WF_OK;
}

wf_status platinum_timeline_refresh_native(platinum_timeline *timeline,
                                           platinum_atproto_client *client)
{
    if (timeline == NULL || client == NULL)
        return WF_ERR_INVALID_ARG;
    return timeline_fetch_native(timeline, client, 0, NULL);
}

wf_status platinum_timeline_load_older_native(
    platinum_timeline *timeline, platinum_atproto_client *client,
    unsigned short *dropped)
{
    if (dropped != NULL)
        *dropped = 0;
    if (timeline == NULL || client == NULL ||
        !platinum_timeline_has_older(timeline))
        return WF_ERR_INVALID_ARG;
    return timeline_fetch_native(timeline, client, 1, dropped);
}


/*
 * A viewer interaction must belong to the signed-in repo and expected
 * collection. This prevents a stale or malformed view from deleting another
 * account's record or a different kind of record.
 */
static int timeline_native_interaction_uri(const char *uri, const char *did,
                                           const char *collection)
{
    char prefix[320];
    size_t prefix_length;

    if (uri == NULL || !platinum_atproto_valid_did(did) ||
        !platinum_atproto_valid_at_uri(uri))
        return 0;

    sprintf(prefix, "at://%s/%s/", did, collection);
    prefix_length = strlen(prefix);
    return strncmp(uri, prefix, prefix_length) == 0 &&
           uri[prefix_length] != '\0';
}

wf_status platinum_timeline_set_engagement_native(
    platinum_timeline *timeline, platinum_atproto_client *client,
    unsigned short index, int repost, int on)
{
    platinum_post_preview *post;
    const platinum_atproto_session *session;
    const char *interaction_uri;
    const char *collection;
    char created_uri[513];
    wf_response response;
    platinum_json root;
    wf_status status;
    int current_on;

    if (timeline == NULL || client == NULL || index >= timeline->count ||
        (repost != 0 && repost != 1) || (on != 0 && on != 1))
        return WF_ERR_INVALID_ARG;

    session = platinum_atproto_client_session(client);
    if (session == NULL || !platinum_atproto_valid_did(session->did))
        return WF_ERR_AUTH;

    post = &timeline->posts[index];
    current_on = repost ? post->reposted : post->liked;
    interaction_uri = repost ? post->repost_uri : post->like_uri;
    collection = repost ? "app.bsky.feed.repost" : "app.bsky.feed.like";

    if (current_on == on) {
        timeline_native_set_status(timeline, NULL);
        return WF_OK;
    }

    if (on && ((repost && post->repost_count == LONG_MAX) ||
               (!repost && post->like_count == LONG_MAX)))
        return WF_ERR_STATE;

    if (!on && !timeline_native_interaction_uri(interaction_uri, session->did,
                                                 collection)) {
        timeline_native_set_status(
            timeline, repost
                ? "The repost record is not available to undo."
                : "The like record is not available to undo.");
        return WF_ERR_NOT_FOUND;
    }

    memset(&response, 0, sizeof(response));
    status = platinum_atproto_feed_engagement(
        client, post->uri, post->cid, repost, on,
        on ? NULL : interaction_uri, &response);
    if (status != WF_OK) {
        if (status == WF_ERR_AUTH)
            timeline_native_set_status(timeline,
                                        "Session expired. Sign in again.");
        else if (status == WF_ERR_NOT_FOUND)
            timeline_native_set_status(timeline,
                                        "That interaction no longer exists.");
        else
            timeline_native_set_status(timeline,
                repost ? "The repost did not go through."
                       : "The like did not go through.");
        wf_response_free(&response);
        return status;
    }

    if (on) {
        if (platinum_json_open(&root,
                               response.body != NULL ? response.body : "") !=
                WF_OK ||
            platinum_json_string(root, "uri", created_uri,
                                 sizeof(created_uri)) != WF_OK ||
            !timeline_native_interaction_uri(created_uri, session->did,
                                             collection)) {
            timeline_native_set_status(
                timeline, "The server returned an invalid interaction.");
            wf_response_free(&response);
            return WF_ERR_PARSE;
        }
    }

    wf_response_free(&response);

    if (repost) {
        post->reposted = on;
        if (on) {
            strcpy(post->repost_uri, created_uri);
            post->repost_count++;
        } else {
            post->repost_uri[0] = '\0';
            if (post->repost_count > 0)
                post->repost_count--;
        }
    } else {
        post->liked = on;
        if (on) {
            strcpy(post->like_uri, created_uri);
            post->like_count++;
        } else {
            post->like_uri[0] = '\0';
            if (post->like_count > 0)
                post->like_count--;
        }
    }

    timeline_native_set_status(timeline, NULL);
    return WF_OK;
}
