#include "account_routes.h"

#include <stdio.h>
#include <string.h>

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

int
platinum_route_seen(void *ctx, platinum_seen_fn fn, const char *seen_at, double now_ms,
                    char *body, size_t cap)
{
    char normalised[32];

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (platinum_valid_seen_at(seen_at, now_ms, normalised, sizeof normalised) != 0) {
        return write_error(body, cap, 400, "invalid_seen_at", "seenAt must be an ISO 8601 timestamp.");
    }
    if (fn(ctx, normalised) != PLATINUM_ACCOUNT_OK) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    /* normalised is digits, dashes, colons, dots and T and Z only: nothing to escape. */
    if (strlen(normalised) + 16 >= cap) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    sprintf(body, "{\"seenAt\":\"%s\"}", normalised);
    return 200;
}

/* Writes s as a JSON string body, escaping quotes and backslashes. Control characters
 * are not in s: the validator refuses them. Returns 0, or -1 if it does not fit. */
static int
json_string(const char *s, char *out, size_t cap)
{
    size_t n = 0;
    const char *p;

    if (cap < 3) return -1;
    out[n++] = '"';
    for (p = s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            if (n + 3 >= cap) return -1;
            out[n++] = '\\';
        } else if (n + 2 >= cap) {
            return -1;
        }
        out[n++] = *p;
    }
    if (n + 2 > cap) return -1;
    out[n++] = '"';
    out[n] = '\0';
    return 0;
}

int
platinum_route_muted_set(void *ctx, platinum_word_fn fn, const char *word, int on_valid,
                         int on, char *body, size_t cap)
{
    char trimmed[512];
    char quoted[640];

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (!on_valid || platinum_valid_muted_word(word, trimmed, sizeof trimmed) != 0) {
        return write_error(body, cap, 400, "invalid_word", "A word of 1 to 100 characters and a boolean \"on\" are required.");
    }
    if (fn(ctx, trimmed, on) != PLATINUM_ACCOUNT_OK) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    if (json_string(trimmed, quoted, sizeof quoted) != 0 || strlen(quoted) + 16 >= cap) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    sprintf(body, "{\"value\":%s,\"on\":%s}", quoted, on ? "true" : "false");
    return 200;
}
