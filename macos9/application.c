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

static void platinum_application_yield(void *userdata);
static void platinum_application_handle_event(platinum_application *app,
                                              EventRecord *event);
static void platinum_application_draw(platinum_application *app)
{
    GrafPtr old_port;

    if (app == NULL || app->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)app->window);

    platinum_ui_draw((GrafPtr)app->window, &app->layout,
                     &app->session);

    SetPort(old_port);
}
