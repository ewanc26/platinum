/* sdk-stubs/Controls.h -- see MacTypes.h. Signatures follow the Control Manager
 * as described by the Multiversal Interfaces (signatures.tsv). */
#ifndef PLATINUM_STUB_CONTROLS_H
#define PLATINUM_STUB_CONTROLS_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>
#include <Events.h>

typedef struct StubControl {
    Rect bounds;
    short value;
    unsigned short refCon;
} ControlRecord;

typedef ControlRecord *ControlPtr;
typedef ControlPtr ControlHandle;

typedef ProcPtr ControlActionUPP;

/* A control definition ID (procID), not a function. 16 is the standard scroll bar. */
#define scrollBarProc 16

ControlHandle NewControl(WindowPtr theWindow, const Rect *boundsRect,
                         ConstStr255Param title, Boolean visible,
                         short value, short min, short max, short procID,
                         long refCon);
void DisposeControl(ControlHandle theControl);

void ShowControl(ControlHandle theControl);
void HideControl(ControlHandle theControl);

void SetControlValue(ControlHandle theControl, short newValue);
short GetControlValue(ControlHandle theControl);
void SetControlMinimum(ControlHandle theControl, short newMinimum);
short GetControlMinimum(ControlHandle theControl);
void SetControlMaximum(ControlHandle theControl, short newMaximum);
short GetControlMaximum(ControlHandle theControl);

short TrackControl(ControlHandle theControl, Point startPt,
                   ControlActionUPP actionProc);
void Draw1Control(ControlHandle theControl);

void InitDialogs(ProcPtr resumeProc);
void FlushEvents(EventMask whichMask, EventMask stopMask);
void InitCursor(void);

#endif
