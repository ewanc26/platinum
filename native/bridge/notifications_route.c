#include "notifications_route.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search_routes.h"

void
platinum_notifications_free(platinum_notifications *out)
{
    if (out == NULL) return;
    free(out->items);
    memset(out, 0, sizeof *out);
}

static int
write_error(char *body, size_t cap, int status, const char *code, const char *message)
{
    if (strlen(code) + strlen(message) + 32 >= cap) {
        body[0] = '\0';
        return 500;
    }
    strcpy(body, "{\"error\":\"");
    strcat(body, code);
    strcat(body, "\",\"message\":\"");
    strcat(body, message);
    strcat(body, "\"}");
    return status;
}

/* Appends s at *len. Returns 0, or -1 if it would not fit. */
static int
append_raw(char *body, size_t cap, size_t *len, const char *s)
{
    size_t n = strlen(s);
    if (*len + n >= cap) return -1;
    memcpy(body + *len, s, n);
    *len += n;
    body[*len] = '\0';
    return 0;
}

/*
 * Appends s as a JSON string. The bytes are copied unchanged, which is correct
 * for the UTF-8 the AppView sends; control characters are escaped because a raw
 * one makes the body invalid JSON. Returns 0, or -1 if it would not fit.
 */
static int
append_string(char *body, size_t cap, size_t *len, const char *s)
{
    const unsigned char *p;

    if (*len + 1 >= cap) return -1;
    body[(*len)++] = '"';
    for (p = (const unsigned char *) s; *p != '\0'; p++) {
        const char *piece = NULL;
        char unicode[7];

        switch (*p) {
        case '"':  piece = "\\\""; break;
        case '\\': piece = "\\\\"; break;
        case '\n': piece = "\\n"; break;
        case '\r': piece = "\\r"; break;
        case '\t': piece = "\\t"; break;
        default:
            if (*p < 0x20) {
                sprintf(unicode, "\\u%04x", (unsigned) *p);
                piece = unicode;
            }
            break;
        }
        if (piece != NULL) {
            if (append_raw(body, cap, len, piece) != 0) return -1;
        } else {
            if (*len + 1 >= cap) return -1;
            body[(*len)++] = (char) *p;
        }
    }
    if (*len + 1 >= cap) return -1;
    body[(*len)++] = '"';
    body[*len] = '\0';
    return 0;
}

static int
append_key_string(char *body, size_t cap, size_t *len, const char *key, const char *value)
{
    if (append_raw(body, cap, len, ",\"") != 0) return -1;
    if (append_raw(body, cap, len, key) != 0) return -1;
    if (append_raw(body, cap, len, "\":") != 0) return -1;
    return append_string(body, cap, len, value);
}

static int
append_author(char *body, size_t cap, size_t *len, const platinum_notification_author *a)
{
    if (append_raw(body, cap, len, "{\"did\":") != 0 ||
        append_string(body, cap, len, a->did) != 0) {
        return -1;
    }
    /* The optional fields are left out rather than sent empty, as api.ts does. */
    if (a->handle[0] != '\0' && append_key_string(body, cap, len, "handle", a->handle) != 0) {
        return -1;
    }
    if (a->display_name[0] != '\0' &&
        append_key_string(body, cap, len, "displayName", a->display_name) != 0) {
        return -1;
    }
    if (a->avatar[0] != '\0' && append_key_string(body, cap, len, "avatar", a->avatar) != 0) {
        return -1;
    }
    return append_raw(body, cap, len, "}");
}

static int
append_item(char *body, size_t cap, size_t *len, const platinum_notification *item)
{
    if (append_raw(body, cap, len, "{\"uri\":") != 0 ||
        append_string(body, cap, len, item->uri) != 0 ||
        append_key_string(body, cap, len, "cid", item->cid) != 0 ||
        append_raw(body, cap, len, ",\"author\":") != 0 ||
        append_author(body, cap, len, &item->author) != 0 ||
        append_key_string(body, cap, len, "reason", item->reason) != 0 ||
        append_key_string(body, cap, len, "indexedAt", item->indexed_at) != 0) {
        return -1;
    }
    return append_raw(body, cap, len, item->is_read ? ",\"isRead\":true" : ",\"isRead\":false");
}

int
platinum_route_notifications(void *ctx, platinum_notifications_fetch_fn fetch,
                             const char *limit_param, const char *cursor_param,
                             char *body, size_t cap)
{
    platinum_notifications page;
    size_t len = 0;
    size_t i;
    int count;

    if (body == NULL || cap == 0 || fetch == NULL) return 500;
    body[0] = '\0';

    /* A cursor is passed through exactly; one that cannot fit the response field
     * is refused rather than truncated, since a cut cursor pages nowhere. */
    if (cursor_param != NULL && strlen(cursor_param) >= sizeof page.cursor) {
        return write_error(body, cap, 400, "invalid_cursor",
                           "cursor must be 255 bytes or fewer.");
    }

    count = platinum_query_limit(limit_param);
    memset(&page, 0, sizeof page);
    if (fetch(ctx, count, cursor_param, &page) != PLATINUM_NOTIFICATIONS_OK) {
        platinum_notifications_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }
    if (page.item_count > PLATINUM_NOTIFICATIONS_MAX) {
        platinum_notifications_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }

    if (append_raw(body, cap, &len, "{\"notifications\":[") != 0) goto too_big;
    for (i = 0; i < page.item_count; i++) {
        if (i > 0 && append_raw(body, cap, &len, ",") != 0) goto too_big;
        if (append_item(body, cap, &len, &page.items[i]) != 0) goto too_big;
    }
    if (append_raw(body, cap, &len, "]") != 0) goto too_big;
    if (page.has_cursor && page.cursor[0] != '\0') {
        if (append_key_string(body, cap, &len, "cursor", page.cursor) != 0) goto too_big;
    }
    if (append_raw(body, cap, &len, "}") != 0) goto too_big;

    platinum_notifications_free(&page);
    return 200;

too_big:
    platinum_notifications_free(&page);
    body[0] = '\0';
    return write_error(body, cap, 500, "bridge_error",
                       "The backend could not complete the request.");
}
