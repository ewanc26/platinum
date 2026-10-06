#ifndef PLATINUM_DIAGWIN_H
#define PLATINUM_DIAGWIN_H

#include "diag.h"

#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_diagwin {
    WindowPtr window;
    platinum_diag diag;
} platinum_diagwin;

enum {
    PLATINUM_DIAGWIN_NONE = 0,
    PLATINUM_DIAGWIN_CLOSE = 1,
    PLATINUM_DIAGWIN_CHECK = 2
};

void platinum_diagwin_init(platinum_diagwin *diagwin);
OSErr platinum_diagwin_open(platinum_diagwin *diagwin);
void platinum_diagwin_close(platinum_diagwin *diagwin);
int platinum_diagwin_handle_event(platinum_diagwin *diagwin,
                                  EventRecord *event);
void platinum_diagwin_draw(platinum_diagwin *diagwin);

#ifdef __cplusplus
}
#endif

#endif
