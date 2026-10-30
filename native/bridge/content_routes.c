#include "content_routes.h"

#include <stdio.h>
#include <string.h>

#include "profile.h"
#include "validate.h"

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

/* Appends s as a JSON string to body at *len. Words are validated as having no control
 * characters; quotes and backslashes are escaped. Returns 0, or -1 if it would not fit. */
static int
append_json_string(char *body, size_t cap, size_t *len, const char *s)
{
    const char *p;
    if (*len + 1 >= cap) return -1;
    body[(*len)++] = '"';
    for (p = s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            if (*len + 2 >= cap) return -1;
            body[(*len)++] = '\\';
        } else if (*len + 1 >= cap) {
            return -1;
        }
        body[(*len)++] = *p;
    }
    if (*len + 2 > cap) return -1;
    body[(*len)++] = '"';
    body[*len] = '\0';
    return 0;
}

static int
append_text(char *body, size_t cap, size_t *len, const char *s)
{
    size_t n = strlen(s);
    if (*len + n >= cap) return -1;
    memcpy(body + *len, s, n + 1);
    *len += n;
    return 0;
}

int
platinum_route_muted_list(void *ctx, platinum_muted_list_fn fn, char *body, size_t cap)
{
    static platinum_muted_view views[PLATINUM_MUTED_LIST_MAX];
    size_t count = 0;
    size_t i;
    size_t len = 0;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (fn(ctx, views, PLATINUM_MUTED_LIST_MAX, &count) != PLATINUM_CONTENT_OK) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    if (count > PLATINUM_MUTED_LIST_MAX) count = PLATINUM_MUTED_LIST_MAX;

    if (append_text(body, cap, &len, "{\"words\":[") != 0) goto too_big;
    for (i = 0; i < count; i++) {
        if (i > 0 && append_text(body, cap, &len, ",") != 0) goto too_big;
        if (append_text(body, cap, &len, "{\"value\":") != 0) goto too_big;
        if (append_json_string(body, cap, &len, views[i].value) != 0) goto too_big;
        if (append_text(body, cap, &len, ",\"targets\":[") != 0) goto too_big;
        /* Targets in their fixed order: content, then tag. */
        if (views[i].content) {
            if (append_json_string(body, cap, &len, "content") != 0) goto too_big;
            if (views[i].tag && append_text(body, cap, &len, ",") != 0) goto too_big;
        }
        if (views[i].tag && append_json_string(body, cap, &len, "tag") != 0) goto too_big;
        if (append_text(body, cap, &len, "]}") != 0) goto too_big;
    }
    if (append_text(body, cap, &len, "]}") != 0) goto too_big;
    return 200;

too_big:
    return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
}

int
platinum_route_post_delete(void *ctx, platinum_delete_fn fn, const char *uri,
                           char *body, size_t cap)
{
    platinum_content_result r;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    /* The TypeScript checks the URI with a placeholder cid; only the URI shape matters. */
    if (platinum_valid_post_ref(uri, "x") != 0) {
        return write_error(body, cap, 400, "invalid_post_ref", "uri must be an app.bsky.feed.post AT URI.");
    }
    r = fn(ctx, uri);
    if (r == PLATINUM_CONTENT_NOT_YOURS) {
        return write_error(body, cap, 403, "not_your_post", "You can only delete your own posts.");
    }
    if (r != PLATINUM_CONTENT_OK) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    if (strlen(uri) + 32 >= cap) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    sprintf(body, "{\"uri\":\"%s\",\"deleted\":true}", uri);
    return 200;
}
