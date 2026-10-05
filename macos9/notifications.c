#include "notifications.h"
#include "scrollbar.h"
#include "text_codec.h"

#include <Quickdraw.h>
#include "json_min.h"
#include <string.h>

static unsigned char kNotificationsTitle[] = {
    13, 'N', 'o', 't', 'i', 'f', 'i', 'c', 'a', 't', 'i', 'o', 'n', 's'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};
static const char kLoading[] = "Loading notifications...";
static const char kEmpty[] = "No notifications.";
static const char kRefreshHint[] = "Command-R to refresh.";

/*
 * Copy a string member of `object` into `destination` as MacRoman.
 *
 * A member that is absent or is not a string leaves the destination empty: the
 * bridge is trusted to send the documented shape, so a field of the wrong type
 * is a response this client does not understand. Values that are simply long
 * are clipped rather than dropped, so a verbose author keeps their name.
 */
static void notification_copy(char *destination,
                              long capacity,
                              platinum_json object,
                              const char *name)
{
    char utf8[PLATINUM_TEXT_UTF8_CAPACITY];
    long length;

    if (destination == NULL || capacity <= 0)
        return;

    destination[0] = '\0';
    if (platinum_json_string_truncating(object, name, utf8, sizeof(utf8))
        != WF_OK)
        return;

    length = platinum_text_utf8_to_macroman(utf8, destination, capacity, NULL);
    if (length < 0)
        destination[0] = '\0';
}

static void notification_time(char *destination,
                              long capacity,
                              platinum_json object,
                              const char *name)
{
    char raw[64];

    if (destination == NULL || capacity <= 0)
        return;

    destination[0] = '\0';
    if (platinum_json_string_truncating(object, name, raw, sizeof(raw))
        != WF_OK)
        return;

    /* RFC 3339 puts the clock time at a fixed offset after the date's "T".
     * Anything shorter is not that shape, so it is shown as it arrived rather
     * than sliced out of the middle. */
    if (strlen(raw) >= 16 && capacity >= 6) {
        memcpy(destination, raw + 11, 5);
        destination[5] = '\0';
    } else {
        long length = platinum_text_utf8_to_macroman(raw,
                                                     destination,
                                                     capacity,
                                                     NULL);
        if (length < 0)
            destination[0] = '\0';
    }
}

static void notifications_status(platinum_notifications *notifications,
                                 const char *status)
{
    long length;

    if (notifications == NULL)
        return;

    notifications->status[0] = '\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_NOTIFICATIONS_STATUS_MAX)
        length = PLATINUM_NOTIFICATIONS_STATUS_MAX;

    memcpy(notifications->status, status, (size_t)length);
    notifications->status[length] = '\0';

    if (notifications->window != NULL)
        InvalRect(&notifications->window->portRect);
}

static int notification_read_flag(platinum_json value, const char *name)
{
    int flag = 0;

    /* Only true and false are booleans. A string reading "true" is not one, and
     * treating it as read would mark a notification the user has not seen. */
    if (platinum_json_bool(value, name, &flag) != WF_OK)
        return 0;

    return flag;
}

static int notifications_parse_item(platinum_notification *item,
                                     platinum_json value)
{
    platinum_json author;
    char did[128];

    if (item == NULL)
        return 0;

    memset(item, 0, sizeof(*item));

    notification_copy(item->uri, sizeof(item->uri), value, "uri");
    notification_copy(item->cid, sizeof(item->cid), value, "cid");
    if (item->uri[0] == '\0' || item->cid[0] == '\0')
        return 0;

    if (platinum_json_member(value, "author", &author) != WF_OK)
        return 0;

    notification_copy(did, sizeof(did), author, "did");
    if (did[0] == '\0')
        return 0;

    /* Display name, then handle, then DID, so an account with no display name
     * still reads as something. */
    notification_copy(item->author, sizeof(item->author), author,
                      "displayName");
    if (item->author[0] == '\0')
        notification_copy(item->author, sizeof(item->author), author,
                          "handle");
    if (item->author[0] == '\0')
        notification_copy(item->author, sizeof(item->author), author, "did");

    /* An @handle with no handle behind it would read as a bare "@". */
    item->handle[0] = '\0';
    notification_copy(item->handle + 1, sizeof(item->handle) - 1, author,
                      "handle");
    if (item->handle[1] != '\0')
        item->handle[0] = '@';
    else
        item->handle[0] = '\0';

    notification_copy(item->reason, sizeof(item->reason), value, "reason");
    notification_time(item->time, sizeof(item->time), value, "indexedAt");
    item->is_read = notification_read_flag(value, "isRead");

    return 1;
}

