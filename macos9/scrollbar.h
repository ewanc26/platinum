#ifndef PLATINUM_SCROLLBAR_H
#define PLATINUM_SCROLLBAR_H

#include <Controls.h>
#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_scrollbar {
    ControlHandle control;
    WindowPtr window;
    Rect bounds;
    short maximum;
} platinum_scrollbar;

OSErr platinum_scrollbar_open(platinum_scrollbar *scrollbar,
                              WindowPtr window,
                              const Rect *bounds);
void platinum_scrollbar_close(platinum_scrollbar *scrollbar);
void platinum_scrollbar_set_range(platinum_scrollbar *scrollbar,
                                  short total,
                                  short visible,
                                  short value);
short platinum_scrollbar_value(
    const platinum_scrollbar *scrollbar);
int platinum_scrollbar_handle_mouse(platinum_scrollbar *scrollbar,
                                     EventRecord *event,
                                     short *value);
void platinum_scrollbar_draw(platinum_scrollbar *scrollbar);

#ifdef __cplusplus
}
#endif

#endif
