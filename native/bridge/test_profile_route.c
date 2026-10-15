#include "profile_route.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* A fake fetch that records what it was asked for and answers from a script. */
typedef struct fake {
    int mode;           /* 0 ok, 1 not found, 2 error */
    char last_actor[256];
    int last_was_self;
    int calls;
} fake;

static platinum_fetch_result
fake_fetch(void *ctx, const char *actor, platinum_profile *out)
{
    fake *f = (fake *) ctx;

    f->calls++;
    f->last_was_self = actor == NULL;
    f->last_actor[0] = '\0';
    if (actor) strcpy(f->last_actor, actor);
    if (f->mode == 1) return PLATINUM_FETCH_NOT_FOUND;
    if (f->mode == 2) return PLATINUM_FETCH_ERROR;
    memset(out, 0, sizeof *out);
    out->following = -1;
    out->followed_by = -1;
    out->muted = -1;
    out->blocking = -1;
    strcpy(out->did, "did:plc:abc");
    out->has_handle = 1;
    strcpy(out->handle, actor ? actor : "me.test");
    return PLATINUM_FETCH_OK;
}

int
main(void)
{
    fake f;
    char body[1024];
    int status;

    /* Found: 200, the encoded profile, and the actor was normalised first. */
    memset(&f, 0, sizeof f);
    status = platinum_route_profile(&f, fake_fetch, "Alice.Test", body, sizeof body);
    CHECK(status == 200);
    CHECK(strcmp(f.last_actor, "alice.test") == 0);
    CHECK(strstr(body, "\"handle\":\"alice.test\"") != NULL);

    /* No actor parameter: your own profile, fetched with NULL. */
    memset(&f, 0, sizeof f);
    status = platinum_route_profile(&f, fake_fetch, NULL, body, sizeof body);
    CHECK(status == 200);
    CHECK(f.last_was_self);

    /* Bad actor: 400 before any fetch happens. */
    memset(&f, 0, sizeof f);
    status = platinum_route_profile(&f, fake_fetch, "not a handle", body, sizeof body);
    CHECK(status == 400);
    CHECK(f.calls == 0);
    CHECK(strstr(body, "\"error\":\"invalid_actor\"") != NULL);
    CHECK(strstr(body, "actor must be a handle or a DID.") != NULL);

    /* Unknown account: 404 actor_not_found. */
    memset(&f, 0, sizeof f);
    f.mode = 1;
    status = platinum_route_profile(&f, fake_fetch, "ghost.test", body, sizeof body);
    CHECK(status == 404);
    CHECK(strcmp(body, "{\"error\":\"actor_not_found\",\"message\":\"No such account.\"}") == 0);

    /* Upstream failure: 500 bridge_error, the same generic message the TypeScript sends. */
    memset(&f, 0, sizeof f);
    f.mode = 2;
    status = platinum_route_profile(&f, fake_fetch, "alice.test", body, sizeof body);
    CHECK(status == 500);
    CHECK(strcmp(body, "{\"error\":\"bridge_error\",\"message\":\"The backend could not complete the request.\"}") == 0);

    /* Your own profile missing is a failure, not a 404. */
    memset(&f, 0, sizeof f);
    f.mode = 1;
    status = platinum_route_profile(&f, fake_fetch, NULL, body, sizeof body);
    CHECK(status == 500);

    /* A body that does not fit becomes the generic 500, not a truncated 200. */
    memset(&f, 0, sizeof f);
    status = platinum_route_profile(&f, fake_fetch, "alice.test", body, 16);
    CHECK(status == 500);
    CHECK(strlen(body) < 16);

    if (failures) {
        fprintf(stderr, "test_profile_route: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_profile_route: %d checks passed\n", checks);
    return 0;
}
