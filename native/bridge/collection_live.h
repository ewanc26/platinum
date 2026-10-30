/*
 * The live collection fetches behind the actor-graph, engagement and author-feed
 * routes: Wolfram's getFollows, getFollowers, getAuthorFeed, getLikes and
 * getRepostedBy, adapted to the route callbacks in collection_routes.h.
 */
#ifndef PLATINUM_NATIVE_COLLECTION_LIVE_H
#define PLATINUM_NATIVE_COLLECTION_LIVE_H

#include "collection_routes.h"

/* platinum_collection_fn for /v1/follows (kind 0), /v1/followers (1) and
 * /v1/author-feed (2). ctx is a wf_agent *; key is the validated actor. */
platinum_collection_result platinum_collection_graph_live(void *ctx, int kind,
                                                          const char *key, int count,
                                                          const char *cursor,
                                                          char *body, size_t cap);

/* platinum_collection_fn for /v1/post/likes (kind 0) and /v1/post/reposts (1).
 * ctx is a wf_agent *; key is the validated post URI. */
platinum_collection_result platinum_collection_engagement_live(void *ctx, int kind,
                                                               const char *key, int count,
                                                               const char *cursor,
                                                               char *body, size_t cap);

#endif
