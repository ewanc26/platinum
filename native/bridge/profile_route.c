#include "profile_route.h"

#include <string.h>

static int
write_error(char *body, size_t cap, const char *code, const char *message)
{
    size_t n;
    /* The codes and messages are fixed strings with no characters to escape. */
    n = strlen(code) + strlen(message) + 32;
    if (n >= cap) {
        body[0] = '\0';
        return 500;
    }
    strcpy(body, "{\"error\":\"");
    strcat(body, code);
    strcat(body, "\",\"message\":\"");
    strcat(body, message);
    strcat(body, "\"}");
    return 0;
}

int
platinum_route_profile(void *ctx, platinum_profile_fetch_fn fetch,
                       const char *actor_param, char *body, size_t cap)
{
    char actor[256];
    platinum_profile profile;
    platinum_fetch_result result;
    int status;

    if (body == NULL || cap == 0 || fetch == NULL) return 500;
    body[0] = '\0';

    if (actor_param != NULL) {
        if (platinum_valid_actor(actor_param, actor, sizeof actor) != 0) {
            write_error(body, cap, "invalid_actor", "actor must be a handle or a DID.");
            return 400;
        }
        result = fetch(ctx, actor, &profile);
        if (result == PLATINUM_FETCH_NOT_FOUND) {
            write_error(body, cap, "actor_not_found", "No such account.");
            return 404;
        }
    } else {
        /* Your own profile: the server's own account, so a missing one is a failure, not a 404. */
        result = fetch(ctx, NULL, &profile);
        if (result == PLATINUM_FETCH_NOT_FOUND) result = PLATINUM_FETCH_ERROR;
    }

    if (result != PLATINUM_FETCH_OK) {
        write_error(body, cap, "bridge_error", "The backend could not complete the request.");
        return 500;
    }

    status = 200;
    if (platinum_profile_json(&profile, body, cap) < 0) {
        write_error(body, cap, "bridge_error", "The backend could not complete the request.");
        status = 500;
    }
    return status;
}
