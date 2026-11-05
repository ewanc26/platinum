#include "atproto_feed.h"

#include <stdio.h>
#include <string.h>

#include "json_min.h"
#include "atproto_url.h"

#include <time.h>

wf_status platinum_atproto_timeline_page(platinum_atproto_client *client,
                                        const char *cursor, size_t limit,
                                        wf_response *out)
{
    const char *names[2];
    const char *values[2];
    char limit_text[8];
    size_t count;

    if (client == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;
    if (limit < 1 || limit > 100)
        return WF_ERR_INVALID_ARG;
    if (cursor != NULL && strlen(cursor) > PLATINUM_ATPROTO_CURSOR_MAX)
        return WF_ERR_INVALID_ARG;

    sprintf(limit_text, "%lu", (unsigned long)limit);

    names[0] = "limit";
    values[0] = limit_text;
    count = 1;

    if (cursor != NULL && cursor[0] != '\0') {
        names[1] = "cursor";
        values[1] = cursor;
        count = 2;
    }

    return platinum_atproto_client_get(client, "app.bsky.feed.getTimeline",
                                       names, values, count, out);
}

wf_status platinum_atproto_notifications_page(
    platinum_atproto_client *client, const char *cursor, size_t limit,
    wf_response *out)
{
    const char *names[2];
    const char *values[2];
    char limit_text[8];
    size_t count;

    if (client == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;
    if (limit < 1 || limit > 100)
        return WF_ERR_INVALID_ARG;
    if (cursor != NULL && strlen(cursor) > PLATINUM_ATPROTO_CURSOR_MAX)
        return WF_ERR_INVALID_ARG;

    sprintf(limit_text, "%lu", (unsigned long)limit);

    names[0] = "limit";
    values[0] = limit_text;
    count = 1;

    if (cursor != NULL && cursor[0] != '\0') {
        names[1] = "cursor";
        values[1] = cursor;
        count = 2;
    }

    return platinum_atproto_client_get(client,
                                       "app.bsky.notification.listNotifications",
                                       names, values, count, out);
}

wf_status platinum_atproto_lists(platinum_atproto_client *client,
                                 const char *actor, size_t limit,
                                 wf_response *out)
{
    const char *names[2];
    const char *values[2];
    char limit_text[8];

    if (client == NULL || out == NULL || actor == NULL || actor[0] == '\0')
        return WF_ERR_INVALID_ARG;
    if (limit < 1 || limit > 100)
        return WF_ERR_INVALID_ARG;
    if (strlen(actor) > PLATINUM_ATPROTO_DID_MAX)
        return WF_ERR_INVALID_ARG;

    sprintf(limit_text, "%lu", (unsigned long)limit);
    names[0] = "actor";
    values[0] = actor;
    names[1] = "limit";
    values[1] = limit_text;

    return platinum_atproto_client_get(client, "app.bsky.graph.getLists",
                                       names, values, 2, out);
}

/* getFollows and getFollowers take an actor, a limit and an optional cursor,
 * so one builder serves both. */
static wf_status graph_actor_page(platinum_atproto_client *client,
                                  const char *nsid, const char *actor,
                                  const char *filter, const char *cursor,
                                  size_t limit, wf_response *out)
{
    const char *names[4];
    const char *values[4];
    char limit_text[8];
    size_t count;

    if (client == NULL || out == NULL || nsid == NULL || actor == NULL ||
        actor[0] == '\0')
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_did(actor) &&
        !platinum_atproto_valid_handle(actor))
        return WF_ERR_INVALID_ARG;
    if (limit < 1 || limit > 100)
        return WF_ERR_INVALID_ARG;
    if (cursor != NULL && strlen(cursor) > PLATINUM_ATPROTO_CURSOR_MAX)
        return WF_ERR_INVALID_ARG;

    sprintf(limit_text, "%lu", (unsigned long)limit);
    names[0] = "actor";
    values[0] = actor;
    names[1] = "limit";
    values[1] = limit_text;
    count = 2;

    if (filter != NULL && filter[0] != '\0') {
        names[count] = "filter";
        values[count] = filter;
        count++;
    }
    if (cursor != NULL && cursor[0] != '\0') {
        names[count] = "cursor";
        values[count] = cursor;
        count++;
    }

    return platinum_atproto_client_get(client, nsid, names, values, count, out);
}

wf_status platinum_atproto_follows_page(platinum_atproto_client *client,
                                       const char *actor, const char *cursor,
                                       size_t limit, wf_response *out)
{
    return graph_actor_page(client, "app.bsky.graph.getFollows", actor, NULL,
                            cursor, limit, out);
}

wf_status platinum_atproto_followers_page(platinum_atproto_client *client,
                                         const char *actor, const char *cursor,
                                         size_t limit, wf_response *out)
{
    return graph_actor_page(client, "app.bsky.graph.getFollowers", actor, NULL,
                            cursor, limit, out);
}

wf_status platinum_atproto_author_page(platinum_atproto_client *client,
                                       const char *actor, const char *cursor,
                                       size_t limit, wf_response *out)
{
    /* The bridge asked for posts without replies; keep that on the native
     * path so the profile tab does not change shape. */
    return graph_actor_page(client, "app.bsky.feed.getAuthorFeed", actor,
                            "posts_no_replies", cursor, limit, out);
}

/* getFeed and getList take one AT URI, a limit and an optional cursor, so one
 * builder serves both and only the NSID and parameter name differ. */
static wf_status feed_uri_page(platinum_atproto_client *client,
                               const char *nsid, const char *name,
                               const char *uri, const char *cursor,
                               size_t limit, wf_response *out)
{
    const char *names[3];
    const char *values[3];
    char limit_text[8];
    size_t count;

    if (client == NULL || out == NULL || nsid == NULL || name == NULL ||
        uri == NULL || uri[0] == '\0')
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_at_uri(uri))
        return WF_ERR_INVALID_ARG;
    if (limit < 1 || limit > 100)
        return WF_ERR_INVALID_ARG;
    if (cursor != NULL && strlen(cursor) > PLATINUM_ATPROTO_CURSOR_MAX)
        return WF_ERR_INVALID_ARG;

    sprintf(limit_text, "%lu", (unsigned long)limit);
    names[0] = name;
    values[0] = uri;
    names[1] = "limit";
    values[1] = limit_text;
    count = 2;

    if (cursor != NULL && cursor[0] != '\0') {
        names[2] = "cursor";
        values[2] = cursor;
        count = 3;
    }

    return platinum_atproto_client_get(client, nsid, names, values, count, out);
}

