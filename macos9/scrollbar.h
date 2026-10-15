#ifndef PLATINUM_SCROLLBAR_H
#define PLATINUM_SCROLLBAR_H

#include <Controls.h>
#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Keyboard navigation, in every list window.
 *
 * The standard KCHR resource maps the arrow and paging keys to these
 * character codes in the message of keyDown and autoKey events:
 *
 *   upArrow 0x1E, downArrow 0x1F, leftArrow 0x1C, rightArrow 0x1D,
 *   pageUp 0x0B (VT), pageDown 0x0C (FF).
 *
 * Apple's headers do not name them: the Multiversal Interfaces (Retro68)
 * have no equivalent, and the Carbon headers only name them as
 * kUpArrowCharCode and friends. Platinum defines them here, and the host
 * SDK stubs do not redefine them. A stub repeating these names with other
 * values would make the host check green without the code being real for
 * the Mac.
 */
#define upArrow 0x1E
#define downArrow 0x1F
#define leftArrow 0x1C
#define rightArrow 0x1D
#define pageUp 0x0B
#define pageDown 0x0C

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
int platinum_scrollbar_handle_mouse(platinum_scrollbar *scrollbar,
                                     EventRecord *event,
                                     short *value);
void platinum_scrollbar_draw(platinum_scrollbar *scrollbar);

#ifdef __cplusplus
}
#endif

#endif
