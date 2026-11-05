/*
 * named_native.c -- Saved Feeds and My Lists without the bridge.
 *
 * The saved feeds are the custom feeds in app.bsky.actor.getPreferences. The
 * preference holds their AT URIs; the display names come from
 * app.bsky.feed.getFeedGenerators. The session layer allows at most
 * PLATINUM_ATPROTO_PARAMS_MAX parameters in one query and this call takes one
 * per URI, so a longer saved list is asked for in batches and the answers are
 * kept in order. My Lists is one app.bsky.graph.getLists call for the
 * signed-in DID.
 *
 * No Toolbox here, so it links on a host for tests. The window is people.c.
 */

#include "named_native.h"

#include <string.h>

#include "atproto_feed.h"
#include "atproto_prefs.h"
#include "atproto_url.h"
#include "json_min.h"

/* The saved feeds of the page being named. Static so a list of 25 URIs does
 * not sit on the small Mac stack. */
static platinum_saved_feeds named_feeds;

static void named_begin(platinum_people *people, int kind, const char *heading)
{
    people->kind = kind;
    people->path[0] = '\0';
    people->cursor[0] = '\0';
    people->count = 0;
    people->scroll_row = 0;
    people->selected = -1;
    people->heading[0] = '\0';
    if (heading != NULL) {
        strncpy(people->heading, heading, sizeof(people->heading) - 1);
        people->heading[sizeof(people->heading) - 1] = '\0';
    }
    people->loading = 1;
    platinum_people_set_status(people, "Loading...");
}

static void named_done(platinum_people *people)
{
    people->loading = 0;
    platinum_people_set_status(people,
                               people->count == 0 ? "Nobody to show." : NULL);
}

static wf_status named_fail(platinum_people *people, wf_status status,
                            const char *message)
{
    people->loading = 0;
    platinum_people_set_status(
        people, status == WF_ERR_AUTH ? "Session expired. Sign in again."
                                      : message);
    return status;
}

/* Read the saved feeds, then name them a batch at a time. A later batch that
 * fails keeps the rows already named and reports the failure. */
static wf_status named_feeds_page(platinum_people *people,
                                  platinum_atproto_client *client)
{
    wf_response response;
    platinum_json root;
    platinum_json generators;
    const char *uris[PLATINUM_ATPROTO_PARAMS_MAX];
    size_t offset;
    size_t batch;
    size_t index;
    long available;
    wf_status status;

    memset(&response, 0, sizeof(response));
    status = platinum_atproto_client_get(
        client, "app.bsky.actor.getPreferences", NULL, NULL, 0, &response);
    if (status != WF_OK) {
        wf_response_free(&response);
        return named_fail(people, status,
                          "The saved feeds could not be loaded.");
    }
    status = platinum_prefs_saved_feeds_parse(
        response.body != NULL ? response.body : "", &named_feeds);
    wf_response_free(&response);
    if (status != WF_OK)
        return named_fail(people, status,
                          "The server returned an invalid list.");

    if (named_feeds.count == 0) {
        named_done(people);
        return WF_OK;
    }

    for (offset = 0; offset < named_feeds.count;
         offset += PLATINUM_ATPROTO_PARAMS_MAX) {
        batch = (size_t)named_feeds.count - offset;
        if (batch > PLATINUM_ATPROTO_PARAMS_MAX)
            batch = PLATINUM_ATPROTO_PARAMS_MAX;
        for (index = 0; index < batch; index++)
            uris[index] = named_feeds.uris[offset + index];

        memset(&response, 0, sizeof(response));
        status = platinum_atproto_feed_generators(client, uris, batch,
                                                  &response);
        if (status != WF_OK) {
            wf_response_free(&response);
            return named_fail(people, status,
                              "The saved feeds could not be loaded.");
        }
        if (platinum_json_open(&root,
                               response.body != NULL ? response.body : "")
                != WF_OK ||
            platinum_json_member(root, "feeds", &generators) != WF_OK ||
            platinum_json_count(generators, &available) != WF_OK) {
            wf_response_free(&response);
            return named_fail(people, WF_ERR_PARSE,
                              "The server returned an invalid list.");
        }
        platinum_people_take_named(people, generators, "displayName");
        wf_response_free(&response);
    }

    named_done(people);
    return WF_OK;
}

wf_status platinum_named_feeds_native(platinum_people *people,
                                      platinum_atproto_client *client,
                                      const char *heading)
{
    if (people == NULL || client == NULL)
        return WF_ERR_INVALID_ARG;
    named_begin(people, PLATINUM_PEOPLE_FEEDS, heading);
    return named_feeds_page(people, client);
}

static wf_status named_lists_page(platinum_people *people,
                                  platinum_atproto_client *client)
{
    const platinum_atproto_session *session;
    wf_response response;
    platinum_json root;
    platinum_json lists;
    long available;
    wf_status status;

    session = platinum_atproto_client_session(client);
    if (session == NULL || !platinum_atproto_valid_did(session->did))
        return named_fail(people, WF_ERR_AUTH,
                          "The lists could not be loaded.");

    memset(&response, 0, sizeof(response));
    status = platinum_atproto_lists(client, session->did, 50, &response);
    if (status != WF_OK) {
        wf_response_free(&response);
        return named_fail(people, status, "The lists could not be loaded.");
    }
    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "lists", &lists) != WF_OK ||
        platinum_json_count(lists, &available) != WF_OK) {
        wf_response_free(&response);
        return named_fail(people, WF_ERR_PARSE,
                          "The server returned an invalid list.");
    }
    platinum_people_take_named(people, lists, "name");
    wf_response_free(&response);

    named_done(people);
    return WF_OK;
}

wf_status platinum_named_lists_native(platinum_people *people,
                                      platinum_atproto_client *client,
                                      const char *heading)
{
    if (people == NULL || client == NULL)
        return WF_ERR_INVALID_ARG;
    named_begin(people, PLATINUM_PEOPLE_LISTS, heading);
    return named_lists_page(people, client);
}