wf_status platinum_atproto_feed_page(platinum_atproto_client *client,
                                     const char *feed_uri, const char *cursor,
                                     size_t limit, wf_response *out)
{
    return feed_uri_page(client, "app.bsky.feed.getFeed", "feed", feed_uri,
                         cursor, limit, out);
}

wf_status platinum_atproto_list_page(platinum_atproto_client *client,
                                     const char *list_uri, const char *cursor,
                                     size_t limit, wf_response *out)
{
    return feed_uri_page(client, "app.bsky.graph.getList", "list", list_uri,
                         cursor, limit, out);
}

wf_status platinum_atproto_feed_generators(platinum_atproto_client *client,
                                          const char *const *uris,
                                          size_t count, wf_response *out)
{
    const char *names[PLATINUM_ATPROTO_PARAMS_MAX];
    size_t index;

    if (client == NULL || out == NULL || uris == NULL || count == 0)
        return WF_ERR_INVALID_ARG;
    if (count > PLATINUM_ATPROTO_PARAMS_MAX)
        return WF_ERR_INVALID_ARG;
    for (index = 0; index < count; index++) {
        if (uris[index] == NULL ||
            !platinum_atproto_valid_at_uri(uris[index]))
            return WF_ERR_INVALID_ARG;
        names[index] = "feeds";
    }

    return platinum_atproto_client_get(client, "app.bsky.feed.getFeedGenerators",
                                       names, uris, count, out);
}

