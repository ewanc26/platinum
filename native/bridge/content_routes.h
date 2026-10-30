/*
 * Muted-word list and post delete routes (Part 8, slice 9).
 *
 * The status and body decisions of server.ts for GET /v1/muted-words and
 * POST /v1/post/delete. The operations are callbacks, so the Wolfram calls stay out.
 */
#ifndef PLATINUM_NATIVE_CONTENT_ROUTES_H
#define PLATINUM_NATIVE_CONTENT_ROUTES_H

#include <stddef.h>

#define PLATINUM_MUTED_LIST_MAX 100

typedef struct platinum_muted_view {
    char value[512];
    int content;   /* applies to post text */
    int tag;       /* applies to tags */
} platinum_muted_view;

typedef enum platinum_content_result {
    PLATINUM_CONTENT_OK = 0,
    PLATINUM_CONTENT_NOT_YOURS,
    PLATINUM_CONTENT_ERROR
} platinum_content_result;

/* Fills up to max entries and sets *count. */
typedef platinum_content_result (*platinum_muted_list_fn)(void *ctx, platinum_muted_view *views,
                                                          size_t max, size_t *count);

/* Deletes the post at uri (already validated). */
typedef platinum_content_result (*platinum_delete_fn)(void *ctx, const char *uri);

/* GET /v1/muted-words. At most 100 words, each with the targets it applies to. */
int platinum_route_muted_list(void *ctx, platinum_muted_list_fn fn, char *body, size_t cap);

/* POST /v1/post/delete. uri is the raw client value, or NULL when absent. */
int platinum_route_post_delete(void *ctx, platinum_delete_fn fn, const char *uri,
                               char *body, size_t cap);

#endif
