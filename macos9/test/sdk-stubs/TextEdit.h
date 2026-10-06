/* sdk-stubs/TextEdit.h -- see MacTypes.h. Signatures follow TextEdit as
 * described by the Multiversal Interfaces (signatures.tsv). The record holds only
 * the fields the sources read, with their real names; its layout is not real. */
#ifndef PLATINUM_STUB_TEXTEDIT_H
#define PLATINUM_STUB_TEXTEDIT_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>

typedef struct StubTextEditRecord {
    Rect viewRect;
    short selStart;
    short selEnd;
    short teLength;
} TERec;

typedef TERec *TEPtr;
typedef TEPtr *TEHandle;
typedef Handle CharsHandle;

TEHandle TENew(const Rect *destRect, const Rect *viewRect);
void TEDispose(TEHandle hTE);

void TEInit(void);
void TEKey(short key, TEHandle hTE);
void TEClick(Point pt, Boolean fExtend, TEHandle hTE);
void TEActivate(TEHandle hTE);
void TEDeactivate(TEHandle hTE);
void TEAutoView(Boolean fAuto, TEHandle hTE);
void TESetSelect(long selStart, long selEnd, TEHandle hTE);
void TESetText(const void *text, long length, TEHandle hTE);
void TEUpdate(const Rect *rUpdate, TEHandle hTE);
CharsHandle TEGetText(TEHandle hTE);

void TextSize(short size);
void TextFont(short font);

#endif
