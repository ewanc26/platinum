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

static void platinum_application_yield(void *userdata);
static void platinum_application_handle_event(platinum_application *app,
                                              EventRecord *event);
static void platinum_application_draw(platinum_application *app);

static unsigned char kWindowTitle[] = {
    8, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm'
};

static unsigned char kTitle[] = {
    8, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm'
};

static unsigned char kBridgeReady[] = {
    13, 'B', 'r', 'i', 'd', 'g', 'e', ' ', 'r', 'e', 'a', 'd', 'y'
};

static unsigned char kNotPaired[] = {
    10, 'N', 'o', 't', ' ', 'p', 'a', 'i', 'r', 'e', 'd'
};

static unsigned char kPaired[] = {
    6, 'P', 'a', 'i', 'r', 'e', 'd'
};

static unsigned char kNextStep[] = {
    46, 'B', 'r', 'i', 'd', 'g', 'e', ' ', 'c', 'o', 'n', 'f', 'i', 'g', 'u', 'r',
    'a', 't', 'i', 'o', 'n', ' ', 'a', 'n', 'd', ' ', 'p', 'a', 'i', 'r', 'i', 'n',
    'g', ' ', 'U', 'I', ' ', 'c', 'o', 'm', 'e', ' ', 'n', 'e', 'x', 't', '.'
};

OSErr platinum_application_init(platinum_application *app)
{
    Rect bounds;
    OSErr err;

    if (app == NULL)
        return paramErr;

    memset(app, 0, sizeof(*app));
    platinum_session_init(&app->session);

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    FlushEvents(everyEvent, 0);

    err = platinum_session_load(&app->session);
    if (err != noErr)
        return err;

    SetRect(&bounds, 72, 56, 600, 416);

    app->window = NewCWindow(NULL, bounds, kWindowTitle, true,
                             documentProc, (WindowPtr)-1L, true, 0L);
    if (app->window == NULL) {
        platinum_session_close(&app->session);
        return memFullErr;
    }

    app->running = 1;
    wf_macos9_set_yield_callback(platinum_application_yield, app);
    InvalRect(&app->window->portRect);
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

    platinum_session_close(&app->session);
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
    unsigned char key;

    if (app == NULL || event == NULL)
        return;

    switch (event->what) {
        case mouseDown:
            part = FindWindow(event->where, &window);
            if (part == inGoAway && window == app->window) {
                app->running = 0;
            } else if (part == inDrag && window == app->window) {
                DragWindow(window, event->where, NULL);
                InvalRect(&window->portRect);
            }
            break;

        case updateEvt:
            window = (WindowPtr)event->message;
            if (window == app->window) {
                BeginUpdate(window);
                platinum_application_draw(app);
                EndUpdate(window);
            }
            break;

        case activateEvt:
            window = (WindowPtr)event->message;
            if (window == app->window)
                HiliteWindow(window, (event->modifiers & activeFlag) != 0);
            break;

        case keyDown:
        case autoKey:
            key = (unsigned char)(event->message & charCodeMask);
            if ((event->modifiers & cmdKey) != 0 &&
                (key == 'q' || key == 'Q')) {
                app->running = 0;
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

    EraseRect(&app->window->portRect);

    MoveTo(20, 28);
    DrawString(kTitle);

    MoveTo(20, 56);
    DrawString(kBridgeReady);

    MoveTo(20, 84);
    if (platinum_session_is_paired(&app->session))
        DrawString(kPaired);
    else
        DrawString(kNotPaired);

    MoveTo(20, 120);
    DrawString(kNextStep);

    SetPort(old_port);
}
