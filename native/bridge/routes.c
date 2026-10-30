#include "routes.h"

#include <string.h>

typedef struct platinum_route_entry {
    const char *method;
    const char *path;
    platinum_route_id id;
    int needs_token;
    const char *name;
} platinum_route_entry;

/* needs_token is 1 for every route that server.ts guards with the bearer token. */
static const platinum_route_entry route_table[] = {
    { "GET",  "/health",                    PLATINUM_ROUTE_HEALTH,                 0, "health" },
    { "GET",  "/client-metadata.json",      PLATINUM_ROUTE_CLIENT_METADATA,        0, "client-metadata" },
    { "GET",  "/login",                     PLATINUM_ROUTE_LOGIN,                  0, "login" },
    { "GET",  "/atproto-oauth-callback",    PLATINUM_ROUTE_OAUTH_CALLBACK,         0, "oauth-callback" },
    { "POST", "/v1/pair",                   PLATINUM_ROUTE_PAIR,                   0, "pair" },
    { "POST", "/v1/login/app-password",     PLATINUM_ROUTE_APP_PASSWORD_LOGIN,     0, "app-password-login" },
    { "POST", "/v1/revoke",                 PLATINUM_ROUTE_REVOKE,                 1, "revoke" },
    { "GET",  "/v1/profile",                PLATINUM_ROUTE_PROFILE,                1, "profile" },
    { "GET",  "/v1/timeline",               PLATINUM_ROUTE_TIMELINE,               1, "timeline" },
    { "GET",  "/v1/conversations",          PLATINUM_ROUTE_CONVERSATIONS,          1, "conversations" },
    { "POST", "/v1/conversation/message",   PLATINUM_ROUTE_CONVERSATION_MESSAGE,   1, "conversation-message" },
    { "GET",  "/v1/conversation",           PLATINUM_ROUTE_CONVERSATION,           1, "conversation" },
    { "GET",  "/v1/notifications",          PLATINUM_ROUTE_NOTIFICATIONS,          1, "notifications" },
    { "POST", "/v1/like",                   PLATINUM_ROUTE_LIKE,                   1, "like" },
    { "POST", "/v1/repost",                 PLATINUM_ROUTE_REPOST,                 1, "repost" },
    { "POST", "/v1/notifications/seen",     PLATINUM_ROUTE_NOTIFICATIONS_SEEN,     1, "notifications-seen" },
    { "GET",  "/v1/follows",                PLATINUM_ROUTE_FOLLOWS,                1, "follows" },
    { "GET",  "/v1/followers",              PLATINUM_ROUTE_FOLLOWERS,              1, "followers" },
    { "GET",  "/v1/author-feed",            PLATINUM_ROUTE_AUTHOR_FEED,            1, "author-feed" },
    { "POST", "/v1/follow",                 PLATINUM_ROUTE_FOLLOW,                 1, "follow" },
    { "POST", "/v1/mute",                   PLATINUM_ROUTE_MUTE,                   1, "mute" },
    { "POST", "/v1/block",                  PLATINUM_ROUTE_BLOCK,                  1, "block" },
    { "GET",  "/v1/post/likes",             PLATINUM_ROUTE_POST_LIKES,             1, "post-likes" },
    { "GET",  "/v1/post/reposts",           PLATINUM_ROUTE_POST_REPOSTS,           1, "post-reposts" },
    { "GET",  "/v1/search/actors",          PLATINUM_ROUTE_SEARCH_ACTORS,          1, "search-actors" },
    { "GET",  "/v1/search/posts",           PLATINUM_ROUTE_SEARCH_POSTS,           1, "search-posts" },
    { "GET",  "/v1/feeds",                  PLATINUM_ROUTE_FEEDS,                  1, "feeds" },
    { "GET",  "/v1/image",                  PLATINUM_ROUTE_IMAGE,                  1, "image" },
    { "GET",  "/v1/muted-words",            PLATINUM_ROUTE_MUTED_WORDS_GET,        1, "muted-words-get" },
    { "POST", "/v1/muted-words",            PLATINUM_ROUTE_MUTED_WORDS_SET,        1, "muted-words-set" },
    { "GET",  "/v1/lists",                  PLATINUM_ROUTE_LISTS,                  1, "lists" },
    { "GET",  "/v1/feed",                   PLATINUM_ROUTE_FEED,                   1, "feed" },
    { "GET",  "/v1/list",                   PLATINUM_ROUTE_LIST,                   1, "list" },
    { "POST", "/v1/post/delete",            PLATINUM_ROUTE_POST_DELETE,            1, "post-delete" },
    { "GET",  "/v1/thread",                 PLATINUM_ROUTE_THREAD,                 1, "thread" },
    { "POST", "/v1/post",                   PLATINUM_ROUTE_POST,                   1, "post" }
};

#define ROUTE_COUNT (sizeof(route_table) / sizeof(route_table[0]))

platinum_route_result
platinum_route_match(const char *method, const char *path,
                     platinum_route_id *out_id, int *out_needs_token)
{
    size_t i;
    int path_seen = 0;

    if (method == NULL || path == NULL) return PLATINUM_ROUTE_NOT_FOUND;

    for (i = 0; i < ROUTE_COUNT; i++) {
        const platinum_route_entry *e = &route_table[i];
        if (strcmp(e->path, path) != 0) continue;
        path_seen = 1;
        if (strcmp(e->method, method) == 0) {
            if (out_id) *out_id = e->id;
            if (out_needs_token) *out_needs_token = e->needs_token;
            return PLATINUM_ROUTE_FOUND;
        }
    }
    return path_seen ? PLATINUM_ROUTE_METHOD_NOT_ALLOWED : PLATINUM_ROUTE_NOT_FOUND;
}

int
platinum_route_bearer_token(const char *authorization, char *out, size_t cap)
{
    const char prefix[] = "Bearer ";
    const size_t plen = sizeof(prefix) - 1;
    size_t n;

    if (authorization == NULL || out == NULL || cap == 0) return -1;
    if (strncmp(authorization, prefix, plen) != 0) return -1;

    n = strlen(authorization + plen);
    if (n == 0 || n >= cap) return -1;
    memcpy(out, authorization + plen, n + 1);
    return 0;
}

const char *
platinum_route_name(platinum_route_id id)
{
    size_t i;
    for (i = 0; i < ROUTE_COUNT; i++) {
        if (route_table[i].id == id) return route_table[i].name;
    }
    return "unknown";
}

size_t
platinum_route_count(void)
{
    return ROUTE_COUNT;
}
