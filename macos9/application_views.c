/* Opening and refreshing the windows: timeline, thread, profile, people, search and notifications. */

#include "application_internal.h"

void platinum_application_refresh_timeline(platinum_application *app)
{
    platinum_bridge_client *bridge;

    if (app == NULL)
        return;

    if (!platinum_session_is_paired(&app->session)) {
        platinum_timeline_init(&app->timeline);
        platinum_application_invalidate(app);
        return;
    }

    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL) {
        platinum_timeline_init(&app->timeline);
        return;
    }

    app->ui.scroll_row = 0;
    app->ui.selected_post = 0;
    {
        wf_status status;
        status = platinum_timeline_refresh(&app->timeline, bridge);
        platinum_application_recover_auth(app, status);
    }
    if (app->timeline.count == 0) {
        app->ui.scroll_row = 0;
        app->ui.selected_post = 0;
    }

    platinum_application_relayout(app);
    platinum_scrollbar_set_range(
        &app->timeline_scrollbar,
        (short)app->timeline.count,
        platinum_application_timeline_visible_rows(app),
        app->ui.scroll_row);
    platinum_application_invalidate(app);
}

/* Open the selected post's author in the Profile window. */
void platinum_application_show_author(platinum_application *app)
{
    platinum_bridge_client *bridge;
    wf_status status;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    if (platinum_profile_open(&app->profile) != noErr)
        return;

    status = platinum_profile_load(&app->profile, bridge,
                                   app->timeline.posts[index].handle);
    platinum_application_recover_auth(app, status);
    if (app->profile.window != NULL)
        InvalRect(&app->profile.window->portRect);
}

/* Likers, reposters (of the selected post) or followers, following (of the
 * account in the Profile window), in the shared People window. */
void platinum_application_show_people(platinum_application *app,
                                             int menu_item)
{
    platinum_bridge_client *bridge;
    const char *route;
    const char *key;
    const char *value;
    const char *heading;
    wf_status status;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;

    if (menu_item == 6 || menu_item == 7) {
        index = app->ui.selected_post;
        if (index < 0 || index >= (short)app->timeline.count)
            return;
        key = "uri";
        value = app->timeline.posts[index].uri;
        route = menu_item == 6 ? "/v1/post/likes" : "/v1/post/reposts";
        heading = menu_item == 6 ? "Liked by" : "Reposted by";
    } else {
        /* The DID read exactly from the profile, not the clipped display copy. */
        if (app->profile.window == NULL || app->profile.target_did[0] == '\0')
            return;
        key = "actor";
        value = app->profile.target_did;
        route = menu_item == 8 ? "/v1/followers" : "/v1/follows";
        heading = menu_item == 8 ? "Followers of this account" : "Followed by this account";
    }

    if (platinum_people_open(&app->people) != noErr)
        return;
    status = platinum_people_load(&app->people, bridge, route, key, value,
                                  heading);
    platinum_application_recover_auth(app, status);
    if (app->people.window != NULL)
        InvalRect(&app->people.window->portRect);
}

void platinum_application_more_people(platinum_application *app)
{
    platinum_bridge_client *bridge;
    unsigned short dropped;
    wf_status status;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    status = platinum_people_load_more(&app->people, bridge, &dropped);
    platinum_application_recover_auth(app, status);
    if (status == WF_OK) {
        app->people.scroll_row -= (short)dropped;
        if (app->people.scroll_row < 0)
            app->people.scroll_row = 0;
        ++app->people.scroll_row;
    }
    if (app->people.window != NULL)
        InvalRect(&app->people.window->portRect);
}

