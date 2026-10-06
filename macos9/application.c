#include "application.h"

#include <Events.h>
#include <Fonts.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <TextEdit.h>
#include <Windows.h>

#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "timeline.h"
#include "profile.h"
#include "notifications.h"
#include "preferences.h"
#include "pairing.h"
#include "text_codec.h"
#include "scrollbar.h"
#include "json_min.h"
#include "wolfram/macos9_tls.h"

#define kFileMenuID 128
#define kEditMenuID 129
#define kViewMenuID 130
#define kWindowMenuID 131
#define kHelpMenuID 132
#define kPostMenuID 133

static void platinum_application_yield(void *userdata);
static void platinum_application_handle_event(platinum_application *app,
                                              EventRecord *event);
static void platinum_application_draw(platinum_application *app);
static OSErr platinum_application_create_menus(platinum_application *app);
static void platinum_application_dispose_menus(platinum_application *app);
static void platinum_application_handle_menu(platinum_application *app,
                                             long choice);
static void platinum_application_invalidate(platinum_application *app);
static void platinum_application_relayout(platinum_application *app);
static void platinum_application_refresh_timeline(platinum_application *app);
static void platinum_application_load_older(platinum_application *app);
static void platinum_application_engage(platinum_application *app,
                                        int repost);
static void platinum_application_reply(platinum_application *app);
static void platinum_application_quote(platinum_application *app);
static void platinum_application_show_thread(platinum_application *app);
static void platinum_application_show_author(platinum_application *app);
static void platinum_application_show_people(platinum_application *app,
                                             int menu_item);
static void platinum_application_more_people(platinum_application *app);
static void platinum_application_open_person(platinum_application *app);
static void platinum_application_more_posts(platinum_application *app);
static void platinum_application_show_posts(platinum_application *app);
static void platinum_application_run_search(platinum_application *app);
static void platinum_application_show_named(platinum_application *app,
                                            int kind);
static void platinum_application_follow(platinum_application *app);
static void platinum_application_show_words(platinum_application *app);
static void platinum_application_remove_word(platinum_application *app);
static void platinum_application_relate(platinum_application *app, int kind);
static void platinum_application_open_profile(platinum_application *app);
static void platinum_application_open_notifications(platinum_application *app);
static void platinum_application_refresh_notifications(
    platinum_application *app);
static void platinum_application_older_notifications(
    platinum_application *app);
static void platinum_application_recover_auth(
    platinum_application *app,
    wf_status status);
static void platinum_application_open_preferences(
    platinum_application *app);
static void platinum_application_sign_out(
    platinum_application *app);
static OSErr platinum_application_open_pairing(
    platinum_application *app);
static void platinum_application_attempt_pair(
    platinum_application *app);
static void platinum_application_open_apppw(platinum_application *app);
static void platinum_application_attempt_apppw(platinum_application *app);
static short platinum_application_timeline_visible_rows(
    const platinum_application *app);

static void platinum_application_show_pairing_error(
    platinum_application *app,
    const char *message);
static void platinum_application_submit_post(platinum_application *app);
static void platinum_application_post_status(platinum_application *app,
                                             wf_status status);

static unsigned char kWindowTitle[] = {
    15, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', '-', ' ',
    'H', 'o', 'm', 'e'
};

static unsigned char kFileMenu[] = { 4, 'F', 'i', 'l', 'e' };
static unsigned char kEditMenu[] = { 4, 'E', 'd', 'i', 't' };
static unsigned char kViewMenu[] = { 4, 'V', 'i', 'e', 'w' };
static unsigned char kPostMenu[] = { 4, 'P', 'o', 's', 't' };
static unsigned char kLikeItem[] = {
    14, 'L', 'i', 'k', 'e', ' ', 'o', 'r', ' ', 'U', 'n', 'l', 'i', 'k', 'e'
};
static unsigned char kAuthorItem[] = {
    21, 'S', 'h', 'o', 'w', ' ', 'A', 'u', 't', 'h', 'o', 'r', '\'', 's', ' ',
    'P', 'r', 'o', 'f', 'i', 'l', 'e'
};
static unsigned char kFollowItem[] = {
    18, 'F', 'o', 'l', 'l', 'o', 'w', ' ', 'o', 'r', ' ', 'U', 'n', 'f', 'o', 'l', 'l', 'o', 'w'
};
static unsigned char kWordsItem[] = {
    11, 'M', 'u', 't', 'e', 'd', ' ', 'W', 'o', 'r', 'd', 's'
};
static unsigned char kAddWordItem[] = {
    15, 'A', 'd', 'd', ' ', 'M', 'u', 't', 'e', 'd', ' ', 'W', 'o', 'r', 'd'
};
static unsigned char kRemoveWordItem[] = {
    18, 'R', 'e', 'm', 'o', 'v', 'e', ' ', 'M', 'u', 't', 'e', 'd', ' ', 'W', 'o', 'r', 'd'
};
static unsigned char kQuoteItem[] = {
    13, 'Q', 'u', 'o', 't', 'e', ' ', 'P', 'o', 's', 't', '.', '.', '.'
};
static unsigned char kMuteItem[] = {
    14, 'M', 'u', 't', 'e', ' ', 'o', 'r', ' ', 'U', 'n', 'm', 'u', 't', 'e'
};
static unsigned char kBlockItem[] = {
    16, 'B', 'l', 'o', 'c', 'k', ' ', 'o', 'r', ' ', 'U', 'n', 'b', 'l', 'o', 'c', 'k'
};
static unsigned char kAuthorPostsItem[] = {
    19, 'S', 'h', 'o', 'w', ' ', 'A', 'u', 't', 'h', 'o', 'r', '\'', 's', ' ',
    'P', 'o', 's', 't', 's'
};
static unsigned char kFeedsItem[] = {
    11, 'S', 'a', 'v', 'e', 'd', ' ', 'F', 'e', 'e', 'd', 's'
};
static unsigned char kListsItem[] = {
    8, 'M', 'y', ' ', 'L', 'i', 's', 't', 's'
};
static unsigned char kLikersItem[] = {
    14, 'W', 'h', 'o', ' ', 'L', 'i', 'k', 'e', 'd', ' ', 'T', 'h', 'i', 's'
};
static unsigned char kRepostersItem[] = {
    17, 'W', 'h', 'o', ' ', 'R', 'e', 'p', 'o', 's', 't', 'e', 'd', ' ', 'T', 'h', 'i', 's'
};
static unsigned char kFollowersItem[] = {
    14, 'S', 'h', 'o', 'w', ' ', 'F', 'o', 'l', 'l', 'o', 'w', 'e', 'r', 's'
};
static unsigned char kFollowingItem[] = {
    14, 'S', 'h', 'o', 'w', ' ', 'F', 'o', 'l', 'l', 'o', 'w', 'i', 'n', 'g'
};
static unsigned char kReplyItem[] = {
    8, 'R', 'e', 'p', 'l', 'y', '.', '.', '.'
};
static unsigned char kRepostItem[] = {
    21, 'R', 'e', 'p', 'o', 's', 't', ' ', 'o', 'r', ' ', 'U', 'n', 'd', 'o',
    ' ', 'R', 'e', 'p', 'o', 's', 't'
};
static unsigned char kWindowMenu[] = { 6, 'W', 'i', 'n', 'd', 'o', 'w' };
static unsigned char kHelpMenu[] = { 4, 'H', 'e', 'l', 'p' };

