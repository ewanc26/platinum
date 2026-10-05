/* sdk-stubs/TextEdit.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_TEXTEDIT_H
#define PLATINUM_STUB_TEXTEDIT_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>

typedef struct StubTextEditRecord {
    Rect viewRect;
    long selStart;
    long selLength;
} TextEditRecord;

typedef TextEditRecord *TEHandle;
typedef TEHandle *TEPtr;

/* The Classic Mac OS signature. Nine parameters, in this order. */
TEHandle TENew(const Rect *boundsRect, const Rect *viewRect, Boolean grow,
               short textMaxLength, short textStorageSize, long fontID,
               WindowPtr window, TEHandle dest, Ptr callBack);
void TEDispose(TEHandle te);

void TEKey(KeyMap keyMap, TEHandle te);
void TEActivate(TEHandle te);
void TEDeactivate(TEHandle te);
void TEClick(Point where, Boolean extendSelection,
             short count, short wordCount, TEHandle te);
void TEAutoView(TEHandle te, short maxSize);
void TESetSelection(TEHandle te, long selStart, long selLength);
void TEGetSelection(TEHandle te, long *selStart, long *selLength);
void TESetCaret(TEHandle te, Point caretPos);
void TEInsert(const Ptr text, long insertLength, TEHandle te);
void TECopy(TEHandle te, StringPtr dest);
void TEPaste(TEHandle te, Handle textHandle);
void TEUpdate(const Rect *updateRect, TEHandle te);
void TEForceRedraw(TEHandle te, Boolean redrawLater);
Handle TEGetText(TEHandle te);
void TESetText(TEHandle te, Handle textHandle);

long TextSize(short size);
void TextAlign(short alignment);
long TextFont(long fontID);

#endif
