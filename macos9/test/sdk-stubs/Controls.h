/* sdk-stubs/Controls.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_CONTROLS_H
#define PLATINUM_STUB_CONTROLS_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>

typedef struct StubControl {
    Rect bounds;
    short value;
    unsigned short refCon;
} ControlRecord;

typedef ControlRecord *ControlPtr;
typedef ControlPtr ControlHandle;

typedef void (*ControlActionProc)(ControlHandle control, short part);
typedef ProcPtr *ControlActionUPP;

/* Standard control procedures live in the real Controls.h. */
void scrollBarProc(ControlHandle control, short part);

ControlHandle NewControl(WindowPtr inWindow, const Rect *boundsRect,
                         Str255 title, Boolean isVisible, short procID,
                         short itemID, short value,
                         ControlActionProc actionProc, long refCon);
void DisposeControl(ControlHandle control);

void SetControlTitle(ControlHandle control, Str255 title);
void GetControlTitle(ControlHandle control, Str255 title);
void SetControlBounds(ControlHandle control, const Rect *bounds);
void GetControlBounds(ControlHandle control, Rect *bounds);
unsigned short GetControlBits(ControlHandle control);

void EnableControl(ControlHandle control);
void DisableControl(ControlHandle control);
void ShowControl(ControlHandle control);
void HideControl(ControlHandle control);

void SetCtlMin(ControlHandle control, short minValue);
void SetCtlMax(ControlHandle control, short maxValue);
void SetCtlValue(ControlHandle control, short value);
short GetCtlValue(ControlHandle control);
short GetCtlMin(ControlHandle control);
short GetCtlMax(ControlHandle control);

short TrackControl(ControlHandle control, Point start,
                   ControlActionUPP actionProc);
void Draw1Control(ControlHandle control, short part);
void DrawControl(ControlHandle control);

#endif
