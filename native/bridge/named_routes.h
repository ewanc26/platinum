/*
 * The named-list routes: GET /v1/feeds and GET /v1/lists (Part 10).
 *
 * Both answer the same shape server.ts builds — {"items":[{"uri","name"}]} —
 * from an operation that is a callback, so the Wolfram calls stay in
 * named_live.c and this file needs neither the network nor Wolfram.
 */
#ifndef PLATINUM_NATIVE_NAMED_ROUTES_H
#define PLATINUM_NATIVE_NAMED_ROUTES_H

#include <stddef.h>

/* Each list serves at most this many names, as the TypeScript does. */
#define PLATINUM_NAMED_MAX 100

typedef struct platinum_named_view {
    char uri[513];
    char name[65];
} platinum_named_view;

typedef enum platinum_named_result {
    PLATINUM_NAMED_OK = 0,
    PLATINUM_NAMED_ERROR
} platinum_named_result;

/* Fills up to max entries and sets *count. */
typedef platinum_named_result (*platinum_named_fn)(void *ctx, platinum_named_view *views,
                                                   size_t max, size_t *count);

/* Writes {"items":[...]} and returns 200, or 500 when the fetch fails or the
 * body does not fit. `name` and `uri` are escaped. */
int platinum_route_named(void *ctx, platinum_named_fn fn, char *body, size_t cap);

#endif
