/*
 * The live publish callback for POST /v1/post, over Wolfram (Part 8, post slice).
 *
 * Supported here: plain posts, quote posts, replies, and the reply gates "everyone"
 * and "nobody" (Wolfram's threadgate). A reply reads the parent post and takes the
 * thread root from the parent's reply.root, or the parent itself when it is
 * top-level, as api.ts does; the parent's cid is the AppView's, not the client's.
 * A parent that no longer exists is PLATINUM_POST_NOT_FOUND.
 *
 * Not supported: the gates "mentioned", "following" and "followers". Wolfram has one
 * rule ("followed and mentioned") that is not the same as any of them, so the post is
 * published and replyGateApplied is false, as the TypeScript reports a gate that could
 * not be set.
 */
#ifndef PLATINUM_NATIVE_PUBLISH_LIVE_H
#define PLATINUM_NATIVE_PUBLISH_LIVE_H

#include <stddef.h>
#include "post_routes.h"

/* A platinum_publish_fn whose ctx is a wf_agent *. */
platinum_post_result platinum_publish_live(void *ctx, const char *text, const platinum_ref *reply,
                                           const platinum_ref *quote, const char *gate,
                                           char *body, size_t cap);

#endif
