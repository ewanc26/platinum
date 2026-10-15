/*
 * The live publish callback for POST /v1/post, over Wolfram (Part 8, post slice).
 *
 * Supported here: plain posts, quote posts, and the reply gates "everyone" and
 * "nobody" (Wolfram's threadgate). Not supported, and refused with a 500 rather
 * than approximated:
 *   - replies: the TypeScript takes the thread root from the parent record, and
 *     this path has only the parent's reference, so the root would be wrong.
 *   - the gates "mentioned", "following" and "followers": Wolfram has one rule
 *     ("followed and mentioned"), which is not the same as any of them. The post is
 *     published and replyGateApplied is false, as the TypeScript reports a gate
 *     that could not be set.
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
