/*
 * Feed, list, actor-graph and post-engagement routes (Part 8, slice 11).
 *
 * GET /v1/feed and /v1/list, GET /v1/follows, /v1/followers and /v1/author-feed,
 * and GET /v1/post/likes and /v1/post/reposts. Each one checks its parameter the
 * way server.ts does, then calls an operation that writes the body. The result
 * encoding is a later slice; this file decides status codes and refusals.
 */
#ifndef PLATINUM_NATIVE_COLLECTION_ROUTES_H
#define PLATINUM_NATIVE_COLLECTION_ROUTES_H

#include <stddef.h>

typedef enum platinum_collection_result {
    PLATINUM_COLLECTION_OK = 0,
    PLATINUM_COLLECTION_NOT_FOUND,
    PLATINUM_COLLECTION_ERROR
} platinum_collection_result;

/* Runs the lookup and writes its JSON body. kind is the route's own number. */
typedef platinum_collection_result (*platinum_collection_fn)(void *ctx, int kind, const char *key,
                                                             int count, const char *cursor,
                                                             char *body, size_t cap);

/* True when uri is an at:// URI of the given collection ("app.bsky.feed.generator" or
 * "app.bsky.graph.list"), as validCollectionUri accepts. */
int platinum_valid_collection_uri(const char *uri, const char *collection);

/* GET /v1/feed (is_feed 1) and /v1/list (is_feed 0). */
int platinum_route_collection(void *ctx, platinum_collection_fn fn, int is_feed, const char *uri,
                              const char *limit, const char *cursor, char *body, size_t cap);

/* GET /v1/follows (kind 0), /v1/followers (1), /v1/author-feed (2). actor is the raw value. */
int platinum_route_actor_graph(void *ctx, platinum_collection_fn fn, int kind, const char *actor,
                               const char *limit, const char *cursor, char *body, size_t cap);

/* GET /v1/post/likes (kind 0) and /v1/post/reposts (kind 1). */
int platinum_route_engagement(void *ctx, platinum_collection_fn fn, int kind, const char *uri,
                              const char *limit, const char *cursor, char *body, size_t cap);

#endif
