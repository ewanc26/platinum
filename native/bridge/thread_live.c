#include "thread_live.h"

#include <string.h>

#include <wolfram/agent.h>
#include <wolfram/thread_typed.h>

#include "timeline_wolfram.h"

/* Marks truncated and stops the walk when the list is full or too deep, as the
 * TypeScript's walk does. Returns 1 when the caller must stop. */
static int
stop_here(size_t count, size_t max, int depth)
{
    return count >= max || depth > PLATINUM_THREAD_DEPTH_MAX;
}

/* Appends one node's post, unless the node is blocked or missing. */
static void
emit_node(const wf_agent_thread_node *node, int depth, platinum_thread_entry *entries,
          size_t max, size_t *count)
{
    wf_agent_post_view view;
    platinum_thread_entry *entry;

    if (node->kind != WF_AGENT_THREAD_KIND_POST || *count >= max) return;
    if (node->post.uri == NULL || node->post.uri[0] == '\0') return;

    memset(&view, 0, sizeof view);
    view.uri = node->post.uri;
    view.cid = node->post.cid;
    view.author = node->post.author;
    view.record = node->post.record;
    view.embed = node->post.embed;
    view.reply_count = node->post.reply_count;
    view.repost_count = node->post.repost_count;
    view.like_count = node->post.like_count;
    view.quote_count = node->post.quote_count;
    view.indexed_at = node->post.indexed_at;
    view.viewer.like = node->post.viewer_like;
    view.viewer.repost = node->post.viewer_repost;

    entry = &entries[*count];
    if (platinum_fill_post_live(&entry->post, &view) != 0) return;
    entry->depth = depth;
    (*count)++;
}

static void
walk_replies(const wf_agent_thread_node *node, int depth, platinum_thread_entry *entries,
             size_t max, size_t *count, int *truncated)
{
    size_t i;

    for (i = 0; i < node->replies_count; i++) {
        const wf_agent_thread_node *reply = &node->replies[i];

        if (reply->kind != WF_AGENT_THREAD_KIND_POST) continue;
        if (stop_here(*count, max, depth)) {
            *truncated = 1;
            return;
        }
        emit_node(reply, depth, entries, max, count);
        walk_replies(reply, depth + 1, entries, max, count, truncated);
    }
}

platinum_thread_result
platinum_thread_fetch_live(void *ctx, const char *uri, platinum_thread_entry *entries,
                           size_t max, size_t *count, int *truncated)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_thread thread;
    const wf_agent_thread_node *ancestors[10];
    size_t ancestor_count = 0;
    const wf_agent_thread_node *node;
    size_t i;

    if (agent == NULL || uri == NULL || entries == NULL || count == NULL ||
        truncated == NULL) {
        return PLATINUM_THREAD_ERROR;
    }
    *count = 0;
    *truncated = 0;

    memset(&thread, 0, sizeof thread);
    if (wf_agent_get_post_thread_typed(agent, uri, PLATINUM_THREAD_DEPTH_MAX, &thread) !=
        WF_OK) {
        wf_agent_thread_free(&thread);
        return PLATINUM_THREAD_ERROR;
    }

    /* The parser gives the nearest parent, and each parent its own. Walk up to
     * the oldest, keeping the ten the Mac may draw above the post. */
    node = thread.root.parent;
    while (node != NULL && node->kind == WF_AGENT_THREAD_KIND_POST) {
        if (ancestor_count == sizeof ancestors / sizeof ancestors[0]) {
            *truncated = 1;
            break;
        }
        ancestors[ancestor_count++] = node;
        node = node->parent;
    }
    for (i = ancestor_count; i > 0; i--) {
        emit_node(ancestors[i - 1], -(int) i, entries, max, count);
    }

    if (thread.root.kind != WF_AGENT_THREAD_KIND_POST) {
        wf_agent_thread_free(&thread);
        return (*count == 0) ? PLATINUM_THREAD_NOT_FOUND : PLATINUM_THREAD_OK;
    }

    emit_node(&thread.root, 0, entries, max, count);
    walk_replies(&thread.root, 1, entries, max, count, truncated);

    wf_agent_thread_free(&thread);
    return PLATINUM_THREAD_OK;
}
