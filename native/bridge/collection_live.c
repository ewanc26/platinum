#include "collection_live.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#include <wolfram/actor_typed.h>
#include <wolfram/agent.h>
#include <wolfram/feed_typed.h>
#include <wolfram/feedgen_typed.h>
#include <wolfram/graph_typed.h>
#include <wolfram/list_typed.h>

#include "timeline_wolfram.h"

/* Adds one actor row; optional fields are left out, as api.ts does. The DID is
 * required, so a row without one is dropped rather than drawn blank. */
static void
add_actor(cJSON *arr, const wf_agent_profile_view *actor)
{
    cJSON *row;

    if (actor->did == NULL || actor->did[0] == '\0') return;
    row = cJSON_CreateObject();
    if (row == NULL) return;
    cJSON_AddStringToObject(row, "did", actor->did);
    if (actor->handle != NULL && actor->handle[0] != '\0') {
        cJSON_AddStringToObject(row, "handle", actor->handle);
    }
    if (actor->display_name != NULL && actor->display_name[0] != '\0') {
        cJSON_AddStringToObject(row, "displayName", actor->display_name);
    }
    cJSON_AddItemToArray(arr, row);
}

/* Prints {"actors":[...],"cursor"} and frees the tree. */
static int
finish_actors(cJSON *root, const char *cursor, char *body, size_t cap)
{
    int ok;

    if (root == NULL || body == NULL || cap == 0) {
        cJSON_Delete(root);
        return -1;
    }
    if (cursor != NULL && cursor[0] != '\0') {
        cJSON_AddStringToObject(root, "cursor", cursor);
    }
    ok = cJSON_PrintPreallocated(root, body, (int) cap, 0);
    cJSON_Delete(root);
    if (!ok) {
        if (cap > 0) body[0] = '\0';
        return -1;
    }
    return 0;
}

static int
write_actor_list(const wf_agent_profile_view *actors, size_t count, const char *cursor,
                 char *body, size_t cap)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(root, "actors");
    size_t i;

    if (root == NULL || arr == NULL) {
        cJSON_Delete(root);
        return -1;
    }
    for (i = 0; i < count; i++) add_actor(arr, &actors[i]);
    return finish_actors(root, cursor, body, cap);
}

platinum_collection_result
platinum_collection_graph_live(void *ctx, int kind, const char *key, int count,
                               const char *cursor, char *body, size_t cap)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_actor_list list;
    wf_agent_feed_list feed;

    if (agent == NULL || key == NULL) return PLATINUM_COLLECTION_ERROR;

    if (kind == 2) {
        platinum_timeline page;
        platinum_timeline_result r;

        memset(&page, 0, sizeof page);
        r = platinum_author_feed_fetch_live(agent, key, count, cursor, &page);
        if (r != PLATINUM_TIMELINE_OK) {
            platinum_timeline_free(&page);
            return PLATINUM_COLLECTION_ERROR;
        }
        if (platinum_write_posts(page.posts, page.post_count,
                                 page.has_cursor ? page.cursor : NULL, body, cap) != 0) {
            platinum_timeline_free(&page);
            return PLATINUM_COLLECTION_ERROR;
        }
        platinum_timeline_free(&page);
        return PLATINUM_COLLECTION_OK;
    }

    (void) feed;
    memset(&list, 0, sizeof list);
    if (kind == 0) {
        if (wf_agent_get_follows_typed(agent, key, count, cursor, &list) != WF_OK) {
            wf_agent_actor_list_free(&list);
            return PLATINUM_COLLECTION_ERROR;
        }
    } else if (kind == 1) {
        if (wf_agent_get_followers_typed(agent, key, count, cursor, &list) != WF_OK) {
            wf_agent_actor_list_free(&list);
            return PLATINUM_COLLECTION_ERROR;
        }
    } else {
        return PLATINUM_COLLECTION_ERROR;
    }

    if (write_actor_list(list.actors, list.actor_count, list.cursor, body, cap) != 0) {
        wf_agent_actor_list_free(&list);
        return PLATINUM_COLLECTION_ERROR;
    }
    wf_agent_actor_list_free(&list);
    return PLATINUM_COLLECTION_OK;
}

