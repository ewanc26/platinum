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

#include <wolfram/agent.h>
#include <wolfram/muted_words.h>

#include "timeline_route.h"

/* A platinum_timeline_fetch_fn whose ctx is a wf_agent *. The posts are malloc'd
 * by this function and released by platinum_timeline_free. */
platinum_timeline_result platinum_timeline_fetch_live(void *ctx, int limit,
                                                      const char *cursor,
                                                      platinum_timeline *out);

/* The author feed, in the same shape and with the same muted-word rule. ctx is a
 * wf_agent *; actor is a validated handle or DID. */
platinum_timeline_result platinum_author_feed_fetch_live(void *ctx, const char *actor,
                                                         int limit, const char *cursor,
                                                         platinum_timeline *out);

/* Fills one bridge post model from a parsed postView, applying the same text,
 * image and card mapping as a timeline page. The view is borrowed, not owned.
 * Shared with the thread flattener. Returns 0, or -1 when the display model
 * cannot read the post. */
struct wf_agent_post_view;
int platinum_fill_post_live(platinum_timeline_post *post,
                            const struct wf_agent_post_view *view);

/* The account's muted words, as the timeline filter reads them. A preferences
 * failure leaves the list empty, so a page is shown unfiltered rather than not
 * at all. Shared with search. */
void platinum_muted_words_load_live(wf_agent *agent, wf_muted_list *out);

/* True when a muted word hides this post, using its validated tag facets the way
 * the AppView's own matcher reads them. Shared with search. */
int platinum_post_view_is_muted_live(const wf_muted_list *muted,
                                     const struct wf_agent_post_view *view);

#endif
