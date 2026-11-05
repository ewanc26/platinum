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
#include <time.h>

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

/* ---- a saved feed's posts ---------------------------------------------- */

static int named_view_is_muted(platinum_json post_view, void *userdata)
{
    return platinum_timeline_view_is_muted(
        post_view, (const platinum_muted_list *)userdata);
}

static wf_status named_feed_page(platinum_thread *thread,
                                 platinum_atproto_client *client,
                                 int append, unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json feed;
    platinum_muted_list muted;
    char cursor[sizeof(thread->cursor)];
    long available;
    long now;
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;

    /* Best-effort, as the timeline is: a preferences failure shows the page
     * unfiltered. The prefs fetch runs first so the feed remains the last
     * request the tests observe. */
    platinum_muted_clear(&muted);
    now = (long)time(NULL);
    if (now < 0)
        now = 0;
    (void)platinum_prefs_muted_load(client, &muted, now);

    memset(&response, 0, sizeof(response));
    thread->loading = 1;
    platinum_thread_set_status(thread, "Loading...");
    status = platinum_atproto_feed_page(client, thread->path,
                                        append ? thread->cursor : NULL,
                                        PLATINUM_THREAD_PAGE, &response);
    if (status != WF_OK) {
        wf_response_free(&response);
        thread->loading = 0;
        platinum_thread_set_status(
            thread, status == WF_ERR_AUTH ? "Session expired. Sign in again."
                                          : "The feed could not be loaded.");
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "feed", &feed) != WF_OK ||
        platinum_json_count(feed, &available) != WF_OK) {
        wf_response_free(&response);
        thread->loading = 0;
        platinum_thread_set_status(thread,
                                   "The server returned an invalid list.");
        return WF_ERR_PARSE;
    }

    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) != WF_OK)
        cursor[0] = '\0';
    platinum_thread_take_feed(thread, feed, named_view_is_muted, &muted, cursor,
                              dropped);
    wf_response_free(&response);

    thread->loading = 0;
    platinum_thread_set_status(thread,
                               thread->count == 0 ? "No posts to show."
                                                  : NULL);
    return WF_OK;
}

wf_status platinum_named_feed_native(platinum_thread *thread,
                                     platinum_atproto_client *client,
                                     const char *uri, const char *heading)
{
    if (thread == NULL || client == NULL || uri == NULL || uri[0] == '\0')
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_at_uri(uri))
        return WF_ERR_INVALID_ARG;

    thread->count = 0;
    thread->scroll_row = 0;
    thread->focus = 0;
    thread->truncated = 0;
    strncpy(thread->path, uri, sizeof(thread->path) - 1);
    thread->path[sizeof(thread->path) - 1] = '\0';
    thread->cursor[0] = '\0';
    thread->native_feed = 1;
    thread->heading[0] = '\0';
    if (heading != NULL) {
        strncpy(thread->heading, heading, sizeof(thread->heading) - 1);
        thread->heading[sizeof(thread->heading) - 1] = '\0';
    }
    return named_feed_page(thread, client, 0, NULL);
}

wf_status platinum_named_feed_more_native(platinum_thread *thread,
                                          platinum_atproto_client *client,
                                          unsigned short *dropped)
{
    if (dropped != NULL)
        *dropped = 0;
    if (thread == NULL || client == NULL || !thread->native_feed ||
        !platinum_thread_has_more(thread))
        return WF_ERR_INVALID_ARG;
    return named_feed_page(thread, client, 1, dropped);
}

/* ---- a list's members --------------------------------------------------- */

static wf_status named_members_page(platinum_people *people,
                                    platinum_atproto_client *client,
                                    int append, unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json items;
    char cursor[sizeof(people->cursor)];
    long available;
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;

    memset(&response, 0, sizeof(response));
    people->loading = 1;
    platinum_people_set_status(people, "Loading...");
    status = platinum_atproto_list_page(client, people->path,
                                        append ? people->cursor : NULL,
                                        PLATINUM_PEOPLE_PAGE, &response);
    if (status != WF_OK) {
        wf_response_free(&response);
        people->loading = 0;
        platinum_people_set_status(
            people, status == WF_ERR_AUTH ? "Session expired. Sign in again."
                                          : "The list could not be loaded.");
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, "items", &items) != WF_OK ||
        platinum_json_count(items, &available) != WF_OK) {
        wf_response_free(&response);
        people->loading = 0;
        platinum_people_set_status(people,
                                   "The server returned an invalid list.");
        return WF_ERR_PARSE;
    }

    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) != WF_OK)
        cursor[0] = '\0';
    platinum_people_take_subjects(people, items, cursor, dropped);
    wf_response_free(&response);

    people->loading = 0;
    platinum_people_set_status(people,
                               people->count == 0 ? "Nobody to show." : NULL);
    return WF_OK;
}

wf_status platinum_named_list_native(platinum_people *people,
                                     platinum_atproto_client *client,
                                     const char *uri, const char *heading)
{
    if (people == NULL || client == NULL || uri == NULL || uri[0] == '\0')
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_at_uri(uri))
        return WF_ERR_INVALID_ARG;

    people->kind = PLATINUM_PEOPLE_ACCOUNTS;
    people->native_list = 1;
    people->count = 0;
    people->scroll_row = 0;
    people->selected = -1;
    strncpy(people->path, uri, sizeof(people->path) - 1);
    people->path[sizeof(people->path) - 1] = '\0';
    people->cursor[0] = '\0';
    people->heading[0] = '\0';
    if (heading != NULL) {
        strncpy(people->heading, heading, sizeof(people->heading) - 1);
        people->heading[sizeof(people->heading) - 1] = '\0';
    }
    return named_members_page(people, client, 0, NULL);
}

wf_status platinum_named_list_more_native(platinum_people *people,
                                          platinum_atproto_client *client,
                                          unsigned short *dropped)
{
    if (dropped != NULL)
        *dropped = 0;
    if (people == NULL || client == NULL || !people->native_list ||
        !platinum_people_has_more(people))
        return WF_ERR_INVALID_ARG;
    return named_members_page(people, client, 1, dropped);
}
