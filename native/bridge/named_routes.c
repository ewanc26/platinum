#include "named_routes.h"

#include <stdio.h>
#include <string.h>

/* Appends s as a JSON string to body at *len, escaping quotes and backslashes.
 * Returns 0, or -1 if it would not fit. */
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
write_error(char *body, size_t cap, int status)
{
    static const char message[] =
        "{\"error\":\"bridge_error\",\"message\":\"The backend could not complete the request.\"}";

    if (sizeof message > cap) {
        if (cap > 0) body[0] = '\0';
        return 500;
    }
    memcpy(body, message, sizeof message);
    return status;
}

int
platinum_route_named(void *ctx, platinum_named_fn fn, char *body, size_t cap)
{
    static platinum_named_view views[PLATINUM_NAMED_MAX];
    size_t count = 0;
    size_t len = 0;
    size_t i;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (fn(ctx, views, PLATINUM_NAMED_MAX, &count) != PLATINUM_NAMED_OK) {
        return write_error(body, cap, 500);
    }
    if (count > PLATINUM_NAMED_MAX) count = PLATINUM_NAMED_MAX;

    if (len + 11 >= cap) return write_error(body, cap, 500);
    memcpy(body + len, "{\"items\":[", 10);
    len += 10;
    body[len] = '\0';

    for (i = 0; i < count; i++) {
        if (i > 0) {
            if (len + 2 >= cap) return write_error(body, cap, 500);
            body[len++] = ',';
            body[len] = '\0';
        }
        if (len + 1 >= cap) return write_error(body, cap, 500);
        body[len++] = '{';
        body[len] = '\0';
        if (len + 6 >= cap) return write_error(body, cap, 500);
        memcpy(body + len, "\"uri\":", 6);
        len += 6;
        if (append_json_string(body, cap, &len, views[i].uri) != 0) {
            return write_error(body, cap, 500);
        }
        if (len + 8 >= cap) return write_error(body, cap, 500);
        memcpy(body + len, ",\"name\":", 8);
        len += 8;
        if (append_json_string(body, cap, &len, views[i].name) != 0) {
            return write_error(body, cap, 500);
        }
        if (len + 2 >= cap) return write_error(body, cap, 500);
        body[len++] = '}';
        body[len] = '\0';
    }

    if (len + 2 >= cap) return write_error(body, cap, 500);
    body[len++] = ']';
    body[len++] = '}';
    body[len] = '\0';
    return 200;
}