/* Run the query typed in the Search window and show the results. */
void platinum_application_run_search(platinum_application *app)
{
    platinum_bridge_client *bridge;
    char query[PLATINUM_SEARCH_MAX * 4 + 1];
    int posts;
    wf_status status;

    if (app == NULL || app->search.window == NULL ||
        !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    if (platinum_search_query(&app->search, query, sizeof(query)) < 0) {
        platinum_search_set_status(&app->search,
                                   "Type something to search for (100 characters at most).");
        return;
    }

    if (app->search.mode == PLATINUM_SEARCH_WORD) {
        status = platinum_bridge_set_muted_word(bridge, query, 1);
        if (status != WF_OK) {
            platinum_search_set_status(&app->search,
                                       "The word could not be added.");
            platinum_application_recover_auth(app, status);
            return;
        }
        platinum_search_close(&app->search);
        platinum_application_show_words(app);
        return;
    }

    posts = app->search.mode == PLATINUM_SEARCH_POSTS;
    platinum_search_close(&app->search);

    if (posts) {
        if (platinum_thread_open(&app->thread) != noErr)
            return;
        status = platinum_thread_load_list(&app->thread, bridge,
                                           "/v1/search/posts", "q", query,
                                           "Posts matching your search");
        platinum_application_recover_auth(app, status);
        if (app->thread.window != NULL)
            InvalRect(&app->thread.window->portRect);
    } else {
        if (platinum_people_open(&app->people) != noErr)
            return;
        status = platinum_people_load(&app->people, bridge,
                                      "/v1/search/actors", "q", query,
                                      "Accounts matching your search (click one)");
        platinum_application_recover_auth(app, status);
        if (app->people.window != NULL)
            InvalRect(&app->people.window->portRect);
    }
}

/* Your muted words, in the People window. */
void platinum_application_show_words(platinum_application *app)
{
    platinum_bridge_client *bridge;
    wf_status status;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL || platinum_people_open(&app->people) != noErr)
        return;
    status = platinum_people_load_kind(
        &app->people, bridge, PLATINUM_PEOPLE_WORDS, "/v1/muted-words", NULL,
        NULL, "Muted words (click one, then View > Remove Muted Word)");
    platinum_application_recover_auth(app, status);
    if (app->people.window != NULL)
        InvalRect(&app->people.window->portRect);
}

/* Remove the muted word selected in the People window. */
void platinum_application_remove_word(platinum_application *app)
{
    platinum_bridge_client *bridge;
    const platinum_person *row;
    char word[sizeof(row->uri)];
    wf_status status;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    row = platinum_people_selection(&app->people);
    if (app->people.window == NULL || app->people.kind != PLATINUM_PEOPLE_WORDS ||
        row == NULL || row->uri[0] == '\0') {
        if (app->people.window != NULL)
            platinum_people_set_status(
                &app->people, "Open Muted Words and click one first.");
        if (app->people.window != NULL)
            InvalRect(&app->people.window->portRect);
        return;
    }
    strcpy(word, row->uri);
    status = platinum_bridge_set_muted_word(bridge, word, 0);
    if (status != WF_OK) {
        platinum_people_set_status(&app->people,
                                   "The word could not be removed.");
        platinum_application_recover_auth(app, status);
        if (app->people.window != NULL)
            InvalRect(&app->people.window->portRect);
        return;
    }
    platinum_application_show_words(app);
}

/* The author's own posts, in the posts list. */
void platinum_application_show_posts(platinum_application *app)
{
    platinum_bridge_client *bridge;
    const char *handle;
    wf_status status;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    handle = app->timeline.posts[index].handle;
    if (handle[0] == '@')
        ++handle;
    if (handle[0] == '\0' || platinum_thread_open(&app->thread) != noErr)
        return;

    status = platinum_thread_load_list(&app->thread, bridge, "/v1/author-feed",
                                       "actor", handle, "Posts by this author");
    platinum_application_recover_auth(app, status);
    if (app->thread.window != NULL)
        InvalRect(&app->thread.window->portRect);
}

void platinum_application_more_posts(platinum_application *app)
{
    platinum_bridge_client *bridge;
    unsigned short dropped;
    wf_status status;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    status = platinum_thread_load_more(&app->thread, bridge, &dropped);
    platinum_application_recover_auth(app, status);
    if (status == WF_OK) {
        app->thread.scroll_row -= (short)dropped;
        if (app->thread.scroll_row < 0)
            app->thread.scroll_row = 0;
        ++app->thread.scroll_row;
    }
    if (app->thread.window != NULL)
        InvalRect(&app->thread.window->portRect);
}

/* Saved feeds or the account's lists, as rows to click. */
void platinum_application_show_named(platinum_application *app,
                                            int kind)
{
    platinum_bridge_client *bridge;
    wf_status status;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL || platinum_people_open(&app->people) != noErr)
        return;
    status = platinum_people_load_kind(
        &app->people, bridge, kind,
        kind == PLATINUM_PEOPLE_FEEDS ? "/v1/feeds" : "/v1/lists", NULL, NULL,
        kind == PLATINUM_PEOPLE_FEEDS ? "Saved feeds (click one)"
                                      : "My lists (click one)");
    platinum_application_recover_auth(app, status);
    if (app->people.window != NULL)
        InvalRect(&app->people.window->portRect);
}

