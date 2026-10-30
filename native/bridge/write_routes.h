/*
 * Write routes: like, repost, follow, mute and block (Part 8, slice 6).
 *
 * The status and body decisions of server.ts and api.ts for these routes. The
 * request body has already been parsed by the server layer and comes in as
 * strings plus a flag for whether "on" was a boolean. The operation itself is a
 * callback, so the Wolfram calls stay out of this file.
 */
#ifndef PLATINUM_NATIVE_WRITE_ROUTES_H
#define PLATINUM_NATIVE_WRITE_ROUTES_H

#include <stddef.h>

typedef enum platinum_write_result {
    PLATINUM_WRITE_OK = 0,
    PLATINUM_WRITE_NOT_FOUND,
    PLATINUM_WRITE_ERROR
} platinum_write_result;

/* Like (kind 0) or repost (kind 1). On OK, *out_on is whether the post now has it
 * and *out_count is the new count. */
typedef platinum_write_result (*platinum_toggle_fn)(void *ctx, int kind, const char *uri,
                                                    const char *cid, int on, int *out_on,
                                                    long *out_count);

/* Follow (kind 0), mute (1) or block (2). */
typedef platinum_write_result (*platinum_subject_fn)(void *ctx, int kind, const char *did, int on);

/* POST /v1/like (kind 0) and /v1/repost (kind 1). on_valid is 0 when "on" was not a
 * boolean. Returns the HTTP status and writes the body. */
int platinum_route_toggle(void *ctx, platinum_toggle_fn fn, int kind, const char *uri,
                          const char *cid, int on_valid, int on, char *body, size_t cap);

/* POST /v1/follow (kind 0), /v1/mute (1) and /v1/block (2). did is NULL when absent. */
int platinum_route_subject(void *ctx, platinum_subject_fn fn, int kind, const char *did,
                           int on_valid, int on, char *body, size_t cap);

#endif
