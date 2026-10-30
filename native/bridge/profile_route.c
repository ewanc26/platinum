#include "profile_route.h"

#include <string.h>

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
platinum_route_profile(void *ctx, platinum_profile_fetch_fn fetch,
                       const char *actor_param, char *body, size_t cap)
{
    char actor[256];
    platinum_profile profile;
    platinum_fetch_result result;

    if (body == NULL || cap == 0 || fetch == NULL) return 500;
    body[0] = '\0';

    if (actor_param != NULL) {
        if (platinum_valid_actor(actor_param, actor, sizeof actor) != 0) {
            return write_error(body, cap, 400, "invalid_actor", "actor must be a handle or a DID.");
        }
        result = fetch(ctx, actor, &profile);
        if (result == PLATINUM_FETCH_NOT_FOUND) {
            return write_error(body, cap, 404, "actor_not_found", "No such account.");
        }
    } else {
        /* Your own profile: the server's own account, so a missing one is a failure, not a 404. */
        result = fetch(ctx, NULL, &profile);
        if (result == PLATINUM_FETCH_NOT_FOUND) result = PLATINUM_FETCH_ERROR;
    }

    if (result != PLATINUM_FETCH_OK) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }

    if (platinum_profile_json(&profile, body, cap) < 0) {
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    return 200;
}
