/* sdk-stubs/Quickdraw.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_QUICKDRAW_H
#define PLATINUM_STUB_QUICKDRAW_H

#include <MacTypes.h>

/*
 * The one global QuickDraw owns. Modelled as a struct because the canonical
 * QuickDraw-era init call is InitGraf(&qd.thePort): qd's own port pointer
 * lives in a member called thePort.
 */
typedef struct StubGrafPort {
    Rect portRect;
    GrafPtr thePort;
} GrafPort;

extern GrafPort qd;

#define srcCopy 0


void SetRect(Rect *r, short left, short top, short right, short bottom);
void InsetRect(Rect *r, short dx, short dy);
void OffsetRect(Rect *r, short dx, short dy);
Boolean EqualRect(const Rect *a, const Rect *b);
Boolean PtInRect(Point pt, const Rect *r);

void FrameRect(const Rect *r);
void EraseRect(const Rect *r);
short FindWindow(Point pt, WindowPtr *window);
void FillRect(const Rect *r, Pattern *pat);
void PaintRect(const Rect *r);
void CopyBits(BitMap *srcBits, BitMap *dstBits, const Rect *srcRect,
              const Rect *dstRect, short mode, RgnHandle maskRgn);
long GetCTSeed(void);
void InvalRect(const Rect *r);

void InitGraf(GrafPtr *thePort);
void InitFonts(void);
void SetPort(GrafPtr port);
void GetPort(GrafPtr *port);

void GlobalToLocal(Point *pt);
void LocalToGlobal(Point *pt);

void MoveTo(short x, short y);
void LineTo(short x, short y);
void DrawText(Ptr text, short length, short y);
void DrawChar(char c);
void DrawString(StringPtr s);

short StringWidth(StringPtr s);
short CharWidth(short c);

void PenSize(short width, short height);
void PenPat(Pattern *pat);

RgnHandle NewRgn(void);
void DisposeRgn(RgnHandle rgn);
void EraseRgn(RgnHandle rgn);
void FillRgn(RgnHandle rgn, Pattern *pat);
void FrameRgn(RgnHandle rgn);
void OffsetRgn(RgnHandle rgn, short dx, short dy);

#endif
