/*
 * Route table for the native bridge (Part 8, first slice).
 *
 * Matches an HTTP method and path to a route id, and says whether the route
 * needs a bridge token. It does not run any handler: the handlers come in later
 * slices, one route family at a time. The table mirrors the routes in
 * bridge/src/server.ts, and a test checks it against that list.
 */
#ifndef PLATINUM_NATIVE_ROUTES_H
#define PLATINUM_NATIVE_ROUTES_H

#include <stddef.h>

typedef enum platinum_route_id {
    PLATINUM_ROUTE_NONE = 0,
    PLATINUM_ROUTE_HEALTH,
    PLATINUM_ROUTE_CLIENT_METADATA,
    PLATINUM_ROUTE_LOGIN,
    PLATINUM_ROUTE_OAUTH_CALLBACK,
    PLATINUM_ROUTE_PAIR,
    PLATINUM_ROUTE_APP_PASSWORD_LOGIN,
    PLATINUM_ROUTE_REVOKE,
    PLATINUM_ROUTE_PROFILE,
    PLATINUM_ROUTE_TIMELINE,
    PLATINUM_ROUTE_CONVERSATIONS,
    PLATINUM_ROUTE_CONVERSATION_MESSAGE,
    PLATINUM_ROUTE_CONVERSATION,
    PLATINUM_ROUTE_NOTIFICATIONS,
    PLATINUM_ROUTE_LIKE,
    PLATINUM_ROUTE_REPOST,
    PLATINUM_ROUTE_NOTIFICATIONS_SEEN,
    PLATINUM_ROUTE_FOLLOWS,
    PLATINUM_ROUTE_FOLLOWERS,
    PLATINUM_ROUTE_AUTHOR_FEED,
    PLATINUM_ROUTE_FOLLOW,
    PLATINUM_ROUTE_MUTE,
    PLATINUM_ROUTE_BLOCK,
    PLATINUM_ROUTE_POST_LIKES,
    PLATINUM_ROUTE_POST_REPOSTS,
    PLATINUM_ROUTE_SEARCH_ACTORS,
    PLATINUM_ROUTE_SEARCH_POSTS,
    PLATINUM_ROUTE_FEEDS,
    PLATINUM_ROUTE_IMAGE,
    PLATINUM_ROUTE_MUTED_WORDS_GET,
    PLATINUM_ROUTE_MUTED_WORDS_SET,
    PLATINUM_ROUTE_LISTS,
    PLATINUM_ROUTE_FEED,
    PLATINUM_ROUTE_LIST,
    PLATINUM_ROUTE_POST_DELETE,
    PLATINUM_ROUTE_THREAD,
    PLATINUM_ROUTE_POST
} platinum_route_id;

typedef enum platinum_route_result {
    PLATINUM_ROUTE_FOUND = 0,
    PLATINUM_ROUTE_NOT_FOUND,         /* no route has this path: 404 */
    PLATINUM_ROUTE_METHOD_NOT_ALLOWED /* the path exists under another method: 405 */
} platinum_route_result;

/* Looks up method and path (the path without its query string). On success
 * writes the route id and whether a bridge token is required. */
platinum_route_result platinum_route_match(const char *method, const char *path,
                                           platinum_route_id *out_id, int *out_needs_token);

/* Reads a bearer token from an Authorization header value. The rule matches
 * server.ts: the header must start with "Bearer " (case-sensitive) and the rest
 * is the token. Returns 0 and writes the token, or -1 if there is none or it
 * does not fit. An empty token counts as none. */
int platinum_route_bearer_token(const char *authorization, char *out, size_t cap);

/* Number of routes in the table. */
size_t platinum_route_count(void);

/* Stable name for logs and tests. Never a token or a path parameter. */
const char *platinum_route_name(platinum_route_id id);

#endif
