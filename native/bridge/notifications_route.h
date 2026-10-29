/*
 * GET /v1/notifications (Part 8, the notifications slice).
 *
 * The status and body decisions of server.ts for the notification list: the
 * limit is 1 to 50 (default 20), the cursor is passed through, and the response
 * is the bridge's notifications contract -- the notification's uri and cid, its
 * author (handle, display name, avatar ref), reason, indexedAt and isRead.
 *
 * The fetch is a callback, so this file needs neither the network nor Wolfram.
 * The Wolfram-backed fetch lives in notifications_wolfram.c, which uses the
 * typed notification parser and is built with -std=c2x.
 */
#ifndef PLATINUM_NATIVE_NOTIFICATIONS_ROUTE_H
#define PLATINUM_NATIVE_NOTIFICATIONS_ROUTE_H

#include <stddef.h>

#define PLATINUM_NOTIFICATIONS_LIMIT_MIN 1
#define PLATINUM_NOTIFICATIONS_LIMIT_MAX 50
#define PLATINUM_NOTIFICATIONS_LIMIT_DEFAULT 20
/* One page holds at most the largest limit the route accepts. */
#define PLATINUM_NOTIFICATIONS_MAX 50

/* One notification as the bridge serves it. Every field is present; the
 * optional author fields are empty strings when the source had none. */
typedef struct platinum_notification_author {
    char did[256];
    char handle[256];
    char display_name[65];
    char avatar[301];
} platinum_notification_author;

typedef struct platinum_notification {
    char uri[513];
    char cid[256];
    platinum_notification_author author;
    char reason[33];
    char indexed_at[65];
    int is_read;
} platinum_notification;

typedef struct platinum_notifications {
    platinum_notification *items;   /* malloc'd, item_count entries; may be NULL */
    size_t item_count;
    char cursor[256];
    int has_cursor;
} platinum_notifications;

typedef enum platinum_notifications_result {
    PLATINUM_NOTIFICATIONS_OK = 0,
    PLATINUM_NOTIFICATIONS_ERROR
} platinum_notifications_result;

/* Fetches one page. limit is 1 to 50 and cursor is the previous page's cursor,
 * or NULL for the first page. Fills out; the caller frees with
 * platinum_notifications_free. */
typedef platinum_notifications_result (*platinum_notifications_fetch_fn)(
    void *ctx, int limit, const char *cursor, platinum_notifications *out);

/* Releases the items and resets the page. NULL-safe. */
void platinum_notifications_free(platinum_notifications *out);

/* GET /v1/notifications. limit_param and cursor_param are the raw query values,
 * or NULL when absent. Writes the JSON body and returns the HTTP status. */
int platinum_route_notifications(void *ctx, platinum_notifications_fetch_fn fetch,
                                 const char *limit_param, const char *cursor_param,
                                 char *body, size_t cap);

#endif
