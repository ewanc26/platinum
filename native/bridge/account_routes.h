/*
 * Notifications-seen and muted-word write routes (Part 8, slice 8).
 *
 * The status and body decisions of server.ts for POST /v1/notifications/seen and
 * POST /v1/muted-words. Input is parsed by the server layer; the operations are
 * callbacks, so the Wolfram calls stay out of this file.
 */
#ifndef PLATINUM_NATIVE_ACCOUNT_ROUTES_H
#define PLATINUM_NATIVE_ACCOUNT_ROUTES_H

#include <stddef.h>

typedef enum platinum_account_result {
    PLATINUM_ACCOUNT_OK = 0,
    PLATINUM_ACCOUNT_ERROR
} platinum_account_result;

/* Records the seen time (already normalised). */
typedef platinum_account_result (*platinum_seen_fn)(void *ctx, const char *seen_at);

/* Mutes (on) or unmutes a word (already trimmed). */
typedef platinum_account_result (*platinum_word_fn)(void *ctx, const char *word, int on);

/* seen_at is the raw client value, or NULL when absent. now_ms is the current time. */
int platinum_route_seen(void *ctx, platinum_seen_fn fn, const char *seen_at, double now_ms,
                        char *body, size_t cap);

/* word is the raw client value, or NULL when absent. on_valid is 0 when "on" was not a boolean. */
int platinum_route_muted_set(void *ctx, platinum_word_fn fn, const char *word, int on_valid,
                             int on, char *body, size_t cap);

#endif