static int feed_valid_cid(const char *cid)
{
    const char *scan;
    size_t length;

    if (cid == NULL)
        return 0;
    length = strlen(cid);
    if (length == 0 || length > 255)
        return 0;
    for (scan = cid; *scan != '\0'; scan++) {
        if (!((*scan >= 'a' && *scan <= 'z') ||
              (*scan >= 'A' && *scan <= 'Z') ||
              (*scan >= '0' && *scan <= '9')))
            return 0;
    }
    return 1;
}

/*
 * Create a new like/repost record or delete the exact record URI returned by
 * AppView. The caller retains the createRecord URI so a later undo can issue
 * deleteRecord without guessing at a record key.
 */
wf_status platinum_atproto_feed_engagement(
    platinum_atproto_client *client, const char *post_uri,
    const char *post_cid, int repost, int on, const char *record_uri,
    wf_response *out)
{
    const platinum_atproto_session *session;
    const char *collection;
    char prefix[320];
    char body[1536];
    char created_at[32];
    const char *rkey;
    time_t now;
    struct tm *utc;
    size_t prefix_length;

    if (client == NULL || out == NULL ||
        (repost != 0 && repost != 1) || (on != 0 && on != 1))
        return WF_ERR_INVALID_ARG;

    session = platinum_atproto_client_session(client);
    if (session == NULL || !platinum_atproto_valid_did(session->did) ||
        !platinum_atproto_valid_at_uri(post_uri) ||
        !feed_valid_cid(post_cid))
        return WF_ERR_INVALID_ARG;

    collection = repost ? "app.bsky.feed.repost" : "app.bsky.feed.like";

    if (on) {
        now = time(NULL);
        if (now == (time_t)-1)
            return WF_ERR_STATE;
        utc = gmtime(&now);
        if (utc == NULL ||
            strftime(created_at, sizeof(created_at), "%Y-%m-%dT%H:%M:%SZ",
                     utc) == 0)
            return WF_ERR_STATE;

        sprintf(body,
                "{\"repo\":\"%s\",\"collection\":\"%s\","
                "\"record\":{\"$type\":\"%s\","
                "\"subject\":{\"uri\":\"%s\",\"cid\":\"%s\"},"
                "\"createdAt\":\"%s\"}}",
                session->did, collection, collection, post_uri, post_cid,
                created_at);
        return platinum_atproto_client_post(client,
                                            "com.atproto.repo.createRecord",
                                            body, out);
    }

    if (record_uri == NULL ||
        !platinum_atproto_valid_at_uri(record_uri))
        return WF_ERR_INVALID_ARG;

    sprintf(prefix, "at://%s/%s/", session->did, collection);
    prefix_length = strlen(prefix);
    if (strncmp(record_uri, prefix, prefix_length) != 0)
        return WF_ERR_INVALID_ARG;

    rkey = record_uri + prefix_length;
    if (rkey[0] == '\0')
        return WF_ERR_INVALID_ARG;

    sprintf(body, "{\"repo\":\"%s\",\"collection\":\"%s\",\"rkey\":\"%s\"}",
            session->did, collection, rkey);
    return platinum_atproto_client_post(client,
                                        "com.atproto.repo.deleteRecord",
                                        body, out);
}

wf_status platinum_atproto_notification_seen(
    platinum_atproto_client *client, const char *seen_at, wf_response *out)
{
    char escaped[128];
    char body[192];

    if (client == NULL || out == NULL || seen_at == NULL || seen_at[0] == '\0')
        return WF_ERR_INVALID_ARG;
    if (platinum_json_escape(escaped, sizeof(escaped), seen_at) != WF_OK)
        return WF_ERR_INVALID_ARG;
    if (strlen(escaped) + 24 >= sizeof(body))
        return WF_ERR_INVALID_ARG;

    strcpy(body, "{\"seenAt\":\"");
    strcat(body, escaped);
    strcat(body, "\"}");

    return platinum_atproto_client_post(client,
                                        "app.bsky.notification.updateSeen",
                                        body, out);
}
