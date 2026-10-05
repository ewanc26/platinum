#ifndef PLATINUM_APPLICATION_H
#define PLATINUM_APPLICATION_H

#include "session.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_application {
    platinum_session session;
    WindowPtr window;
    platinum_ui_layout layout;
    int running;
} platinum_application;

OSErr platinum_application_init(platinum_application *app);
void platinum_application_run(platinum_application *app);
void platinum_application_dispose(platinum_application *app);

#ifdef __cplusplus
}
#endif

#endif
