/*
 * The source fetch for GET /v1/image (Part 9): the image bytes from the Bluesky CDN,
 * through Wolfram's public HTTPS GET (no credentials, a size cap, and its CA bundle).
 *
 * The CDN is HTTPS only, and Wolfram refuses plain HTTP for this call. The base URL is
 * the CDN in production; tests pass a local HTTPS server with a test certificate.
 */
#ifndef PLATINUM_NATIVE_IMAGE_CDN_H
#define PLATINUM_NATIVE_IMAGE_CDN_H

#include <stddef.h>
#include <wolfram/xrpc.h>
#include "image_route.h"

#define PLATINUM_CDN_BASE "https://cdn.bsky.app"
/* The largest source fetched: a 2048 px JPEG is well under this. */
#define PLATINUM_CDN_MAX_BYTES (12 * 1024 * 1024)

typedef struct platinum_cdn {
    wf_xrpc_client *client;
    const char *base;      /* https://host[:port], without a trailing slash */
} platinum_cdn;

/* A platinum_image_source_fn whose ctx is a platinum_cdn *. */
platinum_image_result platinum_image_source_cdn(void *ctx, const char *ref, platinum_rgba *out);

#endif
