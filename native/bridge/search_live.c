#include "search_live.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#include <wolfram/agent.h>
#include <wolfram/actor_typed.h>

static void
add_optional(cJSON *obj, const char *key, const char *value)
{
    if (value != NULL && value[0] != '\0') cJSON_AddStringToObject(obj, key, value);
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
    /* Post search needs the timeline post mapping; until that is shared, answer
     * with an error the route reports as 500 rather than an empty page. */
    if (posts) return PLATINUM_SEARCH_ERROR;

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
