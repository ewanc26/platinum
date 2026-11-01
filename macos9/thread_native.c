/*
 * thread_native.c -- the thread's native page fetch.
 *
 * Kept apart from thread_feed.c so the bridge path and its tests do not pull
 * in the session stack. It speaks app.bsky.feed.getPostThread through the
 * session client and flattens the reply tree the way the bridge did
 * server-side: ancestors oldest first with a negative depth, the post asked
 * for at depth 0, then replies depth-first. At most 40 posts, replies no
 * deeper than six, ten ancestors; blocked, missing and unreadable posts are
 * skipped, and `truncated` says whether anything was left out.
 */

#include "thread.h"

#include <string.h>

#include "atproto_url.h"

static void thread_native_status(platinum_thread *thread, const char *status)
{
    long length;

    if (thread == NULL)
        return;
    thread->status[0] = '\0';
    if (status == NULL)
        return;
    length = (long)strlen(status);
    if (length > PLATINUM_THREAD_STATUS_MAX)
        length = PLATINUM_THREAD_STATUS_MAX;
    memcpy(thread->status, status, (size_t)length);
    thread->status[length] = '\0';
}

/* A reply-tree node counts only when it is a threadViewPost with a post. A
 * missing $type is accepted when a post is present; blocked and not-found
 * nodes always carry their own $type and are skipped with their subtree. */
static int thread_native_is_post(platinum_json node)
{
    char type[64];
    platinum_json post;

    if (platinum_json_string(node, "$type", type, sizeof(type)) == WF_OK) {
        return strcmp(type, "app.bsky.feed.defs#threadViewPost") == 0;
    }
    return platinum_json_member(node, "post", &post) == WF_OK;
}

static int thread_native_emit(platinum_thread *thread, platinum_json node,
                              short depth)
{
    platinum_json post_view;

    if (thread == NULL)
        return 0;
    if (!thread_native_is_post(node))
        return 0;
    if (thread->count >= PLATINUM_THREAD_MAX)
        return 0;
    if (platinum_json_member(node, "post", &post_view) != WF_OK)
        return 0;
    if (!platinum_timeline_parse_view(&thread->items[thread->count].post,
                                      post_view))
        return 0;
    thread->items[thread->count].depth = depth;
    ++thread->count;
    return 1;
}

static void thread_native_replies(platinum_thread *thread, platinum_json node,
                                  short depth, int *truncated)
{
    platinum_json replies;
    platinum_json reply;
    long count;
    long index;

    if (thread == NULL || truncated == NULL)
        return;
    if (platinum_json_member(node, "replies", &replies) != WF_OK)
        return;
    if (platinum_json_count(replies, &count) != WF_OK)
        return;
    for (index = 0; index < count; ++index) {
        if (thread->count >= PLATINUM_THREAD_MAX ||
            depth > PLATINUM_THREAD_MAX_DEPTH) {
            *truncated = 1;
            return;
        }
        if (platinum_json_element(replies, index, &reply) != WF_OK)
            continue;
        if (!thread_native_is_post(reply))
            continue;
        (void)thread_native_emit(thread, reply, depth);
        thread_native_replies(thread, reply, (short)(depth + 1), truncated);
        if (*truncated && thread->count >= PLATINUM_THREAD_MAX)
            return;
    }
}

wf_status platinum_thread_load_native(platinum_thread *thread,
                                      platinum_atproto_client *client,
                                      const char *uri)
{
    const char *names[2];
    const char *values[2];
    wf_response response;
    platinum_json root;
    platinum_json node;
    platinum_json parent;
    platinum_json ancestors[10];
    long ancestor_count;
    long index;
    int truncated;
    int root_is_post;
    wf_status status;

    if (thread == NULL || client == NULL || uri == NULL || uri[0] == '\0')
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_at_uri(uri))
        return WF_ERR_INVALID_ARG;

    thread->count = 0;
    thread->scroll_row = 0;
    thread->focus = 0;
    thread->truncated = 0;
    thread->path[0] = '\0';
    thread->cursor[0] = '\0';
    strcpy(thread->heading, "Thread");
    thread->loading = 1;
    thread_native_status(thread, "Loading thread...");
    memset(&response, 0, sizeof(response));

    names[0] = "uri";
    values[0] = uri;
    names[1] = "depth";
    values[1] = "6";
    status = platinum_atproto_client_get(client,
                                         "app.bsky.feed.getPostThread",
                                         names, values, 2, &response);
    if (status != WF_OK) {
        thread->loading = 0;
        if (status == WF_ERR_AUTH)
            thread_native_status(thread, "Session expired. Sign in again.");
        else
            thread_native_status(thread, "The thread could not be loaded.");
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "thread", &node) != WF_OK) {
        thread->loading = 0;
        thread_native_status(thread,
                             "The server returned an invalid thread.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    truncated = 0;
    ancestor_count = 0;
    while (platinum_json_member(node, "parent", &parent) == WF_OK) {
        if (!thread_native_is_post(parent))
            break;
        if (ancestor_count >= 10) {
            truncated = 1;
            break;
        }
        ancestors[ancestor_count] = parent;
        ++ancestor_count;
        node = parent;
    }
    for (index = ancestor_count; index > 0; --index) {
        (void)thread_native_emit(thread, ancestors[index - 1],
                                 (short)(-(index)));
    }

    if (platinum_json_member(root, "thread", &node) != WF_OK) {
        thread->loading = 0;
        thread_native_status(thread,
                             "The server returned an invalid thread.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }
    root_is_post = thread_native_is_post(node);
    if (root_is_post)
        (void)thread_native_emit(thread, node, 0);

    if (!root_is_post && thread->count == 0) {
        thread->loading = 0;
        thread_native_status(thread, "That post no longer exists.");
        wf_response_free(&response);
        return WF_ERR_NOT_FOUND;
    }

    if (root_is_post)
        thread_native_replies(thread, node, 1, &truncated);

    wf_response_free(&response);
    thread->loading = 0;
    thread->truncated = truncated;

    if (thread->count == 0) {
        thread_native_status(thread, "That post no longer exists.");
        return WF_ERR_NOT_FOUND;
    }
    thread->focus = 0;
    for (index = 0; index < (long)thread->count; ++index) {
        if (thread->items[index].depth == 0) {
            thread->focus = (short)index;
            break;
        }
    }
    thread->scroll_row = thread->focus;
    thread_native_status(thread, truncated ? "Some replies are not shown."
                                           : NULL);
    return WF_OK;
}
