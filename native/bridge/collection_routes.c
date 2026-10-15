#include "collection_routes.h"

#include <stdio.h>
#include <string.h>

#include "profile.h"
#include "search_routes.h"
#include "validate.h"

static int
write_error(char *body, size_t cap, int status, const char *code, const char *message)
{
    if (strlen(code) + strlen(message) + 32 >= cap) {
        body[0] = '\0';
        return 500;
    }
    strcpy(body, "{\"error\":\"");
    strcat(body, code);
    strcat(body, "\",\"message\":\"");
    strcat(body, message);
    strcat(body, "\"}");
    return status;
}

static int
is_alnum_or_mark(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == ':' || c == '%' || c == '-';
}

static int
is_rkey_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == '~' || c == ':' || c == '-';
}

/* validCollectionUri: at://did:<method>:<id>/<collection>/<rkey>, lengths as the TypeScript. */
int
platinum_valid_collection_uri(const char *uri, const char *collection)
{
    const char *p;
    size_t clen;

    if (uri == NULL || collection == NULL || strlen(uri) > 512) return -1;
    if (strncmp(uri, "at://did:", 9) != 0) return -1;

    /* The DID: method then id, up to the slash that starts the collection. */
    p = uri + 9;
    if (!(*p >= 'a' && *p <= 'z')) return -1;
    while (*p >= 'a' && *p <= 'z') p++;
    if (*p != ':') return -1;
    p++;
    if (!is_alnum_or_mark(*p)) return -1;
    while (is_alnum_or_mark(*p)) p++;
    if (*p != '/') return -1;
    p++;

    clen = strlen(collection);
    if (strncmp(p, collection, clen) != 0 || p[clen] != '/') return -1;
    p += clen + 1;

    if (*p == '\0') return -1;
    while (*p) {
        if (!is_rkey_char(*p)) return -1;
        p++;
    }
    return 0;
}

int
platinum_route_collection(void *ctx, platinum_collection_fn fn, int is_feed, const char *uri,
                          const char *limit, const char *cursor, char *body, size_t cap)
{
    const char *collection = is_feed ? "app.bsky.feed.generator" : "app.bsky.graph.list";
    platinum_collection_result r;
    int count;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (platinum_valid_collection_uri(uri, collection) != 0) {
        return write_error(body, cap, 400, "invalid_uri", "uri must be an AT URI of a feed or a list.");
    }
    count = platinum_query_limit(limit);
    r = fn(ctx, is_feed ? 0 : 1, uri, count, cursor, body, cap);
    if (r == PLATINUM_COLLECTION_NOT_FOUND) {
        body[0] = '\0';
        return write_error(body, cap, 404, "not_found", "No such feed or list.");
    }
    if (r != PLATINUM_COLLECTION_OK) {
        body[0] = '\0';
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    return 200;
}

int
platinum_route_actor_graph(void *ctx, platinum_collection_fn fn, int kind, const char *actor,
                           const char *limit, const char *cursor, char *body, size_t cap)
{
    char normalised[256];
    platinum_collection_result r;
    int count;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (actor == NULL || platinum_valid_actor(actor, normalised, sizeof normalised) != 0) {
        return write_error(body, cap, 400, "invalid_actor", "actor must be a handle or a DID.");
    }
    count = platinum_query_limit(limit);
    r = fn(ctx, kind, normalised, count, cursor, body, cap);
    if (r == PLATINUM_COLLECTION_NOT_FOUND) {
        body[0] = '\0';
        return write_error(body, cap, 404, "actor_not_found", "No such account.");
    }
    if (r != PLATINUM_COLLECTION_OK) {
        body[0] = '\0';
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    return 200;
}

int
platinum_route_engagement(void *ctx, platinum_collection_fn fn, int kind, const char *uri,
                          const char *limit, const char *cursor, char *body, size_t cap)
{
    platinum_collection_result r;
    int count;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (platinum_valid_post_ref(uri, "x") != 0) {
        return write_error(body, cap, 400, "invalid_post_ref", "uri must be an app.bsky.feed.post AT URI.");
    }
    count = platinum_query_limit(limit);
    r = fn(ctx, kind, uri, count, cursor, body, cap);
    if (r == PLATINUM_COLLECTION_NOT_FOUND) {
        body[0] = '\0';
        return write_error(body, cap, 404, "post_not_found", "The post no longer exists.");
    }
    if (r != PLATINUM_COLLECTION_OK) {
        body[0] = '\0';
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    return 200;
}

int
platinum_route_thread(void *ctx, platinum_collection_fn fn, const char *uri, char *body, size_t cap)
{
    /* The thread lookup takes no paging: it returns the post's ancestors and replies in one answer. */
    return platinum_route_engagement(ctx, fn, 2, uri, NULL, NULL, body, cap);
}
