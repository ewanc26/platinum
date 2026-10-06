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
    char status[PLATINUM_THREAD_STATUS_MAX + 1];
    int loading;
} platinum_thread;

enum {
    PLATINUM_THREAD_NONE = 0,
    PLATINUM_THREAD_CLOSE = 1
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

/* The window; none of this runs on a host. */
OSErr platinum_thread_open(platinum_thread *thread);
void platinum_thread_close(platinum_thread *thread);
int platinum_thread_handle_event(platinum_thread *thread, EventRecord *event);
void platinum_thread_draw(platinum_thread *thread);

#ifdef __cplusplus
}
#endif

#endif
