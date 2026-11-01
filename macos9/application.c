#include "application_internal.h"
#include "drawutil.h"

static unsigned char kWindowTitle[] = {
    15, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', '-', ' ',
    'H', 'o', 'm', 'e'
};



OSErr platinum_application_init(platinum_application *app)
{
    Rect bounds;
    OSErr err;

    if (app == NULL)
        return paramErr;

    memset(app, 0, sizeof(*app));
    platinum_session_init(&app->session);
    platinum_ui_state_init(&app->ui);
    platinum_timeline_init(&app->timeline);
    platinum_profile_init(&app->profile);
    platinum_notifications_init(&app->notifications);
    platinum_conversations_init(&app->conversations);
    platinum_thread_init(&app->thread);
    platinum_people_init(&app->people);
    platinum_search_init(&app->search);
    platinum_diagwin_init(&app->diagwin);
    platinum_help_init(&app->help);
    platinum_apppw_init(&app->apppw);
    platinum_preferences_init(&app->preferences);
    memset(&app->pairing, 0, sizeof(app->pairing));

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    FlushEvents(everyEvent, 0);

    err = platinum_application_create_menus(app);
    if (err != noErr)
        return err;

    err = platinum_session_load(&app->session);
    if (err != noErr) {
        platinum_application_dispose_menus(app);
        return err;
    }

    SetRect(&bounds, 48, 40, 688, 520);

    if (platinum_window_open(&app->window, &bounds, kWindowTitle) != noErr) {
        platinum_session_close(&app->session);
        platinum_application_dispose_menus(app);
        return memFullErr;
    }

    platinum_ui_layout_compute(&app->window->portRect, &app->layout);

    if (platinum_scrollbar_open(&app->timeline_scrollbar,
                                app->window,
                                &app->layout.timeline_scrollbar) != noErr) {
        DisposeWindow(app->window);
        app->window = NULL;
        platinum_session_close(&app->session);
        platinum_application_dispose_menus(app);
        return memFullErr;
    }

    app->running = 1;
    wf_macos9_set_yield_callback(platinum_application_yield, app);
    platinum_application_relayout(app);
    platinum_application_invalidate(app);

    if (platinum_session_is_paired(&app->session))
        platinum_application_refresh_timeline(app);
    else
        platinum_application_open_apppw(app);

    return noErr;
}

void platinum_application_run(platinum_application *app)
{
    EventRecord event;

    if (app == NULL)
        return;

    while (app->running) {
        if (WaitNextEvent(everyEvent, &event, 6, NULL))
            platinum_application_handle_event(app, &event);
    }
}

void platinum_application_dispose(platinum_application *app)
{
    if (app == NULL)
        return;

    wf_macos9_set_yield_callback(NULL, NULL);

    platinum_scrollbar_close(&app->timeline_scrollbar);

    if (app->window != NULL) {
        DisposeWindow(app->window);
        app->window = NULL;
    }

    platinum_pairing_close(&app->pairing);
    platinum_apppw_close(&app->apppw);
    platinum_preferences_close(&app->preferences);
    platinum_notifications_close(&app->notifications);
    platinum_conversations_close_window(&app->conversations);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_diagwin_close(&app->diagwin);
    platinum_help_close(&app->help);
    platinum_profile_close(&app->profile);
    platinum_application_close_compose(app, 0);
    platinum_session_close(&app->session);
    platinum_application_dispose_menus(app);
}

void platinum_application_yield(void *userdata)
{
    platinum_application *app;
    EventRecord event;

    app = (platinum_application *)userdata;
    if (app == NULL || !app->running)
        return;

    if (WaitNextEvent(updateMask | activMask, &event, 0, NULL))
        platinum_application_handle_event(app, &event);
}

/* True when candidate is the window a secondary view owns and that view is
 * open. A NULL owned window means the view is closed. */
static int window_open_is(WindowPtr candidate, WindowPtr owned)
{
    return owned != NULL && candidate == owned;
}

/* After a secondary window closes, the main window takes focus again. */
static void focus_main(platinum_application *app)
{
    SelectWindow(app->window);
    platinum_application_invalidate(app);
}

