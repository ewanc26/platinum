#ifndef PLATINUM_DIAGWIN_H
#define PLATINUM_DIAGWIN_H

#include "diag.h"
#include "state_panel.h"

#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Longest blocking sentence kept: the screen_state texts fit with room. */
#define PLATINUM_DIAGWIN_STATUS_MAX 127

typedef struct platinum_diagwin {
    WindowPtr window;
    platinum_diag diag;
    char status[PLATINUM_DIAGWIN_STATUS_MAX + 1];
    /* 0 the lines, 1 Close, 2 Check Again, 3 the panel action. */
    short focus;
} platinum_diagwin;

enum {
    PLATINUM_DIAGWIN_NONE = 0,
    PLATINUM_DIAGWIN_CLOSE = 1,
    PLATINUM_DIAGWIN_CHECK = 2,
    PLATINUM_DIAGWIN_SIGN_IN = 3,
    PLATINUM_DIAGWIN_PAIR = 4
};

void platinum_diagwin_init(platinum_diagwin *diagwin);
/* The blocking sentence from screen_state.c, or NULL when the window can run. */
void platinum_diagwin_set_status(platinum_diagwin *diagwin,
                                 const char *status);
OSErr platinum_diagwin_open(platinum_diagwin *diagwin);
void platinum_diagwin_close(platinum_diagwin *diagwin);
int platinum_diagwin_handle_event(platinum_diagwin *diagwin,
                                  EventRecord *event);
void platinum_diagwin_draw(platinum_diagwin *diagwin);

#ifdef __cplusplus
}
#endif

#endif
