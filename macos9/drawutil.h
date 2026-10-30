#ifndef PLATINUM_DRAWUTIL_H
#define PLATINUM_DRAWUTIL_H

#include <MacTypes.h>
#include <Quickdraw.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Open a standard document window, visible with a close box and the same
 * colour/behind settings every Platinum window uses. Returns memFullErr when
 * the Window Manager cannot allocate it. */
OSErr platinum_window_open(WindowPtr *window, const Rect *bounds, ConstStr255Param title);

/* A framed button with its title centred horizontally, in the current port. */
void platinum_draw_button(const Rect *bounds, StringPtr title);

/* A C string at (x, y) in the current port; NULL draws nothing. */
void platinum_draw_text(const char *text, short x, short y);

#ifdef __cplusplus
}
#endif

#endif