static int
close_owned_window(platinum_application *app, WindowPtr window)
{
    if (window_open_is(window, app->profile.window)) {
        platinum_profile_close(&app->profile);
    } else if (window_open_is(window, app->notifications.window)) {
        platinum_notifications_close(&app->notifications);
    } else if (window_open_is(window, app->conversations.window)) {
        platinum_conversations_close_window(&app->conversations);
    } else if (window_open_is(window, app->thread.window)) {
        platinum_thread_close(&app->thread);
    } else if (window_open_is(window, app->people.window)) {
        platinum_people_close(&app->people);
    } else if (window_open_is(window, app->diagwin.window)) {
        platinum_diagwin_close(&app->diagwin);
    } else if (window_open_is(window, app->help.window)) {
        platinum_help_close(&app->help);
    } else if (window_open_is(window, app->search.window)) {
        platinum_search_close(&app->search);
    } else if (window_open_is(window, app->apppw.window)) {
        platinum_apppw_close(&app->apppw);
    } else if (window_open_is(window, app->pairing.window)) {
        platinum_pairing_close(&app->pairing);
    } else if (window_open_is(window, app->preferences.window)) {
        platinum_preferences_close(&app->preferences);
    } else if (window_open_is(window, app->compose.window)) {
        platinum_application_close_compose(app, 0);
    } else {
        return 0;
    }
    focus_main(app);
    return 1;
}

static void
handle_main_mouse(platinum_application *app, EventRecord *event)
{
    short scrollbar_value;
    Point local_where;
    int action;

    if (platinum_scrollbar_handle_mouse(&app->timeline_scrollbar, event,
                                        &scrollbar_value)) {
        app->ui.scroll_row = scrollbar_value;
        platinum_application_invalidate(app);
        return;
    }

    local_where = event->where;
    SetPort((GrafPtr)app->window);
    GlobalToLocal(&local_where);

    action = platinum_ui_handle_mouse(&app->layout, &app->ui, &app->session,
                                      &app->timeline, local_where);
    if (action == PLATINUM_UI_ACTION_QUIT)
        app->running = 0;
    else if (action == PLATINUM_UI_ACTION_REFRESH)
        platinum_application_refresh_timeline(app);
    else if (action == PLATINUM_UI_ACTION_COMPOSE)
        platinum_application_new_post(app);
    else if (action == PLATINUM_UI_ACTION_PROFILE)
        platinum_application_open_profile(app);
    else if (action == PLATINUM_UI_ACTION_NOTIFICATIONS)
        platinum_application_open_notifications(app);
    else if (action == PLATINUM_UI_ACTION_LOAD_OLDER)
        platinum_application_load_older(app);
    else if (action == PLATINUM_UI_ACTION_LIKE)
        platinum_application_engage(app, 0);
    else if (action == PLATINUM_UI_ACTION_REPOST)
        platinum_application_engage(app, 1);
    else if (action == PLATINUM_UI_ACTION_REPLY)
        platinum_application_reply(app);
    else if (action == PLATINUM_UI_ACTION_THREAD)
        platinum_application_show_thread(app);
    else if (action == PLATINUM_UI_ACTION_AUTHOR)
        platinum_application_show_author(app);
    else if (action == PLATINUM_UI_ACTION_FOLLOW)
        platinum_application_follow(app);
    else if (action == PLATINUM_UI_ACTION_SIGN_IN)
        (void)platinum_application_open_apppw(app);
    else if (action == PLATINUM_UI_ACTION_PAIR)
        (void)platinum_application_open_pairing(app);
    platinum_application_invalidate(app);
}

