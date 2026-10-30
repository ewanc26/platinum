/*
 * The live fetches behind GET /v1/feeds and GET /v1/lists: Wolfram's
 * getPreferences + getFeedGenerators, and getLists for the signed-in account.
 */
#ifndef PLATINUM_NATIVE_NAMED_LIVE_H
#define PLATINUM_NATIVE_NAMED_LIVE_H

#include "named_routes.h"

/* The account's saved custom feeds, named and capped at 25. The home timeline
 * and lists in the saved-feeds preference are left out, as server.ts does. */
platinum_named_result platinum_named_feeds_live(void *ctx, platinum_named_view *views,
                                                size_t max, size_t *count);

/* The account's own lists, capped at 50. ctx is a wf_agent *. */
platinum_named_result platinum_named_lists_live(void *ctx, platinum_named_view *views,
                                                size_t max, size_t *count);

#endif
