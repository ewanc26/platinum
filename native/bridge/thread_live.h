/*
 * The live thread fetch: Wolfram's typed getPostThread tree, flattened to the
 * Mac's bounded row list.
 */
#ifndef PLATINUM_NATIVE_THREAD_LIVE_H
#define PLATINUM_NATIVE_THREAD_LIVE_H

#include "thread_route.h"

/* platinum_thread_fn. ctx is a wf_agent *. Returns NOT_FOUND when the thread has
 * no post at its root (the AppView's notFoundPost, or a deleted post). */
platinum_thread_result platinum_thread_fetch_live(void *ctx, const char *uri,
                                                  platinum_thread_entry *entries,
                                                  size_t max, size_t *count,
                                                  int *truncated);

#endif
