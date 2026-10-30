/*
 * The live searches: Wolfram's searchActors, adapted to the search route.
 *
 * actor search returns the bridge's account list shape, {"actors":[...],"cursor"}.
 * Post search is not wired yet and reports an error, which the route turns into
 * a well-formed 500 rather than a result with no posts in it.
 */
#ifndef PLATINUM_NATIVE_SEARCH_LIVE_H
#define PLATINUM_NATIVE_SEARCH_LIVE_H

#include "search_routes.h"

/* A platinum_search_fn whose ctx is a wf_agent *. */
platinum_search_result platinum_search_fetch_live(void *ctx, int posts, const char *query,
                                                  int count, const char *cursor,
                                                  char *body, size_t cap);

#endif
