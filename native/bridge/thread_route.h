/*
 * GET /v1/thread — the Mac's Thread window (Part 10).
 *
 * The AppView returns a reply tree; the Mac draws rows. This route flattens the
 * tree in the order server.ts uses: ancestors oldest first with a negative
 * depth, the post asked for at depth 0, then replies depth-first with a positive
 * depth. At most 40 posts, replies no deeper than six, and `truncated` says
 * whether anything was left out. Blocked and deleted posts are skipped.
 *
 * The fetch is a callback, so this file needs neither the network nor Wolfram;
 * the Wolfram-backed flatten lives in thread_live.c.
 */
#ifndef PLATINUM_NATIVE_THREAD_ROUTE_H
#define PLATINUM_NATIVE_THREAD_ROUTE_H

#include "timeline_route.h"

/* One flat, bounded list, as the TypeScript's MAX_THREAD_POSTS. */
#define PLATINUM_THREAD_POSTS_MAX 40
/* Replies deeper than this are cut, as MAX_THREAD_DEPTH. */
#define PLATINUM_THREAD_DEPTH_MAX 6

typedef struct platinum_thread_entry {
    platinum_timeline_post post;
    int depth;
} platinum_thread_entry;

typedef enum platinum_thread_result {
    PLATINUM_THREAD_OK = 0,
    PLATINUM_THREAD_NOT_FOUND,
    PLATINUM_THREAD_ERROR
} platinum_thread_result;

/* Fills at most max entries, sets *count, and sets *truncated when anything was
 * left out. The uri has already been validated. */
typedef platinum_thread_result (*platinum_thread_fn)(void *ctx, const char *uri,
                                                     platinum_thread_entry *entries,
                                                     size_t max, size_t *count,
                                                     int *truncated);

/* GET /v1/thread. uri is the raw query value, or NULL when absent. Writes the
 * body, returns 400 for a bad reference, 404 when the post is gone, 500 when
 * the fetch fails or the body does not fit. */
int platinum_route_thread_view(void *ctx, platinum_thread_fn fn, const char *uri,
                               char *body, size_t cap);

#endif
