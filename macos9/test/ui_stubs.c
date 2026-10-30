/*
 * ui_stubs.c -- the Toolbox calls ui.c makes, implemented for host tests.
 *
 * macos9/test/sdk-stubs/ is declaration-only on purpose: it exists so every
 * source compiles as C89, not so a host binary can pretend to be a Mac. This
 * file is the narrow exception the UI logic tests need, and it is deliberately
 * separate from those headers:
 *
 *   - geometry (SetRect, PtInRect, InsetRect, OffsetRect, EqualRect) is real,
 *     because hit-testing is what the tests check;
 *   - drawing is a no-op, because a host has no QuickDraw and the tests assert
 *     on state, not pixels;
 *   - StringWidth returns a fixed 7 points per character so button centring is
 *     deterministic;
 *   - platinum_session_is_paired is a double: the tests never call
 *     platinum_ui_draw, and linking the real session would drag the whole
 *     preferences, secret and bridge stack into a pure layout test.
 *
 * Nothing here is linked into Platinum, and no real SDK declaration was changed
 * to accommodate it.
 */
#include <Events.h>
#include <Fonts.h>
#include <Quickdraw.h>
#include <Windows.h>

#include "session.h"

GrafPort qd;

void SetRect(Rect *r, short left, short top, short right, short bottom)
{
    r->left = left;
    r->top = top;
    r->right = right;
    r->bottom = bottom;
}

void InsetRect(Rect *r, short dx, short dy)
{
    r->left += dx;
    r->top += dy;
    r->right -= dx;
    r->bottom -= dy;
}

void OffsetRect(Rect *r, short dx, short dy)
{
    r->left += dx;
    r->right += dx;
    r->top += dy;
    r->bottom += dy;
}

Boolean EqualRect(const Rect *a, const Rect *b)
{
    return a->left == b->left && a->top == b->top && a->right == b->right &&
           a->bottom == b->bottom;
}

Boolean PtInRect(Point pt, const Rect *r)
{
    return pt.h >= r->left && pt.h < r->right && pt.v >= r->top &&
           pt.v < r->bottom;
}

void FrameRect(const Rect *r)
{
    (void)r;
}

void EraseRect(const Rect *r)
{
    (void)r;
}

void PaintRect(const Rect *r)
{
    (void)r;
}

void InvalRect(const Rect *r)
{
    (void)r;
}

void MoveTo(short x, short y)
{
    (void)x;
    (void)y;
}

void LineTo(short x, short y)
{
    (void)x;
    (void)y;
}

void DrawText(Ptr text, short length, short y)
{
    (void)text;
    (void)length;
    (void)y;
}

void DrawChar(char c)
{
    (void)c;
}

void DrawString(StringPtr s)
{
    (void)s;
}

short StringWidth(StringPtr s)
{
    return (short)(s == NULL ? 0 : s[0] * 7);
}

short CharWidth(short c)
{
    (void)c;
    return 7;
}

void TextFont(short font)
{
    (void)font;
}

void TextSize(short size)
{
    (void)size;
}

void SetPort(GrafPtr port)
{
    (void)port;
}

void GetPort(GrafPtr *port)
{
    if (port != NULL)
        *port = NULL;
}

void GlobalToLocal(Point *pt)
{
    (void)pt;
}

void LocalToGlobal(Point *pt)
{
    (void)pt;
}

void InitGraf(GrafPtr *thePort)
{
    if (thePort != NULL)
        *thePort = NULL;
}

/* The tests exercise layout, mouse and keyboard; the paired state is only read
 * by platinum_ui_draw, which they do not call. */
int platinum_session_is_paired(const platinum_session *session)
{
    (void)session;
    return 1;
}
