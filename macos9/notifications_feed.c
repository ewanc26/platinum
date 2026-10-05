/*
 * notifications_feed.c -- the Notifications list: fetching, parsing, paging
 * and marking seen. No QuickDraw here, so it links on a host for tests; the
 * window that draws it is notifications.c.
 */
#include "notifications.h"
#include "text_codec.h"

#include "json_min.h"
#include <string.h>

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
    /* No InvalRect here: this file draws nothing. The application invalidates
     * the window after every call that changes the list. */
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

    /* The newest item comes first. Its timestamp is an identifier for the
     * bridge, so it is copied exactly or not at all. */
    notifications->newest_at[0] = '\0';
    notifications->any_unread = 0;
    for (index = 0; index < notifications->count; ++index)
        if (!notifications->items[index].is_read)
            notifications->any_unread = 1;
    if (notifications->count > 0 &&
        platinum_json_element(items, 0, &value) == WF_OK &&
        platinum_json_string(value, "indexedAt", notifications->newest_at,
                             sizeof(notifications->newest_at)) != WF_OK)
        notifications->newest_at[0] = '\0';

    /* Everything has been copied out of response.body by now. */
    wf_response_free(&response);

    notifications->loading = 0;
    if (notifications->count == 0)
        notifications_status(notifications, kEmpty);
    else
        notifications_status(notifications, kRefreshHint);

    return WF_OK;
}

wf_status platinum_notifications_mark_seen(
    platinum_notifications *notifications,
    platinum_bridge_client *bridge)
{
    if (notifications == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;
    if (!notifications->any_unread || notifications->newest_at[0] == '\0')
        return WF_OK;
    {
        wf_status status;
        status = platinum_bridge_mark_seen(bridge, notifications->newest_at);
        if (status == WF_OK)
            notifications->any_unread = 0;
        return status;
    }
}

