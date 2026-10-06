#include "scrollbar.h"

#include <Quickdraw.h>
#include <string.h>

static unsigned char kEmptyTitle[] = { 0 };

OSErr platinum_scrollbar_open(platinum_scrollbar *scrollbar,
                              WindowPtr window,
                              const Rect *bounds)
{
    if (scrollbar == NULL || window == NULL || bounds == NULL)
        return paramErr;

    memset(scrollbar, 0, sizeof(*scrollbar));
    scrollbar->window = window;
    scrollbar->bounds = *bounds;
    scrollbar->maximum = 0;

    scrollbar->control = NewControl(window,
                                    &scrollbar->bounds,
                                    kEmptyTitle,
                                    1,
                                    0,
                                    0,
                                    0,
                                    scrollBarProc,
                                    0L);
    if (scrollbar->control == NULL) {
        scrollbar->window = NULL;
        return memFullErr;
    }

    return noErr;
}

void platinum_scrollbar_close(platinum_scrollbar *scrollbar)
{
    if (scrollbar == NULL)
        return;

    if (scrollbar->control != NULL) {
        DisposeControl(scrollbar->control);
        scrollbar->control = NULL;
    }

    scrollbar->window = NULL;
    scrollbar->maximum = 0;
}

void platinum_scrollbar_set_range(platinum_scrollbar *scrollbar,
                                  short total,
                                  short visible,
                                  short value)
{
    short maximum;

    if (scrollbar == NULL || scrollbar->control == NULL)
        return;

    if (total < 0)
        total = 0;
    if (visible < 1)
        visible = 1;

    maximum = total - visible;
    if (maximum < 0)
        maximum = 0;

    scrollbar->maximum = maximum;

    if (value < 0)
        value = 0;
    if (value > maximum)
        value = maximum;

    SetControlMinimum(scrollbar->control, 0);
    SetControlMaximum(scrollbar->control, maximum);
    SetControlValue(scrollbar->control, value);

    if (maximum == 0)
        HideControl(scrollbar->control);
    else
        ShowControl(scrollbar->control);
}

short platinum_scrollbar_value(
    const platinum_scrollbar *scrollbar)
{
    if (scrollbar == NULL || scrollbar->control == NULL)
        return 0;

    return GetControlValue(scrollbar->control);
}

int platinum_scrollbar_handle_mouse(platinum_scrollbar *scrollbar,
                                     EventRecord *event,
                                     short *value)
{
    GrafPtr old_port;
    Point where;
    short result;

    if (scrollbar == NULL || scrollbar->control == NULL ||
        event == NULL || value == NULL || event->what != mouseDown ||
        scrollbar->maximum == 0)
        return 0;

    GetPort(&old_port);
    SetPort((GrafPtr)scrollbar->window);

    where = event->where;
    GlobalToLocal(&where);

    if (!PtInRect(where, &scrollbar->bounds)) {
        SetPort(old_port);
        return 0;
    }

    result = TrackControl(scrollbar->control,
                          where,
                          (ControlActionUPP)-1L);
    if (result != 0)
        *value = GetControlValue(scrollbar->control);

    SetPort(old_port);
    return result != 0;
}

void platinum_scrollbar_draw(platinum_scrollbar *scrollbar)
{
    if (scrollbar == NULL || scrollbar->control == NULL)
        return;

    Draw1Control(scrollbar->control);
}
