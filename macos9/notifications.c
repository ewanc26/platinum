#include "notifications.h"
#include "drawutil.h"
#include "scrollbar.h"

#include <Quickdraw.h>
#include <string.h>

static unsigned char kNotificationsTitle[] = {
    13, 'N', 'o', 't', 'i', 'f', 'i', 'c', 'a', 't', 'i', 'o', 'n', 's'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};

OSErr platinum_notifications_open(platinum_notifications *notifications)
{
    Rect bounds;

    if (notifications == NULL)
        return paramErr;

    if (notifications->window != NULL) {
        SelectWindow(notifications->window);
        return noErr;
    }

    SetRect(&bounds, 86, 60, 666, 440);
    notifications->window = NewCWindow(NULL, &bounds, kNotificationsTitle, 1,
                                       documentProc, (WindowPtr)-1L, 1, 0L);
    if (notifications->window == NULL)
        return memFullErr;

    {
        Rect scrollbar_bounds;
        scrollbar_bounds = notifications->window->portRect;
        scrollbar_bounds.left = scrollbar_bounds.right - 15;
        scrollbar_bounds.top = 28;
        scrollbar_bounds.bottom -= 40;

        if (platinum_scrollbar_open(&notifications->scrollbar,
                                    notifications->window,
                                    &scrollbar_bounds) != noErr) {
            DisposeWindow(notifications->window);
            notifications->window = NULL;
            return memFullErr;
        }
    }

    SetPort((GrafPtr)notifications->window);
    platinum_notifications_draw(notifications);
    SelectWindow(notifications->window);
    return noErr;
}

void platinum_notifications_close(platinum_notifications *notifications)
{
    if (notifications == NULL)
        return;

    platinum_scrollbar_close(&notifications->scrollbar);

    if (notifications->window != NULL) {
        DisposeWindow(notifications->window);
        notifications->window = NULL;
    }
}

static void notifications_draw_row(platinum_notifications *notifications,
                                   short index,
                                   short row)
{
    platinum_notification *item;
    short top;

    item = &notifications->items[index];
    top = 36 + (row * 46);

    MoveTo(12, top);
    platinum_draw_text(item->author, 12, top);

    MoveTo(120, top);
    platinum_draw_text(item->handle, 120, top);

    MoveTo(230, top);
    platinum_draw_text(item->reason, 230, top);

    MoveTo(450, top);
    platinum_draw_text(item->time, 450, top);

    if (!item->is_read) {
        MoveTo(12, top + 16);
        platinum_draw_text("New", 12, top + 16);
    }
}

void platinum_notifications_draw(platinum_notifications *notifications)
{
    GrafPtr old_port;
    Rect close_rect;
    short index;
    short row;
    short visible;

    if (notifications == NULL || notifications->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)notifications->window);
    EraseRect(&notifications->window->portRect);

    platinum_draw_text("Author", 12, 24);
    platinum_draw_text("Handle", 120, 24);
    platinum_draw_text("Reason", 230, 24);
    platinum_draw_text("Time", 450, 24);

    if (notifications->status[0] != '\0')
        platinum_draw_text(notifications->status, 12, 54);

    visible = (notifications->window->portRect.bottom - 78) / 46;
    if (visible < 1)
        visible = 1;

    for (row = 0; row < visible; ++row) {
        index = notifications->scroll_row + row;
        if (index >= (short)notifications->count)
            break;
        notifications_draw_row(notifications, index, row + 1);
    }

    close_rect = notifications->window->portRect;
    close_rect.left = close_rect.right - 78;
    close_rect.right -= 10;
    close_rect.top = close_rect.bottom - 34;
    close_rect.bottom -= 10;
    platinum_draw_button(&close_rect, kClose);

    platinum_scrollbar_set_range(
        &notifications->scrollbar,
        (short)notifications->count,
        visible,
        notifications->scroll_row);
    platinum_scrollbar_draw(&notifications->scrollbar);

    SetPort(old_port);
}

int platinum_notifications_handle_event(
    platinum_notifications *notifications,
    EventRecord *event)
{
    Point where;
    Rect close_rect;
    short visible;

    if (notifications == NULL || event == NULL ||
        notifications->window == NULL)
        return PLATINUM_NOTIFICATIONS_NONE;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == notifications->window) {
                BeginUpdate(notifications->window);
                platinum_notifications_draw(notifications);
                EndUpdate(notifications->window);
            }
            return PLATINUM_NOTIFICATIONS_NONE;

        case activateEvt:
            if ((WindowPtr)(long)event->message == notifications->window)
                HiliteWindow(notifications->window,
                             (event->modifiers & activeFlag) != 0);
            return PLATINUM_NOTIFICATIONS_NONE;

        case mouseDown:
            if (platinum_scrollbar_handle_mouse(
                    &notifications->scrollbar,
                    event,
                    &notifications->scroll_row)) {
                InvalRect(&notifications->window->portRect);
                return PLATINUM_NOTIFICATIONS_NONE;
            }

            where = event->where;
            GlobalToLocal(&where);

            close_rect = notifications->window->portRect;
            close_rect.left = close_rect.right - 78;
            close_rect.right -= 10;
            close_rect.top = close_rect.bottom - 34;
            close_rect.bottom -= 10;

            if (PtInRect(where, &close_rect))
                return PLATINUM_NOTIFICATIONS_CLOSE;
            return PLATINUM_NOTIFICATIONS_NONE;

        case keyDown:
        case autoKey:
            visible = (notifications->window->portRect.bottom - 78) / 46;
            if (visible < 1)
                visible = 1;

            if ((event->modifiers & cmdKey) != 0 &&
                ((event->message & charCodeMask) == 'w' ||
                 (event->message & charCodeMask) == 'W'))
                return PLATINUM_NOTIFICATIONS_CLOSE;

            switch (event->message & charCodeMask) {
                case upArrow:
                    if (notifications->scroll_row > 0)
                        --notifications->scroll_row;
                    break;
                case downArrow:
                    /* Down at the bottom of the list asks for the next page,
                     * by keystroke rather than as a side effect of scrolling. */
                    if (notifications->scroll_row + visible >=
                            (short)notifications->count &&
                        platinum_notifications_has_older(notifications))
                        return PLATINUM_NOTIFICATIONS_LOAD_OLDER;
                    if (notifications->scroll_row + visible <
                        (short)notifications->count)
                        ++notifications->scroll_row;
                    break;
                case pageUp:
                    notifications->scroll_row -= visible;
                    if (notifications->scroll_row < 0)
                        notifications->scroll_row = 0;
                    break;
                case pageDown:
                    notifications->scroll_row += visible;
                    if (notifications->scroll_row + visible >
                        (short)notifications->count)
                        notifications->scroll_row =
                            (short)notifications->count - visible;
                    if (notifications->scroll_row < 0)
                        notifications->scroll_row = 0;
                    break;
                default:
                    break;
            }

            if ((event->modifiers & cmdKey) != 0 &&
                ((event->message & charCodeMask) == 'r' ||
                 (event->message & charCodeMask) == 'R'))
                return PLATINUM_NOTIFICATIONS_REFRESH;

            InvalRect(&notifications->window->portRect);
            return PLATINUM_NOTIFICATIONS_NONE;

        default:
            break;
    }

    return PLATINUM_NOTIFICATIONS_NONE;
}