void platinum_notifications_init(platinum_notifications *notifications)
{
    if (notifications == NULL)
        return;

    memset(notifications, 0, sizeof(*notifications));
    notifications_status(notifications, "No notifications loaded.");
}

wf_status platinum_notifications_refresh(
    platinum_notifications *notifications,
    platinum_bridge_client *bridge)
{
    wf_response response;
    platinum_json root;
    platinum_json items;
    platinum_json value;
    long index;
    long available;
    wf_status status;

    if (notifications == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;

    memset(&response, 0, sizeof(response));
    notifications->loading = 1;
    notifications->count = 0;
    notifications->scroll_row = 0;
    notifications_status(notifications, kLoading);

    status = platinum_bridge_get(bridge,
                                 "/v1/notifications?limit=20",
                                 &response);
    if (status != WF_OK) {
        notifications->loading = 0;
        if (status == WF_ERR_AUTH)
            notifications_status(
                notifications, "Session expired. Pair the account again.");
        else
            notifications_status(notifications,
                                  "Notification refresh failed.");
        wf_response_free(&response);
        return status;
    }

    status = platinum_json_open(&root,
                               response.body != NULL ? response.body : "");
    if (status != WF_OK) {
        notifications->loading = 0;
        notifications_status(notifications,
                             "The bridge returned invalid notifications.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    /* notifications has to be an array. A response that omits it is reported as
     * invalid rather than shown as an empty list, because those are different
     * facts and conflating them hides a broken bridge. */
    if (platinum_json_member(root, "notifications", &items) != WF_OK ||
        platinum_json_count(items, &available) != WF_OK) {
        notifications->loading = 0;
        notifications_status(notifications,
                             "The bridge returned an invalid notification list.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    if (available > PLATINUM_NOTIFICATIONS_MAX)
        available = PLATINUM_NOTIFICATIONS_MAX;

    for (index = 0; index < available; ++index) {
        if (platinum_json_element(items, index, &value) != WF_OK)
            continue;
        if (notifications_parse_item(
                &notifications->items[notifications->count],
                value))
            ++notifications->count;
    }

    notifications->cursor[0] = '\0';
    notification_copy(notifications->cursor, sizeof(notifications->cursor),
                      root, "cursor");

    /* Everything has been copied out of response.body by now. */
    wf_response_free(&response);

    notifications->loading = 0;
    if (notifications->count == 0)
        notifications_status(notifications, kEmpty);
    else
        notifications_status(notifications, kRefreshHint);

    return WF_OK;
}

static void notification_text(const char *text, short x, short y)
{
    if (text == NULL || text[0] == '\0')
        return;

    MoveTo(x, y);
    DrawText((Ptr)text, 0, (short)strlen(text));
}

static void notification_button(const Rect *bounds, StringPtr title)
{
    long width;
    short baseline;

    FrameRect(bounds);
    width = StringWidth(title);
    baseline = bounds->top + 14;
    MoveTo(bounds->left +
               (short)((bounds->right - bounds->left - width) / 2),
           baseline);
    DrawString(title);
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
    notification_text(item->author, 12, top);

    MoveTo(120, top);
    notification_text(item->handle, 120, top);

    MoveTo(230, top);
    notification_text(item->reason, 230, top);

    MoveTo(450, top);
    notification_text(item->time, 450, top);

    if (!item->is_read) {
        MoveTo(12, top + 16);
        notification_text("New", 12, top + 16);
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

    notification_text("Author", 12, 24);
    notification_text("Handle", 120, 24);
    notification_text("Reason", 230, 24);
    notification_text("Time", 450, 24);

    if (notifications->status[0] != '\0')
        notification_text(notifications->status, 12, 54);

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
    notification_button(&close_rect, kClose);

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
