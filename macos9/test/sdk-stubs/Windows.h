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

/* The Classic Mac OS signature. Seven parameters, in this order. */
/* A window procedure. The real SDK declares documentProc with this same
 * function-pointer return type, which is why it can be passed to NewCWindow
 * by name. */
typedef ProcPtr (*WindowProc)(WindowPtr window, int message,
                               ParamStructRec *param, Ptr lParam);

WindowPtr NewCWindow(const Rect *boundsRect, Boolean isVisible, short procID,
                     WindowProc handlerProc, WindowPtr behind,
                     Boolean inGoToState, long refCon);

WindowPtr NewWindow(short dialogID, const Rect *boundsRect, Boolean isVisible,
                    short procID, long behind, WindowPtr inFront,
                    WindowPtr goWindowTo);
void DisposeWindow(WindowPtr window);
void InvalRect(const Rect *r);

void ShowWindow(WindowPtr window);
void HideWindow(WindowPtr window);
void SelectWindow(WindowPtr window);
void EnableWindow(WindowPtr window);
void DisableWindow(WindowPtr window);
void BringWindowToFront(WindowPtr window);
WindowPtr FrontWindow(void);
void GetWindowRect(WindowPtr window, Rect *rect);
void SetWindowTitle(WindowPtr window, Str255 title);
void GetWindowTitle(WindowPtr window, Str255 title);
void SetWindowTitle(WindowPtr window, Str255 title);
void SetWindowRefCon(WindowPtr window, long refCon);
long GetWindowRefCon(WindowPtr window);
void HiliteWindow(WindowPtr window);
#define activeFlag 0

void BeginUpdate(WindowPtr window);
void EndUpdate(WindowPtr window);

Ptr NewPtr(long byteCount);
Ptr NewHandle(long byteCount);
void DisposePtr(Ptr p);
void DisposeHandle(Handle h);
void HLock(Handle h);
void HUnlock(Handle h);
unsigned char HGetState(Handle h);
void HSetState(Handle h, unsigned char state);
void HSetSize(Handle h, SInt32 byteCount);
SInt32 GetHandleSize(Handle h);
Ptr SetPtr(Handle h, long byteCount);

ProcPtr documentProc(WindowPtr window, int message, ParamStructRec *param,
                     Ptr lParam);

#endif
