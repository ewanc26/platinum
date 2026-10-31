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

static Rect notifications_close_rect(const platinum_notifications *notifications)
{
    Rect rect;

    rect = notifications->window->portRect;
    rect.left = rect.right - 78;
    rect.right -= 10;
    rect.top = rect.bottom - 34;
    rect.bottom -= 10;
    return rect;
}

static Rect notifications_content_rect(
    const platinum_notifications *notifications)
{
    Rect rect;

    rect = notifications->window->portRect;
    rect.left += 8;
    rect.right -= 8;
    rect.top += 8;
    rect.bottom -= 44;
    return rect;
}

static void notifications_panel(const platinum_notifications *notifications,
                                platinum_panel *panel)
{
    (void)platinum_panel_for_screen(panel, notifications->status,
                                    notifications->loading,
                                    notifications->count != 0);
}

static void notifications_draw_focus(const Rect *rect)
{
    Rect inner;

    inner = *rect;
    InsetRect(&inner, 2, 2);
    FrameRect(&inner);
}

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
    if (platinum_window_open(&notifications->window, &bounds, kNotificationsTitle) != noErr)
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
    platinum_panel panel;
    short index;
    short row;
    short visible;

    if (notifications == NULL || notifications->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)notifications->window);
    EraseRect(&notifications->window->portRect);

    notifications_panel(notifications, &panel);
    close_rect = notifications_close_rect(notifications);
    if (platinum_panel_is_active(&panel)) {
        Rect content;

        content = notifications_content_rect(notifications);
        platinum_panel_draw(&content, &panel);
        if (notifications->focus == 2) {
            platinum_panel_rects rects;
            char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX];
            short count;

            count = platinum_panel_wrap(panel.message,
                                        platinum_panel_columns(&content),
                                        lines, PLATINUM_PANEL_LINES);
            platinum_panel_rects_compute(
                &content, count,
                panel.action != PLATINUM_PANEL_ACTION_NONE, &rects);
            if (rects.button_visible)
                notifications_draw_focus(&rects.button);
        }
        platinum_draw_button(&close_rect, kClose);
        if (notifications->focus == 1)
            notifications_draw_focus(&close_rect);
        SetPort(old_port);
        return;
    }

    platinum_draw_text("Author", 12, 24);
    platinum_draw_text("Handle", 120, 24);
    platinum_draw_text("Reason", 230, 24);
    platinum_draw_text("Time", 450, 24);

    visible = (notifications->window->portRect.bottom - 78) / 46;
    if (visible < 1)
        visible = 1;

    for (row = 0; row < visible; ++row) {
        index = notifications->scroll_row + row;
        if (index >= (short)notifications->count)
            break;
        notifications_draw_row(notifications, index, row + 1);
    }

    if (notifications->status[0] != '\0')
        platinum_draw_text(notifications->status, 12,
                           notifications->window->portRect.bottom - 48);
    if (notifications->capped)
        platinum_draw_text("Showing the newest 40 notifications.", 12,
                           notifications->window->portRect.bottom - 64);

    platinum_draw_button(&close_rect, kClose);
    if (notifications->focus == 1)
        notifications_draw_focus(&close_rect);

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
            where = event->where;
            GlobalToLocal(&where);

            {
                platinum_panel panel;
                Rect content;
                int panel_action;

                notifications_panel(notifications, &panel);
                content = notifications_content_rect(notifications);
                panel_action = platinum_panel_action_at(&content, &panel, where);
                if (panel_action == PLATINUM_PANEL_ACTION_SIGN_IN)
                    return PLATINUM_NOTIFICATIONS_SIGN_IN;
                if (panel_action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                    return PLATINUM_NOTIFICATIONS_PAIR;
                if (platinum_panel_is_active(&panel)) {
                    close_rect = notifications_close_rect(notifications);
                    if (PtInRect(where, &close_rect))
                        return PLATINUM_NOTIFICATIONS_CLOSE;
                    return PLATINUM_NOTIFICATIONS_NONE;
                }
            }

            if (platinum_scrollbar_handle_mouse(
                    &notifications->scrollbar,
                    event,
                    &notifications->scroll_row)) {
                InvalRect(&notifications->window->portRect);
                return PLATINUM_NOTIFICATIONS_NONE;
            }

            close_rect = notifications_close_rect(notifications);
            if (PtInRect(where, &close_rect))
                return PLATINUM_NOTIFICATIONS_CLOSE;
            return PLATINUM_NOTIFICATIONS_NONE;

        case keyDown:
        case autoKey:
            visible = (notifications->window->portRect.bottom - 78) / 46;
            if (visible < 1)
                visible = 1;

            if (platinum_panel_key_is_cancel(event))
                return PLATINUM_NOTIFICATIONS_CLOSE;

            {
                platinum_panel panel;
                unsigned char key;

                notifications_panel(notifications, &panel);
                key = (unsigned char)(event->message & charCodeMask);
                if (key == '\t') {
                    short last = platinum_panel_is_active(&panel) ? 2 : 1;
                    if ((event->modifiers & shiftKey) != 0)
                        notifications->focus =
                            notifications->focus <= 0
                                ? last
                                : (short)(notifications->focus - 1);
                    else
                        notifications->focus =
                            notifications->focus >= last
                                ? 0
                                : (short)(notifications->focus + 1);
                    InvalRect(&notifications->window->portRect);
                    return PLATINUM_NOTIFICATIONS_NONE;
                }
                if (platinum_panel_key_is_return(event)) {
                    if (panel.action == PLATINUM_PANEL_ACTION_SIGN_IN)
                        return PLATINUM_NOTIFICATIONS_SIGN_IN;
                    if (panel.action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                        return PLATINUM_NOTIFICATIONS_PAIR;
                    if (notifications->focus == 1)
                        return PLATINUM_NOTIFICATIONS_CLOSE;
                    return PLATINUM_NOTIFICATIONS_REFRESH;
                }
            }

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

void platinum_notifications_set_status(
    platinum_notifications *notifications, const char *status)
{
    long length;

    if (notifications == NULL)
        return;

    notifications->status[0] = '\0';
    if (status != NULL) {
        length = (long)strlen(status);
        if (length > PLATINUM_NOTIFICATIONS_STATUS_MAX)
            length = PLATINUM_NOTIFICATIONS_STATUS_MAX;
        memcpy(notifications->status, status, (size_t)length);
        notifications->status[length] = '\0';
    }
    if (notifications->window != NULL)
        InvalRect(&notifications->window->portRect);
}