/* A clicked row in the People window: open the account, feed or list. */
void platinum_application_open_person(platinum_application *app)
{
    platinum_bridge_client *bridge;
    const platinum_person *row;
    wf_status status;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    row = platinum_people_selection(&app->people);
    bridge = platinum_session_bridge(&app->session);
    if (row == NULL || row->uri[0] == '\0' || bridge == NULL)
        return;

    if (app->people.kind == PLATINUM_PEOPLE_WORDS) {
        /* A click only selects the word. */
        return;
    }
    if (app->people.kind == PLATINUM_PEOPLE_FEEDS) {
        if (platinum_thread_open(&app->thread) != noErr)
            return;
        status = platinum_thread_load_list(&app->thread, bridge, "/v1/feed",
                                           "uri", row->uri, row->name);
        platinum_application_recover_auth(app, status);
        if (app->thread.window != NULL)
            InvalRect(&app->thread.window->portRect);
    } else if (app->people.kind == PLATINUM_PEOPLE_LISTS) {
        /* The list replaces what is in the window, so copy the name first. */
        char name[PLATINUM_PEOPLE_NAME_MAX];
        char uri[sizeof(row->uri)];

        strcpy(name, row->name);
        strcpy(uri, row->uri);
        status = platinum_people_load_kind(&app->people, bridge,
                                           PLATINUM_PEOPLE_ACCOUNTS, "/v1/list",
                                           "uri", uri, name);
        platinum_application_recover_auth(app, status);
        if (app->people.window != NULL)
            InvalRect(&app->people.window->portRect);
    } else {
        if (platinum_profile_open(&app->profile) != noErr)
            return;
        status = platinum_profile_load(&app->profile, bridge, row->uri);
        platinum_application_recover_auth(app, status);
        if (app->profile.window != NULL)
            InvalRect(&app->profile.window->portRect);
    }
}

/* Follow or unfollow the account in the Profile window. */
void platinum_application_follow(platinum_application *app)
{
    platinum_bridge_client *bridge;
    wf_status status;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    if (app->profile.window == NULL || !app->profile.other) {
        if (app->profile.window != NULL)
            platinum_profile_set_status(
                &app->profile, "Open someone else's profile to follow them.");
        return;
    }

    status = platinum_profile_set_follow(&app->profile, bridge,
                                         !app->profile.following);
    platinum_application_recover_auth(app, status);
    InvalRect(&app->profile.window->portRect);
}

/* Mute or block (or undo it) on the account in the Profile window. Muting is
 * private and undone the same way. Blocking is public, so the first request
 * only asks and the same request again within the window does it; any other
 * menu choice disarms it. Unblocking needs no second step. */
void platinum_application_relate(platinum_application *app, int kind)
{
    platinum_bridge_client *bridge;
    wf_status status;
    int on;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    if (app->profile.window == NULL || !app->profile.other) {
        if (app->profile.window != NULL)
            platinum_profile_set_status(
                &app->profile, "Open someone else's profile first.");
        return;
    }

    on = (kind == PLATINUM_RELATION_BLOCK) ? !app->profile.blocking
                                           : !app->profile.muted;
    if (kind == PLATINUM_RELATION_BLOCK && on && !app->profile.block_armed) {
        app->profile.block_armed = 1;
        platinum_profile_set_status(
            &app->profile,
            "Choose Block again to block them. Any other menu choice cancels.");
        return;
    }
    app->profile.block_armed = 0;

    status = platinum_profile_set_relation(&app->profile, bridge, kind, on);
    platinum_application_recover_auth(app, status);
    InvalRect(&app->profile.window->portRect);
}

/* Open the thread around the selected post. */
void platinum_application_show_thread(platinum_application *app)
{
    platinum_bridge_client *bridge;
    wf_status status;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;
    if (platinum_thread_open(&app->thread) != noErr)
        return;

    status = platinum_thread_load(&app->thread, bridge,
                                  app->timeline.posts[index].uri);
    platinum_application_recover_auth(app, status);
    if (app->thread.window != NULL)
        InvalRect(&app->thread.window->portRect);
}

/* Help > Connection Status. */
void platinum_application_open_diag(platinum_application *app)
{
    if (app == NULL)
        return;
    if (platinum_diagwin_open(&app->diagwin) != noErr)
        return;
    platinum_application_check_diag(app);
    SelectWindow(app->diagwin.window);
}

