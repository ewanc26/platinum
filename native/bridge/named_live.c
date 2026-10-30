#include "named_live.h"

#include <string.h>

#include <wolfram/actor_prefs_typed.h>
#include <wolfram/agent.h>
#include <wolfram/feedgen_typed.h>
#include <wolfram/list_typed.h>
#include <wolfram/xrpc.h>

#define FEEDS_MAX 25
#define LISTS_MAX 50

/* Copies uri and name, clipping each to the view. The TypeScript clips the same
 * way (512 for a URI, 64 for a name). */
static void
add_named(const char *uri, const char *name, platinum_named_view *views, size_t max,
          size_t *count)
{
    platinum_named_view *view;
    size_t n;

    if (uri == NULL || uri[0] == '\0' || *count >= max) return;
    view = &views[*count];
    n = strlen(uri);
    if (n >= sizeof view->uri) n = sizeof view->uri - 1;
    memcpy(view->uri, uri, n);
    view->uri[n] = '\0';

    n = (name != NULL) ? strlen(name) : 0;
    if (n >= sizeof view->name) n = sizeof view->name - 1;
    if (n > 0) memcpy(view->name, name, n);
    view->name[n] = '\0';

    (*count)++;
}

platinum_named_result
platinum_named_feeds_live(void *ctx, platinum_named_view *views, size_t max, size_t *count)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_actor_preferences prefs;
    wf_agent_generator_view_list generators;
    wf_response response;
    const char *uris[FEEDS_MAX];
    size_t uri_count = 0;
    size_t i;
    wf_status status;

    if (agent == NULL || views == NULL || count == NULL) return PLATINUM_NAMED_ERROR;
    *count = 0;

    memset(&prefs, 0, sizeof prefs);
    if (wf_agent_get_actor_prefs_typed(agent, &prefs) != WF_OK) {
        wf_actor_preferences_free(&prefs);
        return PLATINUM_NAMED_ERROR;
    }

    for (i = 0; i < prefs.saved_feeds_v2.item_count && uri_count < FEEDS_MAX; i++) {
        const wf_actor_pref_saved_feed *item = &prefs.saved_feeds_v2.items[i];

        if (item->type != NULL && strcmp(item->type, "feed") == 0 && item->value != NULL) {
            uris[uri_count++] = item->value;
        }
    }
    /* A server that only sends the legacy preference still has saved feeds. */
    if (uri_count == 0) {
        for (i = 0; i < prefs.saved_feeds.pinned_count && uri_count < FEEDS_MAX; i++) {
            if (prefs.saved_feeds.pinned[i] != NULL) uris[uri_count++] = prefs.saved_feeds.pinned[i];
        }
        for (i = 0; i < prefs.saved_feeds.saved_count && uri_count < FEEDS_MAX; i++) {
            if (prefs.saved_feeds.saved[i] != NULL) uris[uri_count++] = prefs.saved_feeds.saved[i];
        }
    }
    if (uri_count == 0) {
        wf_actor_preferences_free(&prefs);
        return PLATINUM_NAMED_OK;
    }

    memset(&response, 0, sizeof response);
    status = wf_agent_get_feed_generators(agent, uris, uri_count, &response);
    /* The URIs point into prefs, so prefs stays alive until the call returns. */
    if (status != WF_OK) {
        wf_response_free(&response);
        wf_actor_preferences_free(&prefs);
        return PLATINUM_NAMED_ERROR;
    }

    memset(&generators, 0, sizeof generators);
    status = wf_agent_parse_generators(response.body, response.body_len, &generators);
    wf_response_free(&response);
    if (status != WF_OK) {
        wf_agent_generator_view_list_free(&generators);
        wf_actor_preferences_free(&prefs);
        return PLATINUM_NAMED_ERROR;
    }

    for (i = 0; i < generators.generator_count; i++) {
        add_named(generators.generators[i].uri, generators.generators[i].display_name,
                  views, max, count);
    }
    wf_agent_generator_view_list_free(&generators);
    wf_actor_preferences_free(&prefs);
    return PLATINUM_NAMED_OK;
}

platinum_named_result
platinum_named_lists_live(void *ctx, platinum_named_view *views, size_t max, size_t *count)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_list_view_list lists;
    const char *did;
    size_t i;

    if (agent == NULL || views == NULL || count == NULL) return PLATINUM_NAMED_ERROR;
    *count = 0;

    did = wf_agent_get_did(agent);
    if (did == NULL || did[0] == '\0') return PLATINUM_NAMED_ERROR;

    memset(&lists, 0, sizeof lists);
    if (wf_agent_get_lists_typed(agent, did, LISTS_MAX, NULL, &lists) != WF_OK) {
        wf_agent_list_view_list_free(&lists);
        return PLATINUM_NAMED_ERROR;
    }
    for (i = 0; i < lists.list_count; i++) {
        add_named(lists.lists[i].uri, lists.lists[i].name, views, max, count);
    }
    wf_agent_list_view_list_free(&lists);
    return PLATINUM_NAMED_OK;
}
