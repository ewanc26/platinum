#include "write_routes.h"

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

int
platinum_route_toggle(void *ctx, platinum_toggle_fn fn, int kind, const char *uri,
                      const char *cid, int on_valid, int on, char *body, size_t cap)
{
    int out_on = 0;
    long out_count = 0;
    platinum_write_result r;
    size_t need;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (!on_valid || platinum_valid_post_ref(uri, cid) != 0) {
        return write_error(body, cap, 400, "invalid_post_ref", "A post uri, cid and a boolean \"on\" are required.");
    }

    r = fn(ctx, kind, uri, cid, on, &out_on, &out_count);
    if (r != PLATINUM_WRITE_OK) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    /* The post is gone while the client asked to like or repost it. */
    if (!out_on && on) {
        return write_error(body, cap, 404, "post_not_found", "The post no longer exists.");
    }

    /* uri has passed platinum_valid_post_ref, so it holds no characters that
     * need escaping. */
    need = strlen(uri) + 64;
    if (need >= cap) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    sprintf(body, "{\"uri\":\"%s\",\"on\":%s,\"count\":%ld}", uri, out_on ? "true" : "false", out_count);
    return 200;
}

int
platinum_route_subject(void *ctx, platinum_subject_fn fn, int kind, const char *did,
                       int on_valid, int on, char *body, size_t cap)
{
    platinum_write_result r;
    size_t need;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (!on_valid || platinum_valid_subject_did(did) != 0) {
        return write_error(body, cap, 400, "invalid_actor", "A DID and a boolean \"on\" are required.");
    }

    r = fn(ctx, kind, did, on);
    if (r == PLATINUM_WRITE_NOT_FOUND) {
        return write_error(body, cap, 404, "actor_not_found", "No such account.");
    }
    if (r != PLATINUM_WRITE_OK) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }

    /* did has passed platinum_valid_subject_did, so it needs no escaping. */
    need = strlen(did) + 32;
    if (need >= cap) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    sprintf(body, "{\"did\":\"%s\",\"on\":%s}", did, on ? "true" : "false");
    return 200;
}
