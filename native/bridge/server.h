/*
 * The native bridge's HTTP server (Part 10, first slice).
 *
 * It binds only to a private address (the transport decision in docs/NATIVE_BRIDGE.md),
 * answers in the order server.ts does (unknown paths first, then the bearer check, then
 * the route), and runs the live routes that are wired so far: GET /health, GET
 * /v1/profile and POST /v1/post. Other routes in the table answer 501 until their
 * live calls are wired. Tokens are looked up through a callback, so the token store
 * (Part 3) and its persistence (Part 4) are chosen by the caller.
 */
#ifndef PLATINUM_NATIVE_SERVER_H
#define PLATINUM_NATIVE_SERVER_H

#include <stddef.h>

/* 1 when host is a loopback or private address the bridge may listen on: 127.0.0.0/8,
 * ::1, 10/8, 172.16/12, 192.168/16, 169.254/16 (link-local). Otherwise 0. */
int platinum_bind_is_private(const char *host);

/* Looks up the Wolfram agent for a bearer token. NULL means the token is not valid. */
typedef void *(*platinum_token_lookup_fn)(void *user, const char *token);

typedef struct platinum_server_ctx {
    platinum_token_lookup_fn lookup;
    void *user;
    const char *version;
    size_t max_body_bytes;
} platinum_server_ctx;

/* Answers one request. target is the path and query, authorization the Authorization
 * header or NULL. Writes the body into out and returns the HTTP status. */
int platinum_server_dispatch(const platinum_server_ctx *ctx, const char *method, const char *target,
                             const char *authorization, const char *body, size_t body_len,
                             char *out, size_t cap);

#endif
