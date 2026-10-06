/* sdk-stubs/Events.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_EVENTS_H
#define PLATINUM_STUB_EVENTS_H

#include <MacTypes.h>

typedef unsigned short EventKind;
typedef unsigned short EventMask;

typedef struct StubEventRecord {
    EventKind what;
    SInt16 message;
    unsigned long when;
    Point where;
    unsigned long modifiers;
    unsigned char link;
    unsigned char cks;
    long eventSize;
} EventRecord;

typedef EventRecord *EventPtr;

#define mouseDown 1
#define mouseUp 2
#define mouseDragged 3
#define keyDown 4
#define keyUp 5
#define autoKey 6
#define activateEvt 7
/* EventRecord.modifiers bit set on an activateEvt that activates (clear: deactivates). */
#define activeFlag 0x0001
#define updateEvt 8

#define charEventMask 0x0001
#define keyDownMask 0x0002
#define mouseDownMask 0x0004
#define mouseUpMask 0x0008
#define updateEvtMask 0x0080
#define updateMask 0x0080
#define activMask 0x0400
#define noMouseDown 0x8000
#define pageUp 0x72
#define pageDown 0x74
#define homeKey 0x73
#define endKey 0x77
#define forwardDelete 0x75
#define enterKey 0x24

#define upArrow 0x7B
#define downArrow 0x7D
#define leftArrow 0x7C
#define rightArrow 0x7E
#define tabKey 0x09
#define spaceKey 0x20

#define charCodeMask 0x00FF
#define keyCodeMask 0x00FF
#define cmdKey 0x0100
#define shiftKey 0x0200
#define optionKey 0x0400
#define controlKey 0x0800

#define everyEvent 0xFFFF

Boolean GetNextEvent(EventMask mask, EventRecord *event);
Boolean WaitNextEvent(EventMask mask, EventRecord *event, SInt32 tick, ProcPtr idleProc);

#endif
