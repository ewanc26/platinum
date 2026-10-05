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

/* The Classic Mac OS signature: seven parameters, in this order. The real SDK
 * declares documentProc with this same function-pointer return type, which is
 * why a procedure can be passed to NewCWindow by name. */
typedef ProcPtr (*WindowProc)(WindowPtr window, int message,
                               ParamStructRec *param, Ptr lParam);

WindowPtr NewCWindow(const Rect *boundsRect, Boolean isVisible, short procID,
                     WindowProc handlerProc, WindowPtr behind,
                     Boolean inGoToState, long refCon);

ProcPtr documentProc(WindowPtr window, int message, ParamStructRec *param,
                     Ptr lParam);

void DisposeWindow(WindowPtr window);
void InvalRect(const Rect *r);
void SelectWindow(WindowPtr window);
WindowPtr FrontWindow(void);
void InitWindows(void);
void HiliteWindow(WindowPtr window);
void SetWindowRefCon(WindowPtr window, long refCon);
long GetWindowRefCon(WindowPtr window);
short DragWindow(WindowPtr window, Point startPoint, const Rect *dragRect);
void SetWindowTitle(WindowPtr window, Str255 title);

void BeginUpdate(WindowPtr window);
void EndUpdate(WindowPtr window);

void HLock(Handle h);
unsigned char HGetState(Handle h);
void HSetState(Handle h, unsigned char state);
SInt32 GetHandleSize(Handle h);

#endif