static unsigned char kPairAccount[] = {
    15, 'P', 'a', 'i', 'r', ' ', 'A', 'c', 'c', 'o', 'u', 'n', 't', '.', '.', '.'
};
static unsigned char kNewPost[] = {
    11, 'N', 'e', 'w', ' ', 'P', 'o', 's', 't', '.', '.', '.'
};
static unsigned char kAppPassword[] = {
    29, 'S', 'i', 'g', 'n', ' ', 'I', 'n', ' ', 'w', 'i', 't', 'h', ' ', 'A',
    'p', 'p', ' ', 'P', 'a', 's', 's', 'w', 'o', 'r', 'd', '.', '.', '.'
};
static unsigned char kCloseWindow[] = {
    12, 'C', 'l', 'o', 's', 'e', ' ', 'W', 'i', 'n', 'd', 'o', 'w'
};
static unsigned char kQuit[] = { 4, 'Q', 'u', 'i', 't' };
static unsigned char kUndo[] = { 4, 'U', 'n', 'd', 'o' };
static unsigned char kCut[] = { 3, 'C', 'u', 't' };
static unsigned char kCopy[] = { 4, 'C', 'o', 'p', 'y' };
static unsigned char kPaste[] = { 5, 'P', 'a', 's', 't', 'e' };
static unsigned char kSelectAll[] = {
    10, 'S', 'e', 'l', 'e', 'c', 't', ' ', 'A', 'l', 'l'
};
static unsigned char kPreferences[] = {
    14, 'P', 'r', 'e', 'f', 'e', 'r', 'e', 'n', 'c', 'e', 's', '.', '.', '.'
};
static unsigned char kRefreshMenu[] = { 7, 'R', 'e', 'f', 'r', 'e', 's', 'h' };
static unsigned char kLoadOlder[] = {
    16, 'L', 'o', 'a', 'd', ' ', 'O', 'l', 'd', 'e', 'r', ' ', 'P', 'o', 's',
    't', 's'
};
static unsigned char kSearchAccounts[] = {
    18, 'S', 'e', 'a', 'r', 'c', 'h', ' ', 'A', 'c', 'c', 'o', 'u', 'n', 't', 's', '.', '.', '.'
};
static unsigned char kSearchPosts[] = {
    15, 'S', 'e', 'a', 'r', 'c', 'h', ' ', 'P', 'o', 's', 't', 's', '.', '.', '.'
};
static unsigned char kShowThread[] = {
    11, 'S', 'h', 'o', 'w', ' ', 'T', 'h', 'r', 'e', 'a', 'd'
};
static unsigned char kShowDetail[] = {
    11, 'S', 'h', 'o', 'w', ' ', 'D', 'e', 't', 'a', 'i', 'l'
};
static unsigned char kTimelineWindow[] = {
    8, 'T', 'i', 'm', 'e', 'l', 'i', 'n', 'e'
};
static unsigned char kNotificationsWindow[] = {
    13, 'N', 'o', 't', 'i', 'f', 'i', 'c', 'a', 't', 'i', 'o', 'n', 's'
};
static unsigned char kProfileWindow[] = {
    7, 'P', 'r', 'o', 'f', 'i', 'l', 'e'
};
static unsigned char kBringAllToFront[] = {
    18, 'B', 'r', 'i', 'n', 'g', ' ', 'A', 'l', 'l', ' ', 't', 'o', ' ', 'F', 'r', 'o', 'n', 't'
};
static unsigned char kAbout[] = {
    14, 'A', 'b', 'o', 'u', 't', ' ', 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm'
};
static unsigned char kHelpItem[] = {
    13, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', 'H', 'e', 'l', 'p'
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
    platinum_thread_init(&app->thread);
    platinum_people_init(&app->people);
    platinum_search_init(&app->search);
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

    app->window = NewCWindow(NULL, &bounds, kWindowTitle, 1,
                             documentProc, (WindowPtr)-1L, 1, 0L);
    if (app->window == NULL) {
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
        platinum_application_open_pairing(app);

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
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_profile_close(&app->profile);
    platinum_compose_close(&app->compose);
    platinum_session_close(&app->session);
    platinum_application_dispose_menus(app);
}

static void platinum_application_yield(void *userdata)
{
    platinum_application *app;
    EventRecord event;

    app = (platinum_application *)userdata;
    if (app == NULL || !app->running)
        return;

    if (WaitNextEvent(updateMask | activMask, &event, 0, NULL))
        platinum_application_handle_event(app, &event);
}

static void platinum_application_handle_event(platinum_application *app,
                                              EventRecord *event)
{
    WindowPtr window;
    short part;
    long choice;
    int action;

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
                if (window == app->window) {
                    app->running = 0;
                } else if (app->profile.window != NULL &&
                           window == app->profile.window) {
                    platinum_profile_close(&app->profile);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (app->notifications.window != NULL &&
                           window == app->notifications.window) {
                    platinum_notifications_close(&app->notifications);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (app->thread.window != NULL &&
                           window == app->thread.window) {
                    platinum_thread_close(&app->thread);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (app->people.window != NULL &&
                           window == app->people.window) {
                    platinum_people_close(&app->people);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (app->search.window != NULL &&
                           window == app->search.window) {
                    platinum_search_close(&app->search);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (app->apppw.window != NULL &&
                           window == app->apppw.window) {
                    platinum_apppw_close(&app->apppw);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (app->pairing.window != NULL &&
                           window == app->pairing.window) {
                    platinum_pairing_close(&app->pairing);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (app->preferences.window != NULL &&
                           window == app->preferences.window) {
                    platinum_preferences_close(&app->preferences);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (app->compose.window != NULL &&
                           window == app->compose.window) {
                    platinum_compose_close(&app->compose);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                }
            } else if (part == inDrag) {
                DragWindow(window, event->where, NULL);
                InvalRect(&window->portRect);
            } else if (part == inContent && window != NULL &&
                       window != FrontWindow()) {
                /* A click in a window that is not frontmost only brings it to
                 * the front; the click is not delivered to its controls. */
                SelectWindow(window);
            } else if (app->profile.window != NULL &&
                       window == app->profile.window) {
                action = platinum_profile_handle_event(&app->profile, event);
                if (action == PLATINUM_PROFILE_CLOSE) {
                    platinum_profile_close(&app->profile);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                }
            } else if (app->search.window != NULL &&
                       window == app->search.window) {
                action = platinum_search_handle_event(&app->search, event);
                if (action == PLATINUM_SEARCH_CANCEL) {
                    platinum_search_close(&app->search);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_SEARCH_RUN) {
                    platinum_application_run_search(app);
                }
            } else if (app->people.window != NULL &&
                       window == app->people.window) {
                action = platinum_people_handle_event(&app->people, event);
                if (action == PLATINUM_PEOPLE_CLOSE) {
                    platinum_people_close(&app->people);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_PEOPLE_LOAD_MORE) {
                    platinum_application_more_people(app);
                } else if (action == PLATINUM_PEOPLE_OPEN) {
                    platinum_application_open_person(app);
                }
            } else if (app->thread.window != NULL &&
                       window == app->thread.window) {
                action = platinum_thread_handle_event(&app->thread, event);
                if (action == PLATINUM_THREAD_CLOSE) {
                    platinum_thread_close(&app->thread);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_THREAD_LOAD_MORE) {
                    platinum_application_more_posts(app);
                }
            } else if (app->notifications.window != NULL &&
                       window == app->notifications.window) {
                action = platinum_notifications_handle_event(
                    &app->notifications, event);
                if (action == PLATINUM_NOTIFICATIONS_CLOSE) {
                    platinum_notifications_close(&app->notifications);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_NOTIFICATIONS_REFRESH) {
                    platinum_application_refresh_notifications(app);
                } else if (action == PLATINUM_NOTIFICATIONS_LOAD_OLDER) {
                    platinum_application_older_notifications(app);
                }
            } else if (app->apppw.window != NULL &&
                       window == app->apppw.window) {
                action = platinum_apppw_handle_event(&app->apppw, event);
                if (action == PLATINUM_APPPW_CANCEL) {
                    platinum_apppw_close(&app->apppw);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_APPPW_SIGN_IN) {
                    platinum_application_attempt_apppw(app);
                }
            } else if (app->pairing.window != NULL &&
                       window == app->pairing.window) {
                action = platinum_pairing_handle_event(&app->pairing, event);
                if (action == PLATINUM_PAIRING_CANCEL) {
                    platinum_pairing_close(&app->pairing);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_PAIRING_PAIR) {
                    platinum_application_attempt_pair(app);
                }
            } else if (app->preferences.window != NULL &&
                       window == app->preferences.window) {
                action = platinum_preferences_handle_event(
                    &app->preferences, event);
                if (action == PLATINUM_PREFERENCES_CLOSE) {
                    platinum_preferences_close(&app->preferences);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_PREFERENCES_SIGN_OUT) {
                    platinum_application_sign_out(app);
                } else if (action == PLATINUM_PREFERENCES_PAIR) {
                    platinum_preferences_close(&app->preferences);
                    platinum_application_open_pairing(app);
                }
            } else if (app->compose.window != NULL &&
                       window == app->compose.window) {
                action = platinum_compose_handle_event(&app->compose, event);
                if (action == PLATINUM_COMPOSE_CANCEL) {
                    platinum_compose_close(&app->compose);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_COMPOSE_POST) {
                    platinum_application_submit_post(app);
                }
            } else if (window == app->window) {
                {
                    short scrollbar_value;
                    if (platinum_scrollbar_handle_mouse(
                            &app->timeline_scrollbar,
                            event,
                            &scrollbar_value)) {
                        app->ui.scroll_row = scrollbar_value;
                        platinum_application_invalidate(app);
                        break;
                    }
                }

                {
                    Point local_where;

                    local_where = event->where;
                    SetPort((GrafPtr)app->window);
                    GlobalToLocal(&local_where);

                    action = platinum_ui_handle_mouse(
                        &app->layout,
                        &app->ui,
                        &app->timeline,
                        local_where);
                }
                if (action == PLATINUM_UI_ACTION_QUIT)
                    app->running = 0;
                else if (action == PLATINUM_UI_ACTION_REFRESH)
                    platinum_application_refresh_timeline(app);
                else if (action == PLATINUM_UI_ACTION_COMPOSE) {
                    if (platinum_compose_open(&app->compose) == noErr)
                        SelectWindow(app->compose.window);
                } else if (action == PLATINUM_UI_ACTION_PROFILE) {
                    platinum_application_open_profile(app);
                } else if (action == PLATINUM_UI_ACTION_NOTIFICATIONS) {
                    platinum_application_open_notifications(app);
                } else if (action == PLATINUM_UI_ACTION_LOAD_OLDER) {
                    platinum_application_load_older(app);
                } else if (action == PLATINUM_UI_ACTION_LIKE) {
                    platinum_application_engage(app, 0);
                } else if (action == PLATINUM_UI_ACTION_REPOST) {
                    platinum_application_engage(app, 1);
                } else if (action == PLATINUM_UI_ACTION_REPLY) {
                    platinum_application_reply(app);
                } else if (action == PLATINUM_UI_ACTION_THREAD) {
                    platinum_application_show_thread(app);
                } else if (action == PLATINUM_UI_ACTION_AUTHOR) {
                    platinum_application_show_author(app);
                } else if (action == PLATINUM_UI_ACTION_FOLLOW) {
                    platinum_application_follow(app);
                }
                platinum_application_invalidate(app);
            }
            break;

        case updateEvt:
            window = (WindowPtr)(long)event->message;
            if (app->apppw.window != NULL &&
                window == app->apppw.window) {
                platinum_apppw_handle_event(&app->apppw, event);
            } else if (app->pairing.window != NULL &&
                window == app->pairing.window) {
                platinum_pairing_handle_event(&app->pairing, event);
            } else if (app->preferences.window != NULL &&
                window == app->preferences.window) {
                platinum_preferences_handle_event(&app->preferences,
                                                  event);
            } else if (app->search.window != NULL &&
                window == app->search.window) {
                platinum_search_handle_event(&app->search, event);
            } else if (app->people.window != NULL &&
                window == app->people.window) {
                platinum_people_handle_event(&app->people, event);
            } else if (app->thread.window != NULL &&
                window == app->thread.window) {
                platinum_thread_handle_event(&app->thread, event);
            } else if (app->notifications.window != NULL &&
                window == app->notifications.window) {
                platinum_notifications_handle_event(&app->notifications,
                                                    event);
            } else if (app->profile.window != NULL && window == app->profile.window) {
                platinum_profile_handle_event(&app->profile, event);
            } else if (app->compose.window != NULL && window == app->compose.window) {
                platinum_compose_handle_event(&app->compose, event);
            } else if (window == app->window) {
                BeginUpdate(window);
                platinum_application_relayout(app);
                platinum_application_draw(app);
                EndUpdate(window);
            }
            break;

        case activateEvt:
            window = (WindowPtr)(long)event->message;
            if (app->apppw.window != NULL &&
                window == app->apppw.window) {
                platinum_apppw_handle_event(&app->apppw, event);
            } else if (app->pairing.window != NULL &&
                window == app->pairing.window) {
                platinum_pairing_handle_event(&app->pairing, event);
            } else if (app->preferences.window != NULL &&
                window == app->preferences.window) {
                platinum_preferences_handle_event(&app->preferences,
                                                  event);
            } else if (app->search.window != NULL &&
                window == app->search.window) {
                platinum_search_handle_event(&app->search, event);
            } else if (app->people.window != NULL &&
                window == app->people.window) {
                platinum_people_handle_event(&app->people, event);
            } else if (app->thread.window != NULL &&
                window == app->thread.window) {
                platinum_thread_handle_event(&app->thread, event);
            } else if (app->notifications.window != NULL &&
                window == app->notifications.window) {
                platinum_notifications_handle_event(&app->notifications,
                                                    event);
            } else if (app->profile.window != NULL && window == app->profile.window) {
                platinum_profile_handle_event(&app->profile, event);
            } else if (app->compose.window != NULL && window == app->compose.window) {
                platinum_compose_handle_event(&app->compose, event);
            } else if (window == app->window) {
                HiliteWindow(window, (event->modifiers & activeFlag) != 0);
            }
            break;

        case keyDown:
        case autoKey:
            if (app->apppw.window != NULL &&
                FrontWindow() == app->apppw.window) {
                action = platinum_apppw_handle_event(&app->apppw, event);
                if (action == PLATINUM_APPPW_CANCEL) {
                    platinum_apppw_close(&app->apppw);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_APPPW_SIGN_IN) {
                    platinum_application_attempt_apppw(app);
                }
            } else if (app->pairing.window != NULL &&
                FrontWindow() == app->pairing.window) {
                action = platinum_pairing_handle_event(&app->pairing, event);
                if (action == PLATINUM_PAIRING_CANCEL) {
                    platinum_pairing_close(&app->pairing);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_PAIRING_PAIR) {
                    platinum_application_attempt_pair(app);
                }
            } else if (app->preferences.window != NULL &&
                FrontWindow() == app->preferences.window) {
                action = platinum_preferences_handle_event(
                    &app->preferences, event);
                if (action == PLATINUM_PREFERENCES_CLOSE) {
                    platinum_preferences_close(&app->preferences);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_PREFERENCES_SIGN_OUT) {
                    platinum_application_sign_out(app);
                } else if (action == PLATINUM_PREFERENCES_PAIR) {
                    platinum_preferences_close(&app->preferences);
                    platinum_application_open_pairing(app);
                }
            } else if (app->search.window != NULL &&
                FrontWindow() == app->search.window) {
                action = platinum_search_handle_event(&app->search, event);
                if (action == PLATINUM_SEARCH_CANCEL) {
                    platinum_search_close(&app->search);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_SEARCH_RUN) {
                    platinum_application_run_search(app);
                }
            } else if (app->people.window != NULL &&
                FrontWindow() == app->people.window) {
                action = platinum_people_handle_event(&app->people, event);
                if (action == PLATINUM_PEOPLE_CLOSE) {
                    platinum_people_close(&app->people);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_PEOPLE_LOAD_MORE) {
                    platinum_application_more_people(app);
                } else if (action == PLATINUM_PEOPLE_OPEN) {
                    platinum_application_open_person(app);
                }
            } else if (app->thread.window != NULL &&
                FrontWindow() == app->thread.window) {
                action = platinum_thread_handle_event(&app->thread, event);
                if (action == PLATINUM_THREAD_CLOSE) {
                    platinum_thread_close(&app->thread);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_THREAD_LOAD_MORE) {
                    platinum_application_more_posts(app);
                }
            } else if (app->notifications.window != NULL &&
                FrontWindow() == app->notifications.window) {
                action = platinum_notifications_handle_event(
                    &app->notifications, event);
                if (action == PLATINUM_NOTIFICATIONS_CLOSE) {
                    platinum_notifications_close(&app->notifications);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_NOTIFICATIONS_REFRESH) {
                    platinum_application_refresh_notifications(app);
                } else if (action == PLATINUM_NOTIFICATIONS_LOAD_OLDER) {
                    platinum_application_older_notifications(app);
                }
            } else if (app->profile.window != NULL &&
                       FrontWindow() == app->profile.window) {
                action = platinum_profile_handle_event(&app->profile, event);
                if (action == PLATINUM_PROFILE_CLOSE) {
                    platinum_profile_close(&app->profile);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                }
            } else if (app->compose.window != NULL &&
                FrontWindow() == app->compose.window) {
                action = platinum_compose_handle_event(&app->compose, event);
                if (action == PLATINUM_COMPOSE_CANCEL) {
                    platinum_compose_close(&app->compose);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                } else if (action == PLATINUM_COMPOSE_POST) {
                    platinum_application_submit_post(app);
                }
            } else {
                action = platinum_ui_handle_key(&app->layout,
                                                &app->ui,
                                                &app->timeline,
                                                event);
                if (action == PLATINUM_UI_ACTION_QUIT)
                    app->running = 0;
                else if (action == PLATINUM_UI_ACTION_REFRESH)
                    platinum_application_refresh_timeline(app);
                else if (action == PLATINUM_UI_ACTION_COMPOSE) {
                    if (platinum_compose_open(&app->compose) == noErr)
                        SelectWindow(app->compose.window);
                } else if (action == PLATINUM_UI_ACTION_PROFILE) {
                    platinum_application_open_profile(app);
                } else if (action == PLATINUM_UI_ACTION_NOTIFICATIONS) {
                    platinum_application_open_notifications(app);
                }
                platinum_application_invalidate(app);
            }
            break;

        default:
            break;
    }
}

static void platinum_application_draw(platinum_application *app)
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

static void platinum_application_refresh_timeline(platinum_application *app)
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
static void platinum_application_show_author(platinum_application *app)
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
static void platinum_application_show_people(platinum_application *app,
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

static void platinum_application_more_people(platinum_application *app)
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
static void platinum_application_run_search(platinum_application *app)
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
static void platinum_application_show_words(platinum_application *app)
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
static void platinum_application_remove_word(platinum_application *app)
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
static void platinum_application_show_posts(platinum_application *app)
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

static void platinum_application_more_posts(platinum_application *app)
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
static void platinum_application_show_named(platinum_application *app,
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
static void platinum_application_open_person(platinum_application *app)
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
static void platinum_application_follow(platinum_application *app)
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
static void platinum_application_relate(platinum_application *app, int kind)
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
static void platinum_application_show_thread(platinum_application *app)
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

/* Open a compose window that answers the selected post. */
static void platinum_application_reply(platinum_application *app)
{
    const platinum_post_preview *post;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    if (app->compose.window != NULL) {
        SelectWindow(app->compose.window);
        return;
    }

    post = &app->timeline.posts[index];
    if (platinum_compose_open(&app->compose) != noErr)
        return;
    (void)platinum_compose_set_reply(&app->compose, post->uri, post->cid,
                                     post->handle);
    SelectWindow(app->compose.window);
    platinum_application_invalidate(app);
}

/* Open compose as a quote of the selected post. */
static void platinum_application_quote(platinum_application *app)
{
    const platinum_post_preview *post;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    if (app->compose.window != NULL) {
        SelectWindow(app->compose.window);
        return;
    }

    post = &app->timeline.posts[index];
    if (platinum_compose_open(&app->compose) != noErr)
        return;
    (void)platinum_compose_set_quote(&app->compose, post->uri, post->cid,
                                     post->handle);
    SelectWindow(app->compose.window);
    platinum_application_invalidate(app);
}

/* Like or repost the selected post, or undo it if the row says it is done. */
static void platinum_application_engage(platinum_application *app, int repost)
{
    platinum_bridge_client *bridge;
    const platinum_post_preview *post;
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

    post = &app->timeline.posts[index];
    status = platinum_timeline_set_engagement(
        &app->timeline, bridge, (unsigned short)index, repost,
        repost ? !post->reposted : !post->liked);
    platinum_application_recover_auth(app, status);
    platinum_application_invalidate(app);
}

static void platinum_application_load_older(platinum_application *app)
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

static short platinum_application_timeline_visible_rows(
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

static void platinum_application_relayout(platinum_application *app)
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

static void platinum_application_invalidate(platinum_application *app)
{
    if (app == NULL || app->window == NULL)
        return;

    InvalRect(&app->window->portRect);
}

static OSErr platinum_application_create_menus(platinum_application *app)
{
    if (app == NULL)
        return paramErr;

    app->file_menu = NewMenu(kFileMenuID, kFileMenu);
    app->edit_menu = NewMenu(kEditMenuID, kEditMenu);
    app->view_menu = NewMenu(kViewMenuID, kViewMenu);
    app->post_menu = NewMenu(kPostMenuID, kPostMenu);
    app->window_menu = NewMenu(kWindowMenuID, kWindowMenu);
    app->help_menu = NewMenu(kHelpMenuID, kHelpMenu);

    if (app->file_menu == NULL || app->edit_menu == NULL ||
        app->view_menu == NULL || app->post_menu == NULL ||
        app->window_menu == NULL ||
        app->help_menu == NULL) {
        platinum_application_dispose_menus(app);
        return memFullErr;
    }

    AppendMenu(app->file_menu, kNewPost);
    AppendMenu(app->file_menu, kPairAccount);
    AppendMenu(app->file_menu, kAppPassword);
    AppendMenu(app->file_menu, kCloseWindow);
    AppendMenu(app->file_menu, kQuit);
    SetItemCmd(app->file_menu, 1, 'n');
    SetItemCmd(app->file_menu, 2, 'k');
    SetItemCmd(app->file_menu, 4, 'w');
    SetItemCmd(app->file_menu, 5, 'q');

    AppendMenu(app->edit_menu, kUndo);
    AppendMenu(app->edit_menu, kCut);
    AppendMenu(app->edit_menu, kCopy);
    AppendMenu(app->edit_menu, kPaste);
    AppendMenu(app->edit_menu, kSelectAll);
    AppendMenu(app->edit_menu, kPreferences);
    SetItemCmd(app->edit_menu, 1, 'z');
    SetItemCmd(app->edit_menu, 2, 'x');
    SetItemCmd(app->edit_menu, 3, 'c');
    SetItemCmd(app->edit_menu, 4, 'v');
    SetItemCmd(app->edit_menu, 5, 'a');
    SetItemCmd(app->edit_menu, 6, ',');
    DisableItem(app->edit_menu, 1);
    DisableItem(app->edit_menu, 2);
    DisableItem(app->edit_menu, 3);
    DisableItem(app->edit_menu, 4);
    DisableItem(app->edit_menu, 5);

    AppendMenu(app->view_menu, kRefreshMenu);
    AppendMenu(app->view_menu, kShowDetail);
    AppendMenu(app->view_menu, kLoadOlder);
    AppendMenu(app->view_menu, kShowThread);
    SetItemCmd(app->view_menu, 4, 't');
    AppendMenu(app->view_menu, kFeedsItem);
    AppendMenu(app->view_menu, kListsItem);
    AppendMenu(app->view_menu, kSearchAccounts);
    AppendMenu(app->view_menu, kSearchPosts);
    SetItemCmd(app->view_menu, 8, 'f');
    AppendMenu(app->view_menu, kWordsItem);
    AppendMenu(app->view_menu, kAddWordItem);
    AppendMenu(app->view_menu, kRemoveWordItem);
    SetItemCmd(app->view_menu, 1, 'r');

    AppendMenu(app->post_menu, kLikeItem);
    AppendMenu(app->post_menu, kRepostItem);
    AppendMenu(app->post_menu, kReplyItem);
    AppendMenu(app->post_menu, kAuthorItem);
    AppendMenu(app->post_menu, kFollowItem);
    AppendMenu(app->post_menu, kLikersItem);
    AppendMenu(app->post_menu, kRepostersItem);
    AppendMenu(app->post_menu, kFollowersItem);
    AppendMenu(app->post_menu, kFollowingItem);
    AppendMenu(app->post_menu, kAuthorPostsItem);
    AppendMenu(app->post_menu, kMuteItem);
    AppendMenu(app->post_menu, kBlockItem);
    AppendMenu(app->post_menu, kQuoteItem);
    SetItemCmd(app->post_menu, 1, 'l');
    SetItemCmd(app->post_menu, 2, 'e');
    SetItemCmd(app->post_menu, 3, 'j');
    SetItemCmd(app->post_menu, 4, 'i');
    SetItemCmd(app->post_menu, 5, 'y');

    AppendMenu(app->window_menu, kTimelineWindow);
    AppendMenu(app->window_menu, kNotificationsWindow);
    AppendMenu(app->window_menu, kProfileWindow);
    AppendMenu(app->window_menu, kBringAllToFront);

    AppendMenu(app->help_menu, kAbout);
    AppendMenu(app->help_menu, kHelpItem);

    InsertMenu(app->file_menu, 0);
    InsertMenu(app->edit_menu, 0);
    InsertMenu(app->view_menu, 0);
    InsertMenu(app->post_menu, 0);
    InsertMenu(app->window_menu, 0);
    InsertMenu(app->help_menu, 0);
    DrawMenuBar();

    return noErr;
}

static void platinum_application_dispose_menus(platinum_application *app)
{
    if (app == NULL)
        return;

    if (app->help_menu != NULL) {
        DeleteMenu(kHelpMenuID);
        DisposeMenu(app->help_menu);
        app->help_menu = NULL;
    }
    if (app->window_menu != NULL) {
        DeleteMenu(kWindowMenuID);
        DisposeMenu(app->window_menu);
        app->window_menu = NULL;
    }
    if (app->post_menu != NULL) {
        DeleteMenu(kPostMenuID);
        DisposeMenu(app->post_menu);
        app->post_menu = NULL;
    }
    if (app->view_menu != NULL) {
        DeleteMenu(kViewMenuID);
        DisposeMenu(app->view_menu);
        app->view_menu = NULL;
    }
    if (app->edit_menu != NULL) {
        DeleteMenu(kEditMenuID);
        DisposeMenu(app->edit_menu);
        app->edit_menu = NULL;
    }
    if (app->file_menu != NULL) {
        DeleteMenu(kFileMenuID);
        DisposeMenu(app->file_menu);
        app->file_menu = NULL;
    }

    DrawMenuBar();
}

static void platinum_application_handle_menu(platinum_application *app,
                                             long choice)
{
    short menu_id;
    short item;

    if (app == NULL || choice == 0)
        return;

    menu_id = (short)((choice >> 16) & 0xFFFF);
    item = (short)(choice & 0xFFFF);

    if (!(menu_id == kPostMenuID && item == 12))
        app->profile.block_armed = 0;

    if (menu_id == kFileMenuID) {
        if (item == 1) {
            if (platinum_compose_open(&app->compose) == noErr)
                SelectWindow(app->compose.window);
        } else if (item == 2) {
            if (platinum_application_open_pairing(app) == noErr)
                SelectWindow(app->pairing.window);
        } else if (item == 3) {
            platinum_application_open_apppw(app);
        } else if (item == 4) {
            if (app->apppw.window != NULL)
                platinum_apppw_close(&app->apppw);
            else if (app->pairing.window != NULL)
                platinum_pairing_close(&app->pairing);
            else if (app->preferences.window != NULL)
                platinum_preferences_close(&app->preferences);
            else if (app->compose.window != NULL)
                platinum_compose_close(&app->compose);
            else
                app->running = 0;
        } else if (item == 5) {
            app->running = 0;
        }
    } else if (menu_id == kEditMenuID) {
        if (item == 6)
            platinum_application_open_preferences(app);
    } else if (menu_id == kViewMenuID) {
        if (item == 1) {
            platinum_application_refresh_timeline(app);
        } else if (item == 2) {
            app->ui.show_detail = !app->ui.show_detail;
            platinum_application_invalidate(app);
        } else if (item == 3) {
            platinum_application_load_older(app);
        } else if (item == 4) {
            platinum_application_show_thread(app);
        } else if (item == 5) {
            platinum_application_show_named(app, PLATINUM_PEOPLE_FEEDS);
        } else if (item == 6) {
            platinum_application_show_named(app, PLATINUM_PEOPLE_LISTS);
        } else if (item == 9) {
            platinum_application_show_words(app);
        } else if (item == 10) {
            if (platinum_search_open(&app->search, PLATINUM_SEARCH_WORD) == noErr)
                SelectWindow(app->search.window);
        } else if (item == 11) {
            platinum_application_remove_word(app);
        } else if (item == 7 || item == 8) {
            if (platinum_search_open(&app->search, item == 8 ? PLATINUM_SEARCH_POSTS : PLATINUM_SEARCH_ACCOUNTS) == noErr)
                SelectWindow(app->search.window);
        }
    } else if (menu_id == kPostMenuID) {
        if (item == 13)
            platinum_application_quote(app);
        else if (item == 12)
            platinum_application_relate(app, PLATINUM_RELATION_BLOCK);
        else if (item == 11)
            platinum_application_relate(app, PLATINUM_RELATION_MUTE);
        else if (item == 10)
            platinum_application_show_posts(app);
        else if (item >= 6 && item <= 9)
            platinum_application_show_people(app, item);
        else if (item == 4)
            platinum_application_show_author(app);
        else if (item == 5)
            platinum_application_follow(app);
        else if (item == 3)
            platinum_application_reply(app);
        else
            platinum_application_engage(app, item == 2);
    } else if (menu_id == kWindowMenuID) {
        if (item == 2)
            platinum_application_open_notifications(app);
        else if (item == 3)
            platinum_application_open_profile(app);
        else
            platinum_application_invalidate(app);
    } else if (menu_id == kHelpMenuID) {
        platinum_application_invalidate(app);
    }
}

static void platinum_application_post_status(platinum_application *app,
                                             wf_status status)
{
    if (app == NULL || app->compose.window == NULL)
        return;

    if (status == WF_ERR_AUTH)
        platinum_compose_set_status(&app->compose,
                                    "Session expired. Use File > Pair Account; your draft is preserved.");
    else if (status == WF_ERR_HTTP)
        platinum_compose_set_status(&app->compose,
                                    "The bridge rejected the post.");
    else if (status == WF_ERR_NETWORK || status == WF_ERR_TIMEOUT)
        platinum_compose_set_status(&app->compose,
                                    "The bridge could not be reached.");
    else
        platinum_compose_set_status(&app->compose,
                                    "The post could not be sent.");
}

static void platinum_application_submit_post(platinum_application *app)
{
    char text[PLATINUM_COMPOSE_MAX_TEXT + 1];
    char utf8_text[PLATINUM_TEXT_UTF8_CAPACITY];
    char *body;
    wf_response response;
    wf_status status;
    platinum_bridge_client *bridge;
    int gate_failed;

    if (app == NULL || app->compose.window == NULL)
        return;

    if (!platinum_session_is_paired(&app->session)) {
        platinum_compose_set_status(&app->compose,
                                    "Pair an account before posting.");
        return;
    }

    if (platinum_compose_get_text(&app->compose,
                                  text,
                                  sizeof(text)) != noErr) {
        platinum_compose_set_status(&app->compose,
                                    "Enter up to 300 MacRoman characters.");
        return;
    }

    if (platinum_text_macroman_to_utf8(text,
                                       utf8_text,
                                       sizeof(utf8_text),
                                       NULL) < 0) {
        platinum_compose_set_status(&app->compose,
                                    "The post text could not be encoded.");
        return;
    }

    body = platinum_bridge_post_body_ex(utf8_text,
                                        app->compose.reply_uri,
                                        app->compose.reply_cid,
                                        app->compose.quote_uri,
                                        app->compose.quote_cid,
                                        app->compose.reply_gate);
    if (body == NULL) {
        platinum_compose_set_status(&app->compose,
                                    "Not enough memory to prepare the post.");
        return;
    }

    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL) {
        free(body);
        platinum_compose_set_status(&app->compose,
                                    "The bridge session is unavailable.");
        return;
    }

    platinum_compose_set_posting(&app->compose, 1);
    platinum_application_invalidate(app);
    memset(&response, 0, sizeof(response));

    status = platinum_bridge_post(bridge, "/v1/post", body, &response);
    free(body);
    gate_failed = status == WF_OK && response.body != NULL &&
                  platinum_bridge_reply_gate_failed(response.body);
    wf_response_free(&response);
    platinum_compose_set_posting(&app->compose, 0);

    if (status != WF_OK) {
        platinum_application_post_status(app, status);
        return;
    }

    platinum_compose_set_status(&app->compose, NULL);
    platinum_compose_close(&app->compose);
    SelectWindow(app->window);
    platinum_application_refresh_timeline(app);
    if (gate_failed) {
        /* The post is out; say plainly that the limit was not set. */
        strcpy(app->timeline.status,
               "Posted, but I could not limit who can reply. Anyone can.");
    }
    platinum_application_invalidate(app);
}

static void platinum_application_open_profile(platinum_application *app)
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

static void platinum_application_open_notifications(platinum_application *app)
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

static void platinum_application_older_notifications(
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

static void platinum_application_refresh_notifications(
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

static void platinum_application_open_preferences(
    platinum_application *app)
{
    if (app == NULL)
        return;

    platinum_preferences_open(&app->preferences, &app->session);
}

static void platinum_application_sign_out(
    platinum_application *app)
{
    wf_status status;

    if (app == NULL)
        return;

    status = platinum_session_sign_out(&app->session);

    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_apppw_close(&app->apppw);
    platinum_timeline_init(&app->timeline);
    app->ui.selected_post = 0;
    app->ui.scroll_row = 0;

    if (status != WF_OK) {
        platinum_preferences_set_status(
            &app->preferences,
            "Signed out locally; bridge revocation may have failed.");
        platinum_application_invalidate(app);
        return;
    }

    platinum_preferences_close(&app->preferences);
    SelectWindow(app->window);
    platinum_application_invalidate(app);
}

static void platinum_application_recover_auth(platinum_application *app,
                                             wf_status status)
{
    if (app == NULL || status != WF_ERR_AUTH)
        return;

    /* Already unpaired: a stale window must not reopen the pairing dialog on
     * every refresh once the session is gone. */
    if (!platinum_session_is_paired(&app->session))
        return;

    /* The bridge token was refused, so it is gone or revoked. Drop the session
     * so the UI shows the unpaired state, clear what it was showing, and give
     * the user somewhere to go next instead of failing silently on every
     * subsequent refresh. Revocation is not attempted: the token is already
     * unusable, and there is nothing left to revoke server-side. */
    (void)platinum_session_sign_out(&app->session);
    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_timeline_init(&app->timeline);
    app->ui.selected_post = 0;
    app->ui.scroll_row = 0;

    (void)platinum_application_open_pairing(app);
    platinum_application_invalidate(app);
}

static OSErr platinum_application_open_pairing(
    platinum_application *app)
{
    const platinum_config *config;
    const char *bridge_url;
    OSErr status;

    if (app == NULL)
        return paramErr;

    if (app->pairing.window != NULL) {
        SelectWindow(app->pairing.window);
        return noErr;
    }

    config = platinum_session_config(&app->session);
    bridge_url = config != NULL ? config->bridge_url : NULL;
    status = platinum_pairing_open(&app->pairing, bridge_url);
    if (status == noErr)
        SelectWindow(app->pairing.window);
    return status;
}

static void platinum_application_show_pairing_error(
    platinum_application *app,
    const char *message)
{
    if (app == NULL || app->pairing.window == NULL)
        return;

    platinum_pairing_set_status(&app->pairing, message);
    SelectWindow(app->pairing.window);
}

/* The window for signing in with an app password. */
static void platinum_application_open_apppw(platinum_application *app)
{
    const platinum_config *config;

    if (app == NULL)
        return;
    config = platinum_session_config(&app->session);
    if (platinum_apppw_open(&app->apppw,
                            config != NULL ? config->bridge_url : NULL) == noErr)
        SelectWindow(app->apppw.window);
}

/*
 * Sign in with the handle and password in the window. Plain http asks twice:
 * the first request only warns. However it ends, the password is wiped from the
 * entry buffer and from the local copy, and the window shows only bullets.
 */
static void platinum_application_attempt_apppw(platinum_application *app)
{
    char bridge_url[PLATINUM_APPPW_URL_MAX + 1];
    char handle[PLATINUM_APPPW_HANDLE_MAX * 4 + 1];
    char password[PLATINUM_SECRET_MAX * 4 + 1];
    wf_status status;
    int reason;

    if (app == NULL || app->apppw.window == NULL)
        return;

    if (platinum_apppw_get_bridge_url(&app->apppw, bridge_url,
                                      sizeof(bridge_url)) != noErr) {
        platinum_apppw_set_status(&app->apppw, "Enter a bridge URL.");
        return;
    }
    if (platinum_apppw_get_handle(&app->apppw, handle, sizeof(handle)) != noErr) {
        platinum_apppw_set_status(&app->apppw, "Enter your handle.");
        return;
    }
    if (platinum_secret_utf8(&app->apppw.password, password,
                             sizeof(password)) < 0) {
        platinum_bridge_wipe(password, sizeof(password));
        platinum_apppw_set_status(&app->apppw, "Enter the app password.");
        return;
    }

    if (platinum_apppw_is_plain_http(bridge_url) && !app->apppw.http_armed) {
        app->apppw.http_armed = 1;
        platinum_bridge_wipe(password, sizeof(password));
        platinum_apppw_set_status(
            &app->apppw,
            "That address is plain http. Choose Sign In again to send it anyway.");
        return;
    }

    if (platinum_session_set_bridge_url(&app->session, bridge_url) != noErr) {
        platinum_bridge_wipe(password, sizeof(password));
        platinum_apppw_set_status(&app->apppw,
                                  "The bridge URL could not be saved.");
        return;
    }

    reason = PLATINUM_LOGIN_OTHER;
    status = platinum_session_sign_in_app_password(&app->session, handle,
                                                   password, &reason);
    platinum_bridge_wipe(password, sizeof(password));
    platinum_secret_wipe(&app->apppw.password);
    app->apppw.http_armed = 0;

    if (status != WF_OK) {
        platinum_apppw_set_status(
            &app->apppw,
            reason == PLATINUM_LOGIN_DISABLED
                ? "This bridge does not allow app-password sign-in."
            : reason == PLATINUM_LOGIN_INVALID
                ? "The handle or app password is not valid."
            : reason == PLATINUM_LOGIN_TOO_MANY
                ? "Too many failed attempts. Try again later."
            : reason == PLATINUM_LOGIN_BAD_SERVICE
                ? "The bridge does not accept that account service."
                : "Sign-in failed. Check the bridge address and try again.");
        return;
    }

    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_pairing_close(&app->pairing);
    platinum_timeline_init(&app->timeline);
    platinum_apppw_close(&app->apppw);
    SelectWindow(app->window);
    platinum_application_refresh_timeline(app);
    platinum_application_invalidate(app);
}

static void platinum_application_attempt_pair(
    platinum_application *app)
{
    char bridge_url[PLATINUM_PAIRING_URL_MAX + 1];
    char code[PLATINUM_PAIRING_CODE_MAX + 1];
    wf_status status;

    if (app == NULL || app->pairing.window == NULL)
        return;

    if (platinum_pairing_get_bridge_url(&app->pairing,
                                        bridge_url,
                                        sizeof(bridge_url)) != noErr) {
        platinum_application_show_pairing_error(
            app, "Enter a bridge URL.");
        return;
    }

    if (platinum_pairing_get_code(&app->pairing,
                                  code,
                                  sizeof(code)) != noErr) {
        platinum_application_show_pairing_error(
            app, "Enter a six-character pairing code.");
        return;
    }

    if (platinum_session_set_bridge_url(&app->session,
                                        bridge_url) != noErr) {
        platinum_application_show_pairing_error(
            app, "The bridge URL could not be saved.");
        return;
    }

    status = platinum_session_pair(&app->session, code);
    if (status != WF_OK) {
        platinum_application_show_pairing_error(
            app, "Pairing failed. Check the bridge and code.");
        return;
    }

    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_timeline_init(&app->timeline);
    platinum_pairing_close(&app->pairing);
    SelectWindow(app->window);
    platinum_application_refresh_timeline(app);
    platinum_application_invalidate(app);
}
