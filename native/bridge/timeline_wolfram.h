/*
 * The live timeline fetch: Wolfram's getTimeline, adapted to the route (Part 8,
 * the home feed slice). Built as C2x against Wolfram's headers and linked to
 * libwolfram; the route itself does not change.
 *
 * Muted words are applied here, before the page reaches the route, so a page can
 * come back short while its cursor still moves on. The list is read from the
 * account's preferences once per fetch; a failure shows the page unfiltered
 * rather than empty, which is what the Node bridge does.
 */
#ifndef PLATINUM_NATIVE_TIMELINE_WOLFRAM_H
#define PLATINUM_NATIVE_TIMELINE_WOLFRAM_H

#include "timeline_route.h"

/* A platinum_timeline_fetch_fn whose ctx is a wf_agent *. The posts are malloc'd
 * by this function and released by platinum_timeline_free. */
platinum_timeline_result platinum_timeline_fetch_live(void *ctx, int limit,
                                                      const char *cursor,
                                                      platinum_timeline *out);

#endif
