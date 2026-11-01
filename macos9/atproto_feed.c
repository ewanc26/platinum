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
