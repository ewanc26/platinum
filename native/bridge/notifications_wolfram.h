/*
 * The live notifications fetch and seen mark: Wolfram's listNotifications and
 * updateSeen, adapted to the routes (Part 8, the notifications slice). Built as
 * C2x against Wolfram's headers and linked to libwolfram; the routes themselves
 * do not change.
 */
#ifndef PLATINUM_NATIVE_NOTIFICATIONS_WOLFRAM_H
#define PLATINUM_NATIVE_NOTIFICATIONS_WOLFRAM_H

#include "account_routes.h"
#include "notifications_route.h"

/* A platinum_notifications_fetch_fn whose ctx is a wf_agent *. The items are
 * malloc'd by this function and released by platinum_notifications_free. */
platinum_notifications_result platinum_notifications_fetch_live(
    void *ctx, int limit, const char *cursor, platinum_notifications *out);

/* A platinum_seen_fn whose ctx is a wf_agent *. Marks everything up to seen_at
 * read; the route has already validated the timestamp. */
platinum_account_result platinum_notifications_seen_live(void *ctx, const char *seen_at);

#endif