/* Take a fresh snapshot: ask the bridge for /health and read what is known.
 * Never puts a token anywhere; only whether one is held. */
void platinum_application_check_diag(platinum_application *app)
{
    const platinum_config *config;

    if (app == NULL || app->diagwin.window == NULL)
        return;
    config = platinum_session_config(&app->session);
    (void)platinum_diag_check(&app->diagwin.diag,
                              platinum_session_bridge(&app->session),
                              config != NULL ? config->bridge_url : NULL,
                              platinum_session_is_paired(&app->session),
                              app->last_error, (long)FreeMem());
    InvalRect(&app->diagwin.window->portRect);
}

void platinum_application_load_older(platinum_application *app)
{
    platinum_bridge_client *bridge;
    unsigned short dropped;
    short old_count;
    wf_status status;

    if (app == NULL || !platinum_timeline_has_older(&app->timeline) ||
        !platinum_session_is_paired(&app->session))
        return;

    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;

    old_count = (short)app->timeline.count;
    status = platinum_timeline_load_older(&app->timeline, bridge, &dropped);
    platinum_application_recover_auth(app, status);

    if (status == WF_OK) {
        /* Rows dropped from the front shift everything up; keep the reader on
         * the same post, then move to the first new one. */
        app->ui.scroll_row -= (short)dropped;
        app->ui.selected_post -= (short)dropped;
        if (app->ui.scroll_row < 0)
            app->ui.scroll_row = 0;
        if (app->ui.selected_post < 0)
            app->ui.selected_post = 0;
        if ((short)app->timeline.count > old_count - (short)dropped)
            app->ui.selected_post = old_count - (short)dropped;
        {
            short visible = platinum_application_timeline_visible_rows(app);
            if (app->ui.selected_post >= app->ui.scroll_row + visible)
                app->ui.scroll_row = app->ui.selected_post - visible + 1;
        }
    }

    platinum_application_relayout(app);
    platinum_application_invalidate(app);
}

void platinum_application_open_profile(platinum_application *app)
{
    platinum_bridge_client *bridge;

    if (app == NULL)
        return;

    if (!platinum_session_is_paired(&app->session)) {
        platinum_profile_set_status(&app->profile,
                                    "Pair an account before viewing Profile.");
        return;
    }

    if (platinum_profile_open(&app->profile) != noErr)
        return;

    bridge = platinum_session_bridge(&app->session);
    if (bridge != NULL)
        {
        wf_status status;
        status = platinum_profile_refresh(&app->profile, bridge);
        platinum_application_recover_auth(app, status);
    }
}

void platinum_application_open_notifications(platinum_application *app)
{
    if (app == NULL)
        return;

    if (!platinum_session_is_paired(&app->session)) {
        platinum_notifications_init(&app->notifications);
        platinum_application_invalidate(app);
        return;
    }

    if (platinum_notifications_open(&app->notifications) != noErr)
        return;

    platinum_application_refresh_notifications(app);
}

void platinum_application_older_notifications(
    platinum_application *app)
{
    platinum_bridge_client *bridge;
    unsigned short dropped;
    wf_status status;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;

    status = platinum_notifications_load_older(&app->notifications, bridge,
                                               &dropped);
    platinum_application_recover_auth(app, status);
    if (status == WF_OK) {
        /* Keep the same rows on screen, then step onto the first new one. */
        app->notifications.scroll_row -= (short)dropped;
        if (app->notifications.scroll_row < 0)
            app->notifications.scroll_row = 0;
        ++app->notifications.scroll_row;
    }
    if (app->notifications.window != NULL)
        InvalRect(&app->notifications.window->portRect);
}

void platinum_application_refresh_notifications(
    platinum_application *app)
{
    platinum_bridge_client *bridge;

    if (app == NULL)
        return;

    if (!platinum_session_is_paired(&app->session)) {
        platinum_notifications_init(&app->notifications);
        return;
    }

    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;

    {
        wf_status status;
        status = platinum_notifications_refresh(&app->notifications, bridge);
        platinum_application_recover_auth(app, status);
        /* Opening or refreshing the window is reading it. A failure to mark
         * seen is not worth an error: the next refresh tries again. */
        if (status == WF_OK)
            (void)platinum_notifications_mark_seen(&app->notifications,
                                                   bridge);
    }
    if (app->notifications.window != NULL)
        InvalRect(&app->notifications.window->portRect);
}
