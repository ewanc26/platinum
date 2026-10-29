#include "collection_live.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#include <wolfram/actor_typed.h>
#include <wolfram/agent.h>
#include <wolfram/feed_typed.h>
#include <wolfram/graph_typed.h>

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
