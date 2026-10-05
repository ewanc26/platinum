/* sdk-stubs/Windows.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_WINDOWS_H
#define PLATINUM_STUB_WINDOWS_H

#include <MacTypes.h>
#include <Quickdraw.h>

typedef struct StubWindow {
    Rect portRect;
    Rect strucRect;
    short wRefCon;
} WindowRecord;

typedef WindowPtr WindowRef;

/* The Classic Mac OS signature: eight parameters, in this order. procID is a
 * window definition ID, not a function pointer; documentProc is that constant. */
#define documentProc 0

WindowPtr NewCWindow(void *wStorage, const Rect *boundsRect,
                     const unsigned char *title, Boolean visible,
                     short procID, WindowPtr behind, Boolean goAwayFlag,
                     long refCon);

void DisposeWindow(WindowPtr window);
void InvalRect(const Rect *r);
void SelectWindow(WindowPtr window);
WindowPtr FrontWindow(void);
void InitWindows(void);
void HiliteWindow(WindowPtr window, Boolean fHilite);
short DragWindow(WindowPtr window, Point startPoint, const Rect *dragRect);

void BeginUpdate(WindowPtr window);
void EndUpdate(WindowPtr window);

void HLock(Handle h);
unsigned char HGetState(Handle h);
void HSetState(Handle h, unsigned char state);
SInt32 GetHandleSize(Handle h);

#endif
