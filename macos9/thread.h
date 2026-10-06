#ifndef PLATINUM_THREAD_H
#define PLATINUM_THREAD_H

#include "bridge_client.h"
#include "scrollbar.h"
#include "timeline.h"

#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The bridge bounds a thread at 40 posts, 6 reply levels and 10 ancestors. */
#define PLATINUM_THREAD_MAX 40
#define PLATINUM_THREAD_PAGE 20
#define PLATINUM_THREAD_MIN_DEPTH (-10)
#define PLATINUM_THREAD_MAX_DEPTH 6
#define PLATINUM_THREAD_STATUS_MAX 127

typedef struct platinum_thread_post {
    platinum_post_preview post;
    short depth; /* < 0 ancestor, 0 the post asked for, > 0 a reply */
} platinum_thread_post;

typedef struct platinum_thread {
    WindowPtr window;
    platinum_scrollbar scrollbar;
    platinum_thread_post items[PLATINUM_THREAD_MAX];
    unsigned short count;
    short scroll_row;
    short focus;     /* index of the depth-0 post, or 0 */
    int truncated;   /* replies were left out to stay within the bound */
    /* When this shows a plain list of posts (an author's posts, a feed, search
     * results) rather than a thread, the request without its cursor, and the
     * cursor for the next page. */
    char path[640];
    char cursor[256];
    char heading[96];
    char status[PLATINUM_THREAD_STATUS_MAX + 1];
    int loading;
} platinum_thread;

enum {
    PLATINUM_THREAD_NONE = 0,
    PLATINUM_THREAD_CLOSE = 1,
    PLATINUM_THREAD_LOAD_MORE = 2
};

void platinum_thread_init(platinum_thread *thread);

/*
 * GET /v1/thread for the post `uri`, replacing the list. A response that is not
 * a thread, or has no depth-0 post, leaves the list empty and says so; a post
 * that cannot be read is skipped. The list is bounded at PLATINUM_THREAD_MAX.
 */
wf_status platinum_thread_load(platinum_thread *thread,
                               platinum_bridge_client *bridge,
                               const char *uri);

/*
 * Load a plain list of posts (no depths) from `route`, such as
 * "/v1/author-feed" with key "actor", replacing the list. `heading` is shown
 * above it. Pages with platinum_thread_load_more; at most PLATINUM_THREAD_MAX
 * posts are kept, dropping the first when over.
 */
wf_status platinum_thread_load_list(platinum_thread *thread,
                                    platinum_bridge_client *bridge,
                                    const char *route,
                                    const char *key,
                                    const char *value,
                                    const char *heading);
wf_status platinum_thread_load_more(platinum_thread *thread,
                                    platinum_bridge_client *bridge,
                                    unsigned short *dropped);
int platinum_thread_has_more(const platinum_thread *thread);

/* The window; none of this runs on a host. */
OSErr platinum_thread_open(platinum_thread *thread);
void platinum_thread_close(platinum_thread *thread);
int platinum_thread_handle_event(platinum_thread *thread, EventRecord *event);
void platinum_thread_draw(platinum_thread *thread);

#ifdef __cplusplus
}
#endif

#endif
