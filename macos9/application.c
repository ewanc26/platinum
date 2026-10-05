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
static void platinum_application_open_profile(platinum_application *app);
static void platinum_application_open_notifications(platinum_application *app);
static void platinum_application_refresh_notifications(
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
static unsigned char kWindowMenu[] = { 6, 'W', 'i', 'n', 'd', 'o', 'w' };
static unsigned char kHelpMenu[] = { 4, 'H', 'e', 'l', 'p' };

static unsigned char kPairAccount[] = {
    15, 'P', 'a', 'i', 'r', ' ', 'A', 'c', 'c', 'o', 'u', 'n', 't', '.', '.', '.'
};
static unsigned char kNewPost[] = {
    11, 'N', 'e', 'w', ' ', 'P', 'o', 's', 't', '.', '.', '.'
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

    app->window = NewCWindow(&bounds, 1, 0,
                             documentProc,
                             (WindowPtr)-1L, 1, 0L);
    SetWindowTitle(app->window, kWindowTitle);
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
    platinum_preferences_close(&app->preferences);
    platinum_notifications_close(&app->notifications);
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
            } else if (app->profile.window != NULL &&
                       window == app->profile.window) {
                action = platinum_profile_handle_event(&app->profile, event);
                if (action == PLATINUM_PROFILE_CLOSE) {
                    platinum_profile_close(&app->profile);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
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
                }
                platinum_application_invalidate(app);
            }
            break;

        case updateEvt:
            window = (WindowPtr)(long)event->message;
            if (app->pairing.window != NULL &&
                window == app->pairing.window) {
                platinum_pairing_handle_event(&app->pairing, event);
            } else if (app->preferences.window != NULL &&
                window == app->preferences.window) {
                platinum_preferences_handle_event(&app->preferences,
                                                  event);
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
            if (app->pairing.window != NULL &&
                window == app->pairing.window) {
                platinum_pairing_handle_event(&app->pairing, event);
            } else if (app->preferences.window != NULL &&
                window == app->preferences.window) {
                platinum_preferences_handle_event(&app->preferences,
                                                  event);
            } else if (app->notifications.window != NULL &&
                window == app->notifications.window) {
                platinum_notifications_handle_event(&app->notifications,
                                                    event);
            } else if (app->profile.window != NULL && window == app->profile.window) {
                platinum_profile_handle_event(&app->profile, event);
            } else if (app->compose.window != NULL && window == app->compose.window) {
                platinum_compose_handle_event(&app->compose, event);
            } else if (window == app->window) {
                HiliteWindow(window);
            }
            break;

        case keyDown:
        case autoKey:
            if (app->pairing.window != NULL &&
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
    app->window_menu = NewMenu(kWindowMenuID, kWindowMenu);
    app->help_menu = NewMenu(kHelpMenuID, kHelpMenu);

    if (app->file_menu == NULL || app->edit_menu == NULL ||
        app->view_menu == NULL || app->window_menu == NULL ||
        app->help_menu == NULL) {
        platinum_application_dispose_menus(app);
        return memFullErr;
    }

    AppendMenu(app->file_menu, kNewPost);
    AppendMenu(app->file_menu, kPairAccount);
    AppendMenu(app->file_menu, kCloseWindow);
    AppendMenu(app->file_menu, kQuit);
    SetItemCmdChar(app->file_menu, 1, 'n');
    SetItemCmdChar(app->file_menu, 2, 'k');
    SetItemCmdChar(app->file_menu, 3, 'w');
    SetItemCmdChar(app->file_menu, 4, 'q');

    AppendMenu(app->edit_menu, kUndo);
    AppendMenu(app->edit_menu, kCut);
    AppendMenu(app->edit_menu, kCopy);
    AppendMenu(app->edit_menu, kPaste);
    AppendMenu(app->edit_menu, kSelectAll);
    AppendMenu(app->edit_menu, kPreferences);
    SetItemCmdChar(app->edit_menu, 1, 'z');
    SetItemCmdChar(app->edit_menu, 2, 'x');
    SetItemCmdChar(app->edit_menu, 3, 'c');
    SetItemCmdChar(app->edit_menu, 4, 'v');
    SetItemCmdChar(app->edit_menu, 5, 'a');
    SetItemCmdChar(app->edit_menu, 6, ',');
    DisableItem(app->edit_menu, 1);
    DisableItem(app->edit_menu, 2);
    DisableItem(app->edit_menu, 3);
    DisableItem(app->edit_menu, 4);
    DisableItem(app->edit_menu, 5);

    AppendMenu(app->view_menu, kRefreshMenu);
    AppendMenu(app->view_menu, kShowDetail);
    SetItemCmdChar(app->view_menu, 1, 'r');

    AppendMenu(app->window_menu, kTimelineWindow);
    AppendMenu(app->window_menu, kNotificationsWindow);
    AppendMenu(app->window_menu, kProfileWindow);
    AppendMenu(app->window_menu, kBringAllToFront);

    AppendMenu(app->help_menu, kAbout);
    AppendMenu(app->help_menu, kHelpItem);

    InsertMenu(app->file_menu, 0);
    InsertMenu(app->edit_menu, 0);
    InsertMenu(app->view_menu, 0);
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

    if (menu_id == kFileMenuID) {
        if (item == 1) {
            if (platinum_compose_open(&app->compose) == noErr)
                SelectWindow(app->compose.window);
        } else if (item == 2) {
            if (platinum_application_open_pairing(app) == noErr)
                SelectWindow(app->pairing.window);
        } else if (item == 3) {
            if (app->pairing.window != NULL)
                platinum_pairing_close(&app->pairing);
            else if (app->preferences.window != NULL)
                platinum_preferences_close(&app->preferences);
            else if (app->compose.window != NULL)
                platinum_compose_close(&app->compose);
            else
                app->running = 0;
        } else if (item == 4) {
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
        }
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

/*
 * Wrap a post body as {"text":"..."}.
 *
 * Written out by hand rather than built through a JSON library, because
 * snprintf is not available on this target and the shape is a single member.
 * The escape is the part that matters: the text is what the user typed, so it
 * is the one thing in this request that can contain a quote or a backslash.
 *
 * Sizes the buffers from the escaped length rather than assuming one. Every
 * byte of the encoded text could in principle need a six-byte \u00XX form, so
 * the worst case is six out for each one in.
 */
static char *platinum_application_post_body(const char *utf8_text)
{
    static const char kPrefix[] = "{\"text\":\"";
    static const char kSuffix[] = "\"}";
    size_t escaped_cap;
    size_t body_cap;
    char *escaped;
    char *body;

    escaped_cap = strlen(utf8_text) * 6 + 1;
    body_cap = escaped_cap + sizeof(kPrefix) + sizeof(kSuffix);
    escaped = (char *)malloc(escaped_cap);
    body = (char *)malloc(body_cap);
    if (escaped == NULL || body == NULL) {
        free(escaped);
        free(body);
        return NULL;
    }

    if (platinum_json_escape(escaped, escaped_cap, utf8_text) != WF_OK) {
        free(escaped);
        free(body);
        return NULL;
    }

    body[0] = '\0';
    strcpy(body, kPrefix);
    strcat(body, escaped);
    strcat(body, kSuffix);
    free(escaped);

    return body;
}

static void platinum_application_submit_post(platinum_application *app)
{
    char text[PLATINUM_COMPOSE_MAX_TEXT + 1];
    char utf8_text[PLATINUM_TEXT_UTF8_CAPACITY];
    char *body;
    wf_response response;
    wf_status status;
    platinum_bridge_client *bridge;

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

    body = platinum_application_post_body(utf8_text);
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
    platinum_timeline_init(&app->timeline);
    platinum_pairing_close(&app->pairing);
    SelectWindow(app->window);
    platinum_application_refresh_timeline(app);
    platinum_application_invalidate(app);
}
