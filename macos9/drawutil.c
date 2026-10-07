#include <string.h>

#include "drawutil.h"

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
