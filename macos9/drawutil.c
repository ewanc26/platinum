#include <string.h>
#include <Windows.h>

#include "drawutil.h"

OSErr platinum_window_open(WindowPtr *window, const Rect *bounds, ConstStr255Param title)
{
    if (window == NULL || bounds == NULL || title == NULL)
        return paramErr;

    *window = NewCWindow(NULL, bounds, title, 1, documentProc, (WindowPtr)-1L, 1, 0L);
    return *window != NULL ? noErr : memFullErr;
}

void platinum_draw_button(const Rect *bounds, StringPtr title)
{
    long width;
    short baseline;

    FrameRect(bounds);
    width = StringWidth(title);
    baseline = bounds->top + 14;
    MoveTo(bounds->left + (short)((bounds->right - bounds->left - width) / 2),
           baseline);
    DrawString(title);
}

void platinum_draw_text(const char *text, short x, short y)
{
    if (text == NULL)
        return;

    MoveTo(x, y);
    DrawText((Ptr)text, 0, (short)strlen(text));
}
