/*
 * thread_feed.c -- loading and parsing a thread. No QuickDraw here, so it links
 * on a host for tests; the window that draws it is thread.c.
 */
#include "thread.h"

#include <string.h>

static void thread_status(platinum_thread *thread, const char *status)
{
    long length;

    thread->status[0] = '\0';
    if (status == NULL)
        return;
    length = (long)strlen(status);
    if (length > PLATINUM_THREAD_STATUS_MAX)
        length = PLATINUM_THREAD_STATUS_MAX;
    memcpy(thread->status, status, (size_t)length);
    thread->status[length] = '\0';
}

void platinum_thread_init(platinum_thread *thread)
{
    if (thread == NULL)
        return;
    memset(thread, 0, sizeof(*thread));
    thread_status(thread, "No thread loaded.");
}

wf_status platinum_thread_load(platinum_thread *thread,
                               platinum_bridge_client *bridge,
                               const char *uri)
{
    wf_response response;
    platinum_json root;
    platinum_json posts;
    platinum_json item;
    char path[64 + 3 * 512];
    long available;
    long index;
    long depth;
    int flag;
    wf_status status;

    if (thread == NULL || bridge == NULL || uri == NULL || uri[0] == '\0')
        return WF_ERR_INVALID_ARG;

    strcpy(path, "/v1/thread?uri=");
    if (platinum_bridge_query_escape(uri, path + strlen(path),
                                     (long)(sizeof(path) - strlen(path))) < 0)
        return WF_ERR_INVALID_ARG;

    thread->count = 0;
    thread->scroll_row = 0;
    thread->focus = 0;
    thread->truncated = 0;
    thread->loading = 1;
    thread_status(thread, "Loading thread...");
    memset(&response, 0, sizeof(response));

    status = platinum_bridge_get(bridge, path, &response);
    if (status != WF_OK) {
        thread->loading = 0;
        if (status == WF_ERR_AUTH)
            thread_status(thread, "Session expired. Pair the account again.");
        else if (response.status == 404)
            thread_status(thread, "That post no longer exists.");
        else
            thread_status(thread, "The thread could not be loaded.");
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "posts", &posts) != WF_OK ||
        platinum_json_count(posts, &available) != WF_OK) {
        thread->loading = 0;
        thread_status(thread, "The bridge returned an invalid thread.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    if (platinum_json_bool(root, "truncated", &flag) == WF_OK)
        thread->truncated = flag;

    for (index = 0; index < available &&
                    thread->count < PLATINUM_THREAD_MAX; ++index) {
        if (platinum_json_element(posts, index, &item) != WF_OK)
            continue;
        if (platinum_json_int(item, "depth", &depth) != WF_OK ||
            depth < PLATINUM_THREAD_MIN_DEPTH ||
            depth > PLATINUM_THREAD_MAX_DEPTH)
            continue;
        if (!platinum_timeline_parse_post(&thread->items[thread->count].post,
                                          item))
            continue;
        thread->items[thread->count].depth = (short)depth;
        if (depth == 0)
            thread->focus = (short)thread->count;
        ++thread->count;
    }
    wf_response_free(&response);
    thread->loading = 0;

    if (thread->count == 0) {
        thread_status(thread, "The thread is empty.");
        return WF_OK;
    }
    thread->scroll_row = thread->focus;
    thread_status(thread, thread->truncated
                              ? "Some replies are not shown."
                              : NULL);
    return WF_OK;
}
