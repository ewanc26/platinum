#include "search_live.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#include <wolfram/agent.h>
#include <wolfram/actor_typed.h>
#include <wolfram/feed_gen_typed.h>
#include <wolfram/muted_words.h>

#include "timeline_wolfram.h"

static void
add_optional(cJSON *obj, const char *key, const char *value)
{
    if (value != NULL && value[0] != '\0') cJSON_AddStringToObject(obj, key, value);
}

/* The search parser keeps the viewer state in `extra` (the postView fields the
 * struct does not name). Read like/repost from there, so a result draws its
 * liked and reposted flags as a timeline post does. Both are optional. */
static void
viewer_from_extra(const cJSON *extra, wf_agent_post_view *view)
{
    const cJSON *viewer;
    const cJSON *like;
    const cJSON *repost;

    if (extra == NULL || !cJSON_IsObject(extra)) return;
    viewer = cJSON_GetObjectItemCaseSensitive(extra, "viewer");
    if (!cJSON_IsObject(viewer)) return;
    like = cJSON_GetObjectItemCaseSensitive(viewer, "like");
    if (cJSON_IsString(like)) view->viewer.like = like->valuestring;
    repost = cJSON_GetObjectItemCaseSensitive(viewer, "repost");
    if (cJSON_IsString(repost)) view->viewer.repost = repost->valuestring;
}

/* Borrows the search post's fields in the shape the shared post mapper reads. */
static void
view_from_search(const wf_feedgen_search_post *post, wf_agent_post_view *view)
{
    memset(view, 0, sizeof *view);
    view->uri = post->uri;
    view->cid = post->cid;
    view->author = post->author;
    view->record = post->record;
    view->embed = post->embed;
    if (post->has_reply_count) view->reply_count = (int) post->reply_count;
    if (post->has_repost_count) view->repost_count = (int) post->repost_count;
    if (post->has_like_count) view->like_count = (int) post->like_count;
    if (post->has_quote_count) view->quote_count = (int) post->quote_count;
    view->indexed_at = post->indexed_at;
    viewer_from_extra(post->extra, view);
}

static platinum_search_result
search_posts_live(wf_agent *agent, const char *query, int count, const char *cursor,
                  char *body, size_t cap)
{
    wf_feedgen_search_result_list results;
    wf_muted_list muted;
    platinum_timeline_post *posts;
    size_t kept = 0;
    size_t i;
    int ok;

    memset(&results, 0, sizeof results);
    if (wf_feedgen_search_posts_typed(agent, query, count, cursor, &results) != WF_OK) {
        wf_feedgen_search_result_list_free(&results);
        return PLATINUM_SEARCH_ERROR;
    }

    posts = results.post_count > 0 ? calloc(results.post_count, sizeof *posts) : NULL;
    if (results.post_count > 0 && posts == NULL) {
        wf_feedgen_search_result_list_free(&results);
        return PLATINUM_SEARCH_ERROR;
    }

    platinum_muted_words_load_live(agent, &muted);
    for (i = 0; i < results.post_count; i++) {
        wf_agent_post_view view;

        view_from_search(&results.posts[i], &view);
        if (platinum_post_view_is_muted_live(&muted, &view)) continue;
        if (platinum_fill_post_live(&posts[kept], &view) != 0) continue;
        kept++;
    }

    ok = platinum_write_posts(posts, kept, results.cursor, body, cap) == 0;
    free(posts);
    wf_feedgen_search_result_list_free(&results);
    return ok ? PLATINUM_SEARCH_OK : PLATINUM_SEARCH_ERROR;
}

platinum_search_result
platinum_search_fetch_live(void *ctx, int posts, const char *query, int count,
                           const char *cursor, char *body, size_t cap)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_actor_list list;
    cJSON *root;
    cJSON *arr;
    size_t i;

    if (agent == NULL || body == NULL || cap == 0) return PLATINUM_SEARCH_ERROR;
    body[0] = '\0';

    if (posts) return search_posts_live(agent, query, count, cursor, body, cap);

    memset(&list, 0, sizeof list);
    if (wf_agent_search_actors_typed(agent, query, count, cursor, &list) != WF_OK) {
        wf_agent_actor_list_free(&list);
        return PLATINUM_SEARCH_ERROR;
    }

    root = cJSON_CreateObject();
    arr = cJSON_AddArrayToObject(root, "actors");
    if (root == NULL || arr == NULL) {
        cJSON_Delete(root);
        wf_agent_actor_list_free(&list);
        return PLATINUM_SEARCH_ERROR;
    }
    for (i = 0; i < list.actor_count; i++) {
        const wf_agent_profile_view *actor = &list.actors[i];
        cJSON *row;

        if (actor->did == NULL || actor->did[0] == '\0') continue;
        row = cJSON_CreateObject();
        if (row == NULL) continue;
        cJSON_AddStringToObject(row, "did", actor->did);
        add_optional(row, "handle", actor->handle);
        add_optional(row, "displayName", actor->display_name);
        cJSON_AddItemToArray(arr, row);
    }
    if (list.cursor != NULL && list.cursor[0] != '\0') {
        cJSON_AddStringToObject(root, "cursor", list.cursor);
    }

    if (!cJSON_PrintPreallocated(root, body, (int) cap, 0)) {
        if (cap > 0) body[0] = '\0';
        cJSON_Delete(root);
        wf_agent_actor_list_free(&list);
        return PLATINUM_SEARCH_ERROR;
    }

    cJSON_Delete(root);
    wf_agent_actor_list_free(&list);
    return PLATINUM_SEARCH_OK;
}
