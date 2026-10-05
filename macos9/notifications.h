#ifndef PLATINUM_NOTIFICATIONS_H
#define PLATINUM_NOTIFICATIONS_H

#include "bridge_client.h"
#include "scrollbar.h"

#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_NOTIFICATIONS_MAX 20
#define PLATINUM_NOTIFICATION_AUTHOR_MAX 64
#define PLATINUM_NOTIFICATION_HANDLE_MAX 64
#define PLATINUM_NOTIFICATION_REASON_MAX 32
#define PLATINUM_NOTIFICATION_TIME_MAX 8
#define PLATINUM_NOTIFICATIONS_STATUS_MAX 127

typedef struct platinum_notification {
    char uri[513];
    char cid[256];
    char author[PLATINUM_NOTIFICATION_AUTHOR_MAX];
    char handle[PLATINUM_NOTIFICATION_HANDLE_MAX];
    char reason[PLATINUM_NOTIFICATION_REASON_MAX];
    char time[PLATINUM_NOTIFICATION_TIME_MAX];
    int is_read;
} platinum_notification;

typedef struct platinum_notifications {
    WindowPtr window;
    platinum_scrollbar scrollbar;
    platinum_notification items[PLATINUM_NOTIFICATIONS_MAX];
    unsigned short count;
    short scroll_row;
    char cursor[256];
    /* indexedAt of the newest item, exactly as sent, for marking seen. */
    char newest_at[65];
    int any_unread;
    char status[PLATINUM_NOTIFICATIONS_STATUS_MAX + 1];
    int loading;
} platinum_notifications;

enum {
    PLATINUM_NOTIFICATIONS_NONE = 0,
    PLATINUM_NOTIFICATIONS_CLOSE = 1,
    PLATINUM_NOTIFICATIONS_REFRESH = 2
};

void platinum_notifications_init(platinum_notifications *notifications);
wf_status platinum_notifications_refresh(
    platinum_notifications *notifications,
    platinum_bridge_client *bridge);
/*
 * Mark everything up to the newest loaded notification as seen, if any loaded
 * item was unread. Uses the newest item's own timestamp, not the Mac clock, so
 * anything that arrived after the list was fetched stays unread. Rows keep the
 * read state they were loaded with, so new ones stay marked until the next
 * refresh.
 */
wf_status platinum_notifications_mark_seen(
    platinum_notifications *notifications,
    platinum_bridge_client *bridge);
OSErr platinum_notifications_open(platinum_notifications *notifications);
void platinum_notifications_close(platinum_notifications *notifications);
int platinum_notifications_handle_event(
    platinum_notifications *notifications,
    EventRecord *event);
void platinum_notifications_draw(platinum_notifications *notifications);

#ifdef __cplusplus
}
#endif

#endif
