#include "application.h"

#include <Events.h>
#include <Fonts.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <TextEdit.h>
#include <Windows.h>

#include <stdlib.h>
#include <string.h>

#include "mac9_tls.h"
#include "ui.h"

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

static unsigned char kWindowTitle[] = {
    15, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', '-', ' ',
    'H', 'o', 'm', 'e'
};

static unsigned char kFileMenu[] = { 4, 'F', 'i', 'l', 'e' };
static unsigned char kEditMenu[] = { 4, 'E', 'd', 'i', 't' };
static unsigned char kViewMenu[] = { 4, 'V', 'i', 'e', 'w' };
static unsigned char kWindowMenu[] = { 6, 'W', 'i', 'n', 'd', 'o', 'w' };
static unsigned char kHelpMenu[] = { 4, 'H', 'e', 'l', 'p' };

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

    app->window = NewCWindow(NULL, bounds, kWindowTitle, true,
                             documentProc, (WindowPtr)-1L, true, 0L);
    if (app->window == NULL) {
        platinum_session_close(&app->session);
        platinum_application_dispose_menus(app);
        return memFullErr;
    }

    app->running = 1;
    wf_macos9_set_yield_callback(platinum_application_yield, app);
    platinum_ui_layout_compute(&app->window->portRect, &app->layout);
    platinum_application_invalidate(app);
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

    if (app->window != NULL) {
        DisposeWindow(app->window);
        app->window = NULL;
    }

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
            } else if (part == inGoAway && window == app->window) {
                app->running = 0;
            } else if (part == inDrag && window == app->window) {
                DragWindow(window, event->where, NULL);
                InvalRect(&window->portRect);
            } else if (app->compose.window != NULL &&
                       window == app->compose.window) {
                action = platinum_compose_handle_event(&app->compose, event);
                if (action == PLATINUM_COMPOSE_CANCEL ||
                    action == PLATINUM_COMPOSE_POST) {
                    platinum_compose_close(&app->compose);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                }
            } else if (window == app->window) {
                action = platinum_ui_handle_mouse(&app->layout,
                                                  &app->ui,
                                                  event->where);
                if (action == PLATINUM_UI_ACTION_QUIT)
                    app->running = 0;
                else if (action == PLATINUM_UI_ACTION_COMPOSE) {
                    if (platinum_compose_open(&app->compose) == noErr)
                        SelectWindow(app->compose.window);
                }
                platinum_application_invalidate(app);
            }
            break;

        case updateEvt:
            window = (WindowPtr)event->message;
            if (app->compose.window != NULL && window == app->compose.window) {
                platinum_compose_handle_event(&app->compose, event);
            } else if (window == app->window) {
                BeginUpdate(window);
                platinum_ui_layout_compute(&window->portRect, &app->layout);
                platinum_application_draw(app);
                EndUpdate(window);
            }
            break;

        case activateEvt:
            window = (WindowPtr)event->message;
            if (app->compose.window != NULL && window == app->compose.window) {
                platinum_compose_handle_event(&app->compose, event);
            } else if (window == app->window) {
                HiliteWindow(window, (event->modifiers & activeFlag) != 0);
            }
            break;

        case keyDown:
        case autoKey:
            if (app->compose.window != NULL &&
                FrontWindow() == app->compose.window) {
                action = platinum_compose_handle_event(&app->compose, event);
                if (action == PLATINUM_COMPOSE_CANCEL ||
                    action == PLATINUM_COMPOSE_POST) {
                    platinum_compose_close(&app->compose);
                    SelectWindow(app->window);
                    platinum_application_invalidate(app);
                }
            } else {
                action = platinum_ui_handle_key(&app->layout, &app->ui, event);
                if (action == PLATINUM_UI_ACTION_QUIT)
                    app->running = 0;
                else if (action == PLATINUM_UI_ACTION_COMPOSE) {
                    if (platinum_compose_open(&app->compose) == noErr)
                        SelectWindow(app->compose.window);
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

    platinum_ui_draw((GrafPtr)app->window,
                     &app->layout,
                     &app->ui,
                     &app->session);

    SetPort(old_port);
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
    AppendMenu(app->file_menu, kCloseWindow);
    AppendMenu(app->file_menu, kQuit);
    SetItemCmdChar(app->file_menu, 1, 'n');
    SetItemCmdChar(app->file_menu, 2, 'w');
    SetItemCmdChar(app->file_menu, 3, 'q');

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
            if (app->compose.window != NULL)
                platinum_compose_close(&app->compose);
            else
                app->running = 0;
        } else if (item == 3) {
            app->running = 0;
        }
    } else if (menu_id == kViewMenuID) {
        if (item == 1) {
            app->ui.scroll_row = 0;
            app->ui.selected_post = 0;
            platinum_application_invalidate(app);
        } else if (item == 2) {
            app->ui.show_detail = !app->ui.show_detail;
            platinum_application_invalidate(app);
        }
    } else if (menu_id == kWindowMenuID) {
        platinum_application_invalidate(app);
    } else if (menu_id == kHelpMenuID) {
        platinum_application_invalidate(app);
    }
}
