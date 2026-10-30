/*
 * Search routes (Part 8, slice 10): GET /v1/search/actors and /v1/search/posts.
 *
 * The query and limit rules of server.ts. The search itself is a callback that
 * returns the response body, since the result encoding is a later slice.
 */
#ifndef PLATINUM_NATIVE_SEARCH_ROUTES_H
#define PLATINUM_NATIVE_SEARCH_ROUTES_H

#include <stddef.h>

/* The limit query value as server.ts reads it: JavaScript's Number() of the value
 * (20 when absent), truncated and clamped to 1..50. NULL means absent. */
int platinum_query_limit(const char *value);

typedef enum platinum_search_result {
    PLATINUM_SEARCH_OK = 0,
    PLATINUM_SEARCH_ERROR
} platinum_search_result;

/* Runs the search and writes its JSON body. cursor is NULL when absent. */
typedef platinum_search_result (*platinum_search_fn)(void *ctx, int posts, const char *query,
                                                     int count, const char *cursor,
                                                     char *body, size_t cap);

/* GET /v1/search/actors (posts = 0) and /v1/search/posts (posts = 1). q is the raw
 * query value, or NULL. */
int platinum_route_search(void *ctx, platinum_search_fn fn, int posts, const char *q,
                          const char *limit, const char *cursor, char *body, size_t cap);

#endif
