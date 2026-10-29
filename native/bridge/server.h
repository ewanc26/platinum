/*
 * The native bridge's HTTP server (Part 10).
 *
 * It binds only to a private address (the transport decision in docs/NATIVE_BRIDGE.md),
 * answers in the order server.ts does (unknown paths first, then the bearer check, then
 * the route), and runs all route families: every route now performs its full input
 * validation (bad JSON → 400, missing fields → 400, bad refs → 400) and dispatches to
 * its handler module.  Routes without a live Wolfram call yet use a stub callback that
 * returns a well-formed 500 bridge_error response rather than 501.  App-password
 * sign-in and revoke are served when an auth module is supplied.  Tokens are looked
 * up through a callback, so the token store (Part 3) and its persistence (Part 4) are
 * chosen by the caller.
 */
#ifndef PLATINUM_NATIVE_SERVER_H
#define PLATINUM_NATIVE_SERVER_H

#include <stddef.h>

#include "image_route.h"

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
    /* The CDN the image route fetches from; NULL answers image requests with 502. */
    void *cdn;
    /* The image source for GET /v1/image; NULL uses platinum_image_source_cdn.
     * A caller injects one to exercise the route, and its binary response, without
     * the network. */
    platinum_image_source_fn image_source;
    /* platinum_bridge_auth for the sign-in and revoke routes; NULL leaves them 501. */
    void *auth;
} platinum_server_ctx;

/* Answers one request. target is the path and query, authorization the Authorization
 * header or NULL. Writes the body into out and returns the HTTP status. JSON only:
 * a binary body needs platinum_server_dispatch_ex to report its length. */
int platinum_server_dispatch(const platinum_server_ctx *ctx, const char *method, const char *target,
                             const char *authorization, const char *body, size_t body_len,
                             char *out, size_t cap);

/* As platinum_server_dispatch, also reporting the response content type, the
 * peer address used to key the app-password failure limiter, and how many bytes
 * of `out` are the body. The default content type is application/json; the OAuth
 * sign-in page is HTML and an image is application/x-platinum-image. peer may be
 * NULL.
 *
 * `out_len` is required for a binary body: it is the only way to tell where the
 * body ends, because a blob may contain a zero byte. It is set for every status,
 * so a caller never has to re-derive the length; JSON is NUL-terminated and its
 * length is the string length. It may be NULL only for a caller that cannot
 * receive a binary body. */
int platinum_server_dispatch_ex(const platinum_server_ctx *ctx, const char *method,
                                const char *target, const char *authorization,
                                const char *peer, const char *body, size_t body_len,
                                char *out, size_t cap, const char **content_type,
                                size_t *out_len);

#endif