static void
handle_secondary_mouse(platinum_application *app, WindowPtr window,
                       EventRecord *event)
{
    int action;

    if (window_open_is(window, app->profile.window)) {
        action = platinum_profile_handle_event(&app->profile, event);
        if (action == PLATINUM_PROFILE_CLOSE) {
            platinum_profile_close(&app->profile);
            focus_main(app);
        } else if (action == PLATINUM_PROFILE_SIGN_IN) {
            platinum_application_open_apppw(app);
        } else if (action == PLATINUM_PROFILE_PAIR) {
            platinum_application_open_pairing(app);
        }
    } else if (window_open_is(window, app->diagwin.window)) {
        action = platinum_diagwin_handle_event(&app->diagwin, event);
        if (action == PLATINUM_DIAGWIN_CLOSE) {
            platinum_diagwin_close(&app->diagwin);
            focus_main(app);
        } else if (action == PLATINUM_DIAGWIN_CHECK) {
            platinum_application_check_diag(app);
        } else if (action == PLATINUM_DIAGWIN_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_DIAGWIN_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (window_open_is(window, app->help.window)) {
        action = platinum_help_handle_event(&app->help, event);
        if (action == PLATINUM_HELP_CLOSE) {
            platinum_help_close(&app->help);
            focus_main(app);
        }
    } else if (window_open_is(window, app->search.window)) {
        action = platinum_search_handle_event(&app->search, event);
        if (action == PLATINUM_SEARCH_CANCEL) {
            platinum_search_close(&app->search);
            focus_main(app);
        } else if (action == PLATINUM_SEARCH_RUN) {
            platinum_application_run_search(app);
        } else if (action == PLATINUM_SEARCH_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_SEARCH_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (window_open_is(window, app->people.window)) {
        action = platinum_people_handle_event(&app->people, event);
        if (action == PLATINUM_PEOPLE_CLOSE) {
            platinum_people_close(&app->people);
            focus_main(app);
        } else if (action == PLATINUM_PEOPLE_LOAD_MORE) {
            platinum_application_more_people(app);
        } else if (action == PLATINUM_PEOPLE_OPEN) {
            platinum_application_open_person(app);
        } else if (action == PLATINUM_PEOPLE_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_PEOPLE_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (window_open_is(window, app->thread.window)) {
        action = platinum_thread_handle_event(&app->thread, event);
        if (action == PLATINUM_THREAD_CLOSE) {
            platinum_thread_close(&app->thread);
            focus_main(app);
        } else if (action == PLATINUM_THREAD_LOAD_MORE) {
            platinum_application_more_posts(app);
        } else if (action == PLATINUM_THREAD_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_THREAD_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (window_open_is(window, app->notifications.window)) {
        action = platinum_notifications_handle_event(&app->notifications, event);
        if (action == PLATINUM_NOTIFICATIONS_CLOSE) {
            platinum_notifications_close(&app->notifications);
            focus_main(app);
        } else if (action == PLATINUM_NOTIFICATIONS_REFRESH) {
            platinum_application_refresh_notifications(app);
        } else if (action == PLATINUM_NOTIFICATIONS_LOAD_OLDER) {
            platinum_application_older_notifications(app);
        } else if (action == PLATINUM_NOTIFICATIONS_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_NOTIFICATIONS_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (window_open_is(window, app->conversations.window)) {
        action = platinum_conversations_handle_event(&app->conversations, event);
        if (action == PLATINUM_CONVERSATIONS_CLOSE) {
            platinum_conversations_close_window(&app->conversations);
            focus_main(app);
        } else if (action == PLATINUM_CONVERSATIONS_REFRESH) {
            platinum_application_refresh_conversations(app);
        } else if (action == PLATINUM_CONVERSATIONS_LOAD_OLDER) {
            platinum_application_older_conversations(app);
        } else if (action == PLATINUM_CONVERSATIONS_OPEN) {
            platinum_application_open_conversation(app);
        } else if (action == PLATINUM_CONVERSATIONS_SEND) {
            platinum_application_send_message(app);
        } else if (action == PLATINUM_CONVERSATIONS_OLDER_MESSAGES) {
            platinum_application_older_messages(app);
        } else if (action == PLATINUM_CONVERSATIONS_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_CONVERSATIONS_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (window_open_is(window, app->apppw.window)) {
        action = platinum_apppw_handle_event(&app->apppw, event);
        if (action == PLATINUM_APPPW_CANCEL) {
            platinum_apppw_close(&app->apppw);
            focus_main(app);
        } else if (action == PLATINUM_APPPW_SIGN_IN) {
            platinum_application_attempt_apppw(app);
        }
    } else if (window_open_is(window, app->pairing.window)) {
        action = platinum_pairing_handle_event(&app->pairing, event);
        if (action == PLATINUM_PAIRING_CANCEL) {
            platinum_pairing_close(&app->pairing);
            focus_main(app);
        } else if (action == PLATINUM_PAIRING_PAIR) {
            platinum_application_attempt_pair(app);
        }
    } else if (window_open_is(window, app->preferences.window)) {
        action = platinum_preferences_handle_event(&app->preferences, event);
        if (action == PLATINUM_PREFERENCES_CLOSE) {
            platinum_preferences_close(&app->preferences);
            focus_main(app);
        } else if (action == PLATINUM_PREFERENCES_SIGN_OUT) {
            platinum_application_sign_out(app);
        } else if (action == PLATINUM_PREFERENCES_PAIR) {
            platinum_preferences_close(&app->preferences);
            platinum_application_open_pairing(app);
        }
    } else if (window_open_is(window, app->compose.window)) {
        action = platinum_compose_handle_event(&app->compose, event);
        if (action == PLATINUM_COMPOSE_CANCEL) {
            platinum_application_close_compose(app, 0);
            focus_main(app);
        } else if (action == PLATINUM_COMPOSE_POST) {
            platinum_application_submit_post(app);
        }
    }
}

static void
handle_secondary_update(platinum_application *app, WindowPtr window,
                        EventRecord *event)
{
    if (window_open_is(window, app->apppw.window))
        platinum_apppw_handle_event(&app->apppw, event);
    else if (window_open_is(window, app->pairing.window))
        platinum_pairing_handle_event(&app->pairing, event);
    else if (window_open_is(window, app->preferences.window))
        platinum_preferences_handle_event(&app->preferences, event);
    else if (window_open_is(window, app->diagwin.window))
        platinum_diagwin_handle_event(&app->diagwin, event);
    else if (window_open_is(window, app->help.window))
        platinum_help_handle_event(&app->help, event);
    else if (window_open_is(window, app->search.window))
        platinum_search_handle_event(&app->search, event);
    else if (window_open_is(window, app->people.window))
        platinum_people_handle_event(&app->people, event);
    else if (window_open_is(window, app->thread.window))
        platinum_thread_handle_event(&app->thread, event);
    else if (window_open_is(window, app->notifications.window))
        platinum_notifications_handle_event(&app->notifications, event);
    else if (window_open_is(window, app->profile.window))
        platinum_profile_handle_event(&app->profile, event);
    else if (window_open_is(window, app->compose.window))
        platinum_compose_handle_event(&app->compose, event);
}

static void
handle_secondary_activate(platinum_application *app, WindowPtr window,
                          EventRecord *event)
{
    if (window_open_is(window, app->apppw.window))
        platinum_apppw_handle_event(&app->apppw, event);
    else if (window_open_is(window, app->pairing.window))
        platinum_pairing_handle_event(&app->pairing, event);
    else if (window_open_is(window, app->preferences.window))
        platinum_preferences_handle_event(&app->preferences, event);
    else if (window_open_is(window, app->diagwin.window))
        platinum_diagwin_handle_event(&app->diagwin, event);
    else if (window_open_is(window, app->help.window))
        platinum_help_handle_event(&app->help, event);
    else if (window_open_is(window, app->search.window))
        platinum_search_handle_event(&app->search, event);
    else if (window_open_is(window, app->people.window))
        platinum_people_handle_event(&app->people, event);
    else if (window_open_is(window, app->thread.window))
        platinum_thread_handle_event(&app->thread, event);
    else if (window_open_is(window, app->notifications.window))
        platinum_notifications_handle_event(&app->notifications, event);
    else if (window_open_is(window, app->profile.window))
        platinum_profile_handle_event(&app->profile, event);
    else if (window_open_is(window, app->compose.window))
        platinum_compose_handle_event(&app->compose, event);
}

static int
handle_secondary_key(platinum_application *app, EventRecord *event)
{
    int action;

    if (app->apppw.window != NULL && FrontWindow() == app->apppw.window) {
        action = platinum_apppw_handle_event(&app->apppw, event);
        if (action == PLATINUM_APPPW_CANCEL) {
            platinum_apppw_close(&app->apppw);
            focus_main(app);
        } else if (action == PLATINUM_APPPW_SIGN_IN) {
            platinum_application_attempt_apppw(app);
        }
    } else if (app->pairing.window != NULL && FrontWindow() == app->pairing.window) {
        action = platinum_pairing_handle_event(&app->pairing, event);
        if (action == PLATINUM_PAIRING_CANCEL) {
            platinum_pairing_close(&app->pairing);
            focus_main(app);
        } else if (action == PLATINUM_PAIRING_PAIR) {
            platinum_application_attempt_pair(app);
        }
    } else if (app->preferences.window != NULL &&
               FrontWindow() == app->preferences.window) {
        action = platinum_preferences_handle_event(&app->preferences, event);
        if (action == PLATINUM_PREFERENCES_CLOSE) {
            platinum_preferences_close(&app->preferences);
            focus_main(app);
        } else if (action == PLATINUM_PREFERENCES_SIGN_OUT) {
            platinum_application_sign_out(app);
        } else if (action == PLATINUM_PREFERENCES_PAIR) {
            platinum_preferences_close(&app->preferences);
            platinum_application_open_pairing(app);
        }
    } else if (app->diagwin.window != NULL && FrontWindow() == app->diagwin.window) {
        action = platinum_diagwin_handle_event(&app->diagwin, event);
        if (action == PLATINUM_DIAGWIN_CLOSE) {
            platinum_diagwin_close(&app->diagwin);
            focus_main(app);
        } else if (action == PLATINUM_DIAGWIN_CHECK) {
            platinum_application_check_diag(app);
        } else if (action == PLATINUM_DIAGWIN_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_DIAGWIN_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (app->help.window != NULL && FrontWindow() == app->help.window) {
        action = platinum_help_handle_event(&app->help, event);
        if (action == PLATINUM_HELP_CLOSE) {
            platinum_help_close(&app->help);
            focus_main(app);
        }
    } else if (app->search.window != NULL && FrontWindow() == app->search.window) {
        action = platinum_search_handle_event(&app->search, event);
        if (action == PLATINUM_SEARCH_CANCEL) {
            platinum_search_close(&app->search);
            focus_main(app);
        } else if (action == PLATINUM_SEARCH_RUN) {
            platinum_application_run_search(app);
        } else if (action == PLATINUM_SEARCH_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_SEARCH_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (app->people.window != NULL && FrontWindow() == app->people.window) {
        action = platinum_people_handle_event(&app->people, event);
        if (action == PLATINUM_PEOPLE_CLOSE) {
            platinum_people_close(&app->people);
            focus_main(app);
        } else if (action == PLATINUM_PEOPLE_LOAD_MORE) {
            platinum_application_more_people(app);
        } else if (action == PLATINUM_PEOPLE_OPEN) {
            platinum_application_open_person(app);
        } else if (action == PLATINUM_PEOPLE_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_PEOPLE_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (app->thread.window != NULL && FrontWindow() == app->thread.window) {
        action = platinum_thread_handle_event(&app->thread, event);
        if (action == PLATINUM_THREAD_CLOSE) {
            platinum_thread_close(&app->thread);
            focus_main(app);
        } else if (action == PLATINUM_THREAD_LOAD_MORE) {
            platinum_application_more_posts(app);
        } else if (action == PLATINUM_THREAD_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_THREAD_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (app->notifications.window != NULL &&
               FrontWindow() == app->notifications.window) {
        action = platinum_notifications_handle_event(&app->notifications, event);
        if (action == PLATINUM_NOTIFICATIONS_CLOSE) {
            platinum_notifications_close(&app->notifications);
            focus_main(app);
        } else if (action == PLATINUM_NOTIFICATIONS_REFRESH) {
            platinum_application_refresh_notifications(app);
        } else if (action == PLATINUM_NOTIFICATIONS_LOAD_OLDER) {
            platinum_application_older_notifications(app);
        } else if (action == PLATINUM_NOTIFICATIONS_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_NOTIFICATIONS_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (app->conversations.window != NULL &&
               FrontWindow() == app->conversations.window) {
        action = platinum_conversations_handle_event(&app->conversations, event);
        if (action == PLATINUM_CONVERSATIONS_CLOSE) {
            platinum_conversations_close_window(&app->conversations);
            focus_main(app);
        } else if (action == PLATINUM_CONVERSATIONS_REFRESH) {
            platinum_application_refresh_conversations(app);
        } else if (action == PLATINUM_CONVERSATIONS_LOAD_OLDER) {
            platinum_application_older_conversations(app);
        } else if (action == PLATINUM_CONVERSATIONS_OPEN) {
            platinum_application_open_conversation(app);
        } else if (action == PLATINUM_CONVERSATIONS_SEND) {
            platinum_application_send_message(app);
        } else if (action == PLATINUM_CONVERSATIONS_OLDER_MESSAGES) {
            platinum_application_older_messages(app);
        } else if (action == PLATINUM_CONVERSATIONS_SIGN_IN) {
            (void)platinum_application_open_apppw(app);
        } else if (action == PLATINUM_CONVERSATIONS_PAIR) {
            (void)platinum_application_open_pairing(app);
        }
    } else if (app->profile.window != NULL && FrontWindow() == app->profile.window) {
        action = platinum_profile_handle_event(&app->profile, event);
        if (action == PLATINUM_PROFILE_CLOSE) {
            platinum_profile_close(&app->profile);
            focus_main(app);
        } else if (action == PLATINUM_PROFILE_SIGN_IN) {
            platinum_application_open_apppw(app);
        } else if (action == PLATINUM_PROFILE_PAIR) {
            platinum_application_open_pairing(app);
        }
    } else if (app->compose.window != NULL && FrontWindow() == app->compose.window) {
        action = platinum_compose_handle_event(&app->compose, event);
        if (action == PLATINUM_COMPOSE_CANCEL) {
            platinum_application_close_compose(app, 0);
            focus_main(app);
        } else if (action == PLATINUM_COMPOSE_POST) {
            platinum_application_submit_post(app);
        }
    } else {
        return 0;
    }
    return 1;
}

static void
handle_main_key(platinum_application *app, EventRecord *event)
{
    int action = platinum_ui_handle_key(&app->layout, &app->ui, &app->session,
                                        &app->timeline, event);

    if (action == PLATINUM_UI_ACTION_QUIT)
        app->running = 0;
    else if (action == PLATINUM_UI_ACTION_REFRESH)
        platinum_application_refresh_timeline(app);
    else if (action == PLATINUM_UI_ACTION_COMPOSE)
        platinum_application_new_post(app);
    else if (action == PLATINUM_UI_ACTION_PROFILE)
        platinum_application_open_profile(app);
    else if (action == PLATINUM_UI_ACTION_NOTIFICATIONS)
        platinum_application_open_notifications(app);
    else if (action == PLATINUM_UI_ACTION_SIGN_IN)
        (void)platinum_application_open_apppw(app);
    else if (action == PLATINUM_UI_ACTION_PAIR)
        (void)platinum_application_open_pairing(app);
    platinum_application_invalidate(app);
}

void platinum_application_handle_event(platinum_application *app,
                                              EventRecord *event)
{
    WindowPtr window;
    short part;
    long choice;

    if (app == NULL || event == NULL)
        return;

    switch (event->what) {
        case mouseDown:
            part = FindWindow(event->where, &window);
            if (part == inMenuBar) {
                choice = MenuSelect(event->where);
                platinum_application_handle_menu(app, choice);
                HiliteMenu(0);
            } else if (part == inGoAway) {
                if (window == app->window)
                    app->running = 0;
                else
                    close_owned_window(app, window);
            } else if (part == inDrag) {
                DragWindow(window, event->where, NULL);
                InvalRect(&window->portRect);
            } else if (part == inContent && window != NULL &&
                       window != FrontWindow()) {
                /* A click in a window that is not frontmost only brings it to
                 * the front; the click is not delivered to its controls. */
                SelectWindow(window);
            } else if (window == app->window) {
                handle_main_mouse(app, event);
            } else {
                handle_secondary_mouse(app, window, event);
            }
            break;

        case updateEvt:
            window = (WindowPtr)(long)event->message;
            if (window == app->window) {
                BeginUpdate(window);
                platinum_application_relayout(app);
                platinum_application_draw(app);
                EndUpdate(window);
            } else {
                handle_secondary_update(app, window, event);
            }
            break;

        case activateEvt:
            window = (WindowPtr)(long)event->message;
            if (window == app->window)
                HiliteWindow(window, (event->modifiers & activeFlag) != 0);
            else
                handle_secondary_activate(app, window, event);
            break;

        case keyDown:
        case autoKey:
            if (!handle_secondary_key(app, event))
                handle_main_key(app, event);
            break;

        default:
            break;
    }
}

void platinum_application_draw(platinum_application *app)
{
    GrafPtr old_port;

    if (app == NULL || app->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)app->window);

    platinum_ui_draw(app->window,
                     &app->layout,
                     &app->ui,
                     &app->session,
                     &app->timeline);

    platinum_scrollbar_draw(&app->timeline_scrollbar);

    SetPort(old_port);
}

short platinum_application_timeline_visible_rows(
    const platinum_application *app)
{
    short visible;

    if (app == NULL)
        return 1;

    visible = (app->layout.timeline.bottom -
               app->layout.timeline.top - 24) / 64;
    if (visible < 1)
        visible = 1;

    return visible;
}

void platinum_application_relayout(platinum_application *app)
{
    if (app == NULL || app->window == NULL)
        return;

    platinum_ui_layout_compute(&app->window->portRect, &app->layout);

    if (!app->ui.show_detail) {
        app->layout.timeline.bottom = app->layout.detail.bottom;
        app->layout.timeline_scrollbar.bottom = app->layout.detail.bottom;
        app->layout.navigation.bottom = app->layout.detail.bottom;
        app->layout.detail.top = app->layout.detail.bottom;
    }

    platinum_scrollbar_set_range(
        &app->timeline_scrollbar,
        (short)app->timeline.count,
        platinum_application_timeline_visible_rows(app),
        app->ui.scroll_row);
}

void platinum_application_invalidate(platinum_application *app)
{
    if (app == NULL || app->window == NULL)
        return;

    InvalRect(&app->window->portRect);
}
