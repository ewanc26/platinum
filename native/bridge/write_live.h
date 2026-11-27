/*
 * The live write callbacks for like, repost, follow, mute, block and post delete,
 * over Wolfram. Each one is idempotent the way api.ts is: it reads the viewer state
 * the AppView reports first, and writes only when the state asked for differs, so
 * the Mac sends the state it wants and never holds a record URI.
 */
#ifndef PLATINUM_NATIVE_WRITE_LIVE_H
#define PLATINUM_NATIVE_WRITE_LIVE_H

#include "content_routes.h"
#include "write_routes.h"

/* platinum_toggle_fn for /v1/like (kind 0) and /v1/repost (1). ctx is a wf_agent *.
 * A post the AppView no longer has is OK with *out_on 0, which the route answers
 * with 404 when the client asked for on. */
platinum_write_result platinum_toggle_live(void *ctx, int kind, const char *uri,
                                           const char *cid, int on, int *out_on,
                                           long *out_count);

/* platinum_subject_fn for /v1/follow (kind 0), /v1/mute (1) and /v1/block (2). ctx
 * is a wf_agent *. An account the AppView answers with 400 is NOT_FOUND, as api.ts
 * maps it. */
platinum_write_result platinum_subject_live(void *ctx, int kind, const char *did, int on);

/* platinum_delete_fn for /v1/post/delete. ctx is a wf_agent *. A post in another
 * repository is NOT_YOURS and nothing is sent. */
platinum_content_result platinum_delete_live(void *ctx, const char *uri);

#endif
