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
    thread->path[0] = '\0';
    thread->cursor[0] = '\0';
    strcpy(thread->heading, "Thread");
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

/* One page of a plain list of posts. `append` adds to the end, dropping from
 * the front to stay within the bound, and leaves the list untouched on failure. */
static wf_status thread_fetch_list(platinum_thread *thread,
                                   platinum_bridge_client *bridge,
                                   int append,
                                   unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json posts;
    platinum_json item;
    char request[sizeof(thread->path) + 3 * 256];
    char cursor[sizeof(thread->cursor)];
    /* Static, not on the stack: 20 previews are about 26KB, and a Classic Mac
     * stack is small. Not re-entrant, which nothing here needs. */
    static platinum_post_preview parsed[PLATINUM_THREAD_PAGE];
    long available;
    long index;
    long got;
    long drop;
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;

    strcpy(request, thread->path);
    if (append) {
        strcat(request, "&cursor=");
        if (platinum_bridge_query_escape(thread->cursor, request + strlen(request),
                                         (long)(sizeof(request) - strlen(request))) < 0)
            return WF_ERR_INVALID_ARG;
    }

    memset(&response, 0, sizeof(response));
    thread->loading = 1;
    thread_status(thread, "Loading...");
    status = platinum_bridge_get(bridge, request, &response);
    if (status != WF_OK) {
        thread->loading = 0;
        if (status == WF_ERR_AUTH)
            thread_status(thread, "Session expired. Pair the account again.");
        else if (response.status == 404)
            thread_status(thread, "That account, feed or list no longer exists.");
        else
            thread_status(thread, "The posts could not be loaded.");
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "posts", &posts) != WF_OK ||
        platinum_json_count(posts, &available) != WF_OK) {
        thread->loading = 0;
        thread_status(thread, "The bridge returned an invalid list.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    /* Parse into scratch first, so a bad page never disturbs the list. */
    got = 0;
    for (index = 0; index < available && got < PLATINUM_THREAD_PAGE; ++index) {
        if (platinum_json_element(posts, index, &item) != WF_OK)
            continue;
        if (platinum_timeline_parse_post(&parsed[got], item))
            ++got;
    }
    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) != WF_OK)
        cursor[0] = '\0';
    wf_response_free(&response);

    if (!append) {
        thread->count = 0;
        thread->scroll_row = 0;
    }
    drop = (long)thread->count + got - PLATINUM_THREAD_MAX;
    if (drop > (long)thread->count)
        drop = (long)thread->count;
    if (drop > 0) {
        memmove(&thread->items[0], &thread->items[drop],
                (size_t)((long)thread->count - drop) * sizeof(thread->items[0]));
        thread->count = (unsigned short)((long)thread->count - drop);
        if (dropped != NULL)
            *dropped = (unsigned short)drop;
    }
    for (index = 0; index < got; ++index) {
        thread->items[thread->count].post = parsed[index];
        thread->items[thread->count].depth = 0;
        ++thread->count;
    }
    strcpy(thread->cursor, cursor);
    thread->truncated = 0;
    thread->focus = 0;
    thread->loading = 0;
    thread_status(thread, thread->count == 0 ? "No posts to show." : NULL);
    return WF_OK;
}

wf_status platinum_thread_load_list(platinum_thread *thread,
                                    platinum_bridge_client *bridge,
                                    const char *route,
                                    const char *key,
                                    const char *value,
                                    const char *heading)
{
    long length;

    if (thread == NULL || bridge == NULL || route == NULL || key == NULL ||
        value == NULL || value[0] == '\0')
        return WF_ERR_INVALID_ARG;

    strcpy(thread->path, route);
    strcat(thread->path, "?");
    strcat(thread->path, key);
    strcat(thread->path, "=");
    length = (long)strlen(thread->path);
    if (platinum_bridge_query_escape(value, thread->path + length,
                                     (long)sizeof(thread->path) - length) < 0) {
        thread->path[0] = '\0';
        return WF_ERR_INVALID_ARG;
    }
    thread->cursor[0] = '\0';
    thread->heading[0] = '\0';
    if (heading != NULL) {
        strncpy(thread->heading, heading, sizeof(thread->heading) - 1);
        thread->heading[sizeof(thread->heading) - 1] = '\0';
    }
    return thread_fetch_list(thread, bridge, 0, NULL);
}

int platinum_thread_has_more(const platinum_thread *thread)
{
    return thread != NULL && thread->path[0] != '\0' &&
           thread->cursor[0] != '\0' && thread->count > 0;
}

wf_status platinum_thread_load_more(platinum_thread *thread,
                                    platinum_bridge_client *bridge,
                                    unsigned short *dropped)
{
    if (dropped != NULL)
        *dropped = 0;
    if (thread == NULL || bridge == NULL || !platinum_thread_has_more(thread))
        return WF_ERR_INVALID_ARG;
    return thread_fetch_list(thread, bridge, 1, dropped);
}
