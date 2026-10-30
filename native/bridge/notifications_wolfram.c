#include "notifications_wolfram.h"

#include <stdlib.h>
#include <string.h>

#include <wolfram/agent.h>

#include "image_route.h"

/* Copies src into dst, cutting to fit cap bytes on a UTF-8 boundary. The native
 * model has fixed buffers, so the Node bridge's code-point limits are applied as
 * the byte prefix that fits; a cut never splits a code point. */
static void
clip_bytes(const char *src, char *dst, size_t cap)
{
    size_t n;

    if (dst == NULL || cap == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        while (n > 0 && ((unsigned char) src[n] & 0xC0) == 0x80) n--;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

platinum_notifications_result
platinum_notifications_fetch_live(void *ctx, int limit, const char *cursor,
                                  platinum_notifications *out)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_notification_list list;
    size_t i;
    size_t count;

    if (agent == NULL || out == NULL) return PLATINUM_NOTIFICATIONS_ERROR;
    memset(&list, 0, sizeof list);

    if (wf_agent_list_notifications_typed(agent, limit, cursor, &list) != WF_OK) {
        wf_agent_notification_list_free(&list);
        return PLATINUM_NOTIFICATIONS_ERROR;
    }

    count = list.notification_count < PLATINUM_NOTIFICATIONS_MAX
                ? list.notification_count
                : PLATINUM_NOTIFICATIONS_MAX;
    if (count > 0) {
        out->items = calloc(count, sizeof *out->items);
        if (out->items == NULL) {
            wf_agent_notification_list_free(&list);
            return PLATINUM_NOTIFICATIONS_ERROR;
        }
        for (i = 0; i < count; i++) {
            const wf_agent_notification *src = &list.notifications[i];
            platinum_notification *item = &out->items[out->item_count];

            clip_bytes(src->uri, item->uri, sizeof item->uri);
            clip_bytes(src->cid, item->cid, sizeof item->cid);
            clip_bytes(src->author.did, item->author.did, sizeof item->author.did);
            clip_bytes(src->author.handle, item->author.handle,
                       sizeof item->author.handle);
            clip_bytes(src->author.display_name, item->author.display_name,
                       sizeof item->author.display_name);
            platinum_avatar_ref(src->author.avatar, item->author.avatar,
                                sizeof item->author.avatar);
            clip_bytes(src->reason, item->reason, sizeof item->reason);
            clip_bytes(src->indexed_at, item->indexed_at, sizeof item->indexed_at);
            item->is_read = src->is_read ? 1 : 0;
            out->item_count++;
        }
    }

    /* A cursor the Mac's field cannot hold is not offered: a cut cursor would
     * page nowhere, and the client's "load older" then stays disabled. */
    if (list.cursor != NULL && list.cursor[0] != '\0' &&
        strlen(list.cursor) < sizeof out->cursor) {
        strcpy(out->cursor, list.cursor);
        out->has_cursor = 1;
    }

    wf_agent_notification_list_free(&list);
    return PLATINUM_NOTIFICATIONS_OK;
}

platinum_account_result
platinum_notifications_seen_live(void *ctx, const char *seen_at)
{
    wf_agent *agent = (wf_agent *) ctx;

    if (agent == NULL || seen_at == NULL) return PLATINUM_ACCOUNT_ERROR;
    if (wf_agent_update_seen_notifications(agent, seen_at) != WF_OK) {
        return PLATINUM_ACCOUNT_ERROR;
    }
    return PLATINUM_ACCOUNT_OK;
}
