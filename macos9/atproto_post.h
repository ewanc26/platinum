#ifndef PLATINUM_ATPROTO_POST_H
#define PLATINUM_ATPROTO_POST_H

/*
 * atproto_post.h -- publish a post through the native session.
 *
 * This is the native half of POST /v1/post: an app.bsky.feed.post record
 * written with com.atproto.repo.createRecord, carrying its text, rich-text
 * facets, a reply reference or a quote embed, and, when a reply gate is
 * chosen, an app.bsky.feed.threadgate record under the post's own record key.
 *
 * The reply root and the quoted post's cid come from the AppView
 * (app.bsky.feed.getPosts), not from the Mac's copy of the row, as the bridge
 * does: a reply names its thread root correctly and a stale copy cannot be
 * quoted. A parent or quoted post that no longer exists writes nothing.
 *
 * Facets are a C89 subset of Wolfram's richtext.h rules (which are C++ and not
 * part of the Mac OS 9 transport): http(s) links, @handle mentions resolved to
 * a DID, and #tags. Offsets are bytes into the exact UTF-8 text sent. A mention
 * that does not resolve gets no facet; it never fails the post.
 *
 * The body builders allocate nothing on the network side and link on a host,
 * so the tests check the exact JSON the Mac sends.
 */

#include <stddef.h>

#include "atproto_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Most facets one post carries; later matches are left as plain text. */
#define PLATINUM_ATPROTO_FACETS_MAX 16

/*
 * Resolve `handle` (no @) to a DID in `did`, `cap` bytes. Return 1 when it
 * resolved, 0 when it did not (the mention is then left as plain text).
 */
typedef int (*platinum_atproto_handle_resolver)(const char *handle, char *did,
                                                size_t cap, void *userdata);

/*
 * The facets JSON array for `utf8_text`: "[{...},...]", or an empty string
 * when the text has none. `resolve` may be NULL, which leaves every mention
 * as plain text. The caller frees the result; NULL on bad input or no memory.
 */
char *platinum_atproto_post_facets(const char *utf8_text,
                                   platinum_atproto_handle_resolver resolve,
                                   void *userdata);

/*
 * The com.atproto.repo.createRecord body for an app.bsky.feed.post record.
 *
 * `facets_json` is NULL, empty, or an array from platinum_atproto_post_facets.
 * The reply references are all four or none; the quote is both or neither; a
 * post is not both a reply and a quote here, because compose never makes one.
 * `created_at` is an RFC 3339 UTC timestamp. The caller frees the result;
 * NULL on bad input or no memory.
 */
char *platinum_atproto_post_record_body(
    const char *repo_did, const char *utf8_text, const char *facets_json,
    const char *root_uri, const char *root_cid, const char *parent_uri,
    const char *parent_cid, const char *quote_uri, const char *quote_cid,
    const char *created_at);

/*
 * The createRecord body for the app.bsky.feed.threadgate that limits replies
 * to `post_uri`, which must be a post in `repo_did`; the gate's record key is
 * the post's. `gate` is "nobody" (allow is an empty list), "mentioned",
 * "following" or "followers". "everyone" has no gate and is refused here.
 */
wf_status platinum_atproto_threadgate_body(const char *repo_did,
                                           const char *post_uri,
                                           const char *gate,
                                           const char *created_at, char *out,
                                           size_t cap);

/*
 * Publish a post through the native session.
 *
 * `reply_uri`/`reply_cid` name the post being answered (both or neither),
 * `quote_uri`/`quote_cid` the post being quoted. `gate` is NULL or a reply
 * gate name ("everyone" is the same as NULL); a gate on a reply is refused
 * before any request, as the bridge refuses it.
 *
 * WF_OK once the post record is written. If a gate was asked for and could
 * not be written, the post still stands and `*gate_failed` is 1. WF_ERR_NOT_FOUND
 * when the parent or quoted post is gone; nothing is written then. A failed
 * createRecord is never retried here: the service may have accepted it.
 */
wf_status platinum_atproto_publish(platinum_atproto_client *client,
                                   const char *utf8_text,
                                   const char *reply_uri,
                                   const char *reply_cid,
                                   const char *quote_uri,
                                   const char *quote_cid, const char *gate,
                                   int *gate_failed);

#ifdef __cplusplus
}
#endif

#endif
