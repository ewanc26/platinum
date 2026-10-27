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
#define keyDown 3
#define keyUp 4
#define autoKey 5
#define updateEvt 6
#define activateEvt 8
/* EventRecord.modifiers bit set on an activateEvt that activates (clear: deactivates). */
#define activeFlag 0x0001

#define charCodeMask 0x00FF
#define keyCodeMask 0xFF00
#define cmdKey 0x0100
#define shiftKey 0x0200
#define optionKey 0x0800
#define controlKey 0x1000

#define everyEvent 0xFFFF

#define updateMask 0x0040
#define activMask 0x0100

Boolean GetNextEvent(EventMask mask, EventRecord *event);
Boolean WaitNextEvent(EventMask mask, EventRecord *event, SInt32 tick, ProcPtr idleProc);

#endif
