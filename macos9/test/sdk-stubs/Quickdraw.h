/* sdk-stubs/Quickdraw.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_QUICKDRAW_H
#define PLATINUM_STUB_QUICKDRAW_H

#include <MacTypes.h>

void SetRect(Rect *r, short top, short left, short bottom, short right);
void InsetRect(Rect *r, short dx, short dy);
void OffsetRect(Rect *r, short dx, short dy);
Boolean EqualRect(const Rect *a, const Rect *b);
Boolean PtInRect(Point pt, const Rect *r);

void FrameRect(const Rect *r);
void EraseRect(const Rect *r);
void FillRect(const Rect *r, Pattern *pat);
void PaintRect(const Rect *r);
void InvalRect(const Rect *r);
void CopyBits(const BitMap *src, const BitMap *dst, Rect *srcRect,
              Rect *dstRect, GrafPtr mode, short copyMode);

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
void GetFontMetrics(FontFamily family, FontStyle style, FontMetrics *metrics);

void PenSize(short width, short height);
void PenPat(Pattern *pat);
void RGBFore(RGBColor *color);
void RGBBack(RGBColor *color);

RgnHandle NewRgn(void);
void DisposeRgn(RgnHandle rgn);
void EraseRgn(RgnHandle rgn);
void FillRgn(RgnHandle rgn, Pattern *pat);
void FrameRgn(RgnHandle rgn);
void OffsetRgn(RgnHandle rgn, short dx, short dy);
void SetRectRgn(RgnHandle rgn, const Rect *rect);

#endif
