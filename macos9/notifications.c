#include "notifications.h"
#include "scrollbar.h"
#include "text_codec.h"

#include <Quickdraw.h>
#include <cJSON.h>
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

static void notification_copy(char *destination,
                              long capacity,
                              const cJSON *value)
{
    long length;

    if (destination == NULL || capacity <= 0)
        return;

    destination[0] = '\0';
    if (value == NULL || !cJSON_IsString(value) ||
        value->valuestring == NULL)
        return;

    length = platinum_text_utf8_to_macroman(value->valuestring,
                                            destination,
                                            capacity,
                                            NULL);
    if (length < 0)
        destination[0] = '\0';
}

static void notification_time(char *destination,
                              long capacity,
                              const cJSON *value)
{
    if (destination == NULL || capacity <= 0)
        return;

    destination[0] = '\0';
    if (value == NULL || !cJSON_IsString(value) ||
        value->valuestring == NULL)
        return;

    if (strlen(value->valuestring) >= 16 && capacity >= 6) {
        memcpy(destination, value->valuestring + 11, 5);
        destination[5] = '\0';
    } else {
        notification_copy(destination, capacity, value);
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

static long notification_number(const cJSON *value,
                                const char *name)
{
    const cJSON *item;

    item = cJSON_GetObjectItemCaseSensitive(value, name);
    if (item == NULL || !cJSON_IsBool(item))
        return 0;
    return cJSON_IsTrue(item);
}

static int notifications_parse_item(platinum_notification *item,
                                     const cJSON *value)
{
    const cJSON *author;
    const cJSON *did;
    const cJSON *handle;
    const cJSON *display_name;
    const cJSON *indexed_at;

    if (item == NULL || value == NULL || !cJSON_IsObject(value))
        return 0;

    memset(item, 0, sizeof(*item));

    if (!notification_copy(item->uri, sizeof(item->uri),
                           cJSON_GetObjectItemCaseSensitive(value, "uri")))
        return 0;
    if (!notification_copy(item->cid, sizeof(item->cid),
                           cJSON_GetObjectItemCaseSensitive(value, "cid")))
        return 0;

    author = cJSON_GetObjectItemCaseSensitive(value, "author");
    if (author == NULL || !cJSON_IsObject(author))
        return 0;

    did = cJSON_GetObjectItemCaseSensitive(author, "did");
    if (did == NULL || !cJSON_IsString(did))
        return 0;

    handle = cJSON_GetObjectItemCaseSensitive(author, "handle");
    display_name = cJSON_GetObjectItemCaseSensitive(author, "displayName");

    notification_copy(item->author, sizeof(item->author), display_name);
    if (item->author[0] == '\0')
        notification_copy(item->author, sizeof(item->author), handle);
    if (item->author[0] == '\0')
        notification_copy(item->author, sizeof(item->author), did);

    item->handle[0] = '@';
    if (handle != NULL && cJSON_IsString(handle) &&
        handle->valuestring != NULL) {
        notification_copy(item->handle + 1,
                          sizeof(item->handle) - 1,
                          handle);
    } else {
        item->handle[1] = '\0';
    }

    notification_copy(item->reason, sizeof(item->reason),
                      cJSON_GetObjectItemCaseSensitive(value, "reason"));
    indexed_at = cJSON_GetObjectItemCaseSensitive(value, "indexedAt");
    notification_time(item->time, sizeof(item->time), indexed_at);
    item->is_read = (int)notification_number(value, "isRead");

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
    cJSON *root;
    cJSON *items;
    cJSON *cursor;
    cJSON *value;
    int index;
    int parsed;
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

    root = cJSON_Parse(response.body != NULL ? response.body : "");
    if (root == NULL) {
        notifications->loading = 0;
        notifications_status(notifications,
                             "The bridge returned invalid notifications.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    items = cJSON_GetObjectItemCaseSensitive(root, "notifications");
    if (items == NULL || !cJSON_IsArray(items)) {
        cJSON_Delete(root);
        notifications->loading = 0;
        notifications_status(notifications,
                             "The bridge returned an invalid notification list.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    parsed = cJSON_GetArraySize(items);
    if (parsed > PLATINUM_NOTIFICATIONS_MAX)
        parsed = PLATINUM_NOTIFICATIONS_MAX;

    for (index = 0; index < parsed; ++index) {
        value = cJSON_GetArrayItem(items, index);
        if (notifications_parse_item(
                &notifications->items[notifications->count],
                value))
            ++notifications->count;
    }

    cursor = cJSON_GetObjectItemCaseSensitive(root, "cursor");
    notifications->cursor[0] = '\0';
    if (cursor != NULL && cJSON_IsString(cursor) &&
        cursor->valuestring != NULL) {
        notification_copy(notifications->cursor,
                          sizeof(notifications->cursor),
                          cursor);
    }

    cJSON_Delete(root);
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
    notifications->window = NewCWindow(NULL, bounds, kNotificationsTitle,
                                       true, documentProc, (WindowPtr)-1L,
                                       true, 0L);
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
            if ((WindowPtr)event->message == notifications->window) {
                BeginUpdate(notifications->window);
                platinum_notifications_draw(notifications);
                EndUpdate(notifications->window);
            }
            return PLATINUM_NOTIFICATIONS_NONE;

        case activateEvt:
            if ((WindowPtr)event->message == notifications->window)
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