platinum_collection_result
platinum_collection_engagement_live(void *ctx, int kind, const char *key, int count,
                                    const char *cursor, char *body, size_t cap)
{
    wf_agent *agent = (wf_agent *) ctx;

    if (agent == NULL || key == NULL) return PLATINUM_COLLECTION_ERROR;

    if (kind == 0) {
        wf_agent_actor_like_list likes;
        cJSON *root;
        cJSON *arr;
        size_t i;

        memset(&likes, 0, sizeof likes);
        if (wf_agent_get_likes_typed(agent, key, count, cursor, &likes) != WF_OK) {
            wf_agent_actor_like_list_free(&likes);
            return PLATINUM_COLLECTION_ERROR;
        }
        root = cJSON_CreateObject();
        arr = cJSON_AddArrayToObject(root, "actors");
        if (root == NULL || arr == NULL) {
            cJSON_Delete(root);
            wf_agent_actor_like_list_free(&likes);
            return PLATINUM_COLLECTION_ERROR;
        }
        for (i = 0; i < likes.like_count; i++) add_actor(arr, &likes.likes[i].actor);
        if (finish_actors(root, likes.cursor, body, cap) != 0) {
            wf_agent_actor_like_list_free(&likes);
            return PLATINUM_COLLECTION_ERROR;
        }
        wf_agent_actor_like_list_free(&likes);
        return PLATINUM_COLLECTION_OK;
    }

    if (kind == 1) {
        wf_agent_actor_list list;

        memset(&list, 0, sizeof list);
        if (wf_agent_get_reposted_by_typed(agent, key, count, cursor, &list) != WF_OK) {
            wf_agent_actor_list_free(&list);
            return PLATINUM_COLLECTION_ERROR;
        }
        if (write_actor_list(list.actors, list.actor_count, list.cursor, body, cap) != 0) {
            wf_agent_actor_list_free(&list);
            return PLATINUM_COLLECTION_ERROR;
        }
        wf_agent_actor_list_free(&list);
        return PLATINUM_COLLECTION_OK;
    }

    return PLATINUM_COLLECTION_ERROR;
}

/* getFeed: a page of posts in the timeline shape, with muted words applied. */
static platinum_collection_result
feed_page(wf_agent *agent, const wf_response *res, char *body, size_t cap)
{
    wf_agent_feed_view_list page;
    platinum_timeline posts;
    platinum_collection_result out = PLATINUM_COLLECTION_ERROR;

    memset(&page, 0, sizeof page);
    if (wf_agent_parse_feed_views(res->body, res->body_len, &page) != WF_OK) {
        wf_agent_feed_view_list_free(&page);
        return PLATINUM_COLLECTION_ERROR;
    }
    memset(&posts, 0, sizeof posts);
    if (platinum_timeline_fill_live(agent, &page, &posts) == PLATINUM_TIMELINE_OK &&
        platinum_write_posts(posts.posts, posts.post_count,
                             posts.has_cursor ? posts.cursor : NULL, body, cap) == 0) {
        out = PLATINUM_COLLECTION_OK;
    }
    platinum_timeline_free(&posts);
    wf_agent_feed_view_list_free(&page);
    return out;
}

/* getList: the list's members, as actor rows. */
static platinum_collection_result
list_page(const wf_response *res, char *body, size_t cap)
{
    wf_agent_list_item_list items;
    cJSON *root;
    cJSON *arr;
    size_t i;
    int rc;

    memset(&items, 0, sizeof items);
    if (wf_agent_parse_list_items(res->body, res->body_len, &items) != WF_OK) {
        wf_agent_list_item_list_free(&items);
        return PLATINUM_COLLECTION_ERROR;
    }
    root = cJSON_CreateObject();
    arr = cJSON_AddArrayToObject(root, "actors");
    if (root == NULL || arr == NULL) {
        cJSON_Delete(root);
        wf_agent_list_item_list_free(&items);
        return PLATINUM_COLLECTION_ERROR;
    }
    for (i = 0; i < items.item_count; i++) add_actor(arr, &items.items[i].subject);
    rc = finish_actors(root, items.cursor, body, cap);
    wf_agent_list_item_list_free(&items);
    return rc == 0 ? PLATINUM_COLLECTION_OK : PLATINUM_COLLECTION_ERROR;
}

platinum_collection_result
platinum_collection_named_live(void *ctx, int kind, const char *key, int count,
                               const char *cursor, char *body, size_t cap)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_response res;
    wf_status st;
    platinum_collection_result out;

    if (agent == NULL || key == NULL || (kind != 0 && kind != 1)) {
        return PLATINUM_COLLECTION_ERROR;
    }

    /* The raw call keeps the HTTP status: api.ts answers a 400 from getFeed or
     * getList (an unknown feed or list) as "no such feed or list". */
    memset(&res, 0, sizeof res);
    st = kind == 0 ? wf_agent_get_feed(agent, key, count, cursor, &res)
                   : wf_agent_get_list(agent, key, count, cursor, &res);
    if (st == WF_ERR_HTTP && res.status == 400) {
        wf_response_free(&res);
        return PLATINUM_COLLECTION_NOT_FOUND;
    }
    if (st != WF_OK || res.body == NULL) {
        wf_response_free(&res);
        return PLATINUM_COLLECTION_ERROR;
    }
    out = kind == 0 ? feed_page(agent, &res, body, cap) : list_page(&res, body, cap);
    wf_response_free(&res);
    return out;
}
