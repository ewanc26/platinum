#ifndef PLATINUM_APPLICATION_H
#define PLATINUM_APPLICATION_H

#include "session.h"
#include "compose.h"
#include "ui.h"
#include "timeline.h"
#include "profile.h"
#include "notifications.h"
#include "thread.h"
#include "people.h"
#include "search.h"
#include "preferences.h"
#include "pairing.h"
#include "scrollbar.h"
#include <Menus.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_application {
    platinum_session session;
    WindowPtr window;
    platinum_ui_layout layout;
    platinum_ui_state ui;
    platinum_compose compose;
    platinum_timeline timeline;
    platinum_profile profile;
    platinum_notifications notifications;
    platinum_thread thread;
    platinum_people people;
    platinum_search search;
    platinum_preferences preferences;
    platinum_pairing pairing;
    platinum_scrollbar timeline_scrollbar;
    MenuHandle file_menu;
    MenuHandle edit_menu;
    MenuHandle view_menu;
    MenuHandle post_menu;
    MenuHandle window_menu;
    MenuHandle help_menu;
    int running;
} platinum_application;

OSErr platinum_application_init(platinum_application *app);
void platinum_application_run(platinum_application *app);
void platinum_application_dispose(platinum_application *app);

#ifdef __cplusplus
}
#endif

#endif
