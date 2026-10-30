#include "routes.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(void)
{
    platinum_route_id id = PLATINUM_ROUTE_NONE;
    int needs = -1;
    char tok[64];

    /* Every route in server.ts is present: 36 entries (muted-words has GET and POST). */
    CHECK(platinum_route_count() == 36);

    /* Public routes do not need a token. */
    CHECK(platinum_route_match("GET", "/health", &id, &needs) == PLATINUM_ROUTE_FOUND);
    CHECK(id == PLATINUM_ROUTE_HEALTH && needs == 0);
    CHECK(platinum_route_match("POST", "/v1/pair", &id, &needs) == PLATINUM_ROUTE_FOUND);
    CHECK(id == PLATINUM_ROUTE_PAIR && needs == 0);
    CHECK(platinum_route_match("POST", "/v1/login/app-password", &id, &needs) == PLATINUM_ROUTE_FOUND);
    CHECK(needs == 0);

    /* Authenticated routes do. */
    CHECK(platinum_route_match("GET", "/v1/timeline", &id, &needs) == PLATINUM_ROUTE_FOUND);
    CHECK(id == PLATINUM_ROUTE_TIMELINE && needs == 1);
    CHECK(platinum_route_match("POST", "/v1/post", &id, &needs) == PLATINUM_ROUTE_FOUND);
    CHECK(id == PLATINUM_ROUTE_POST && needs == 1);

    /* Same path, two methods: muted-words is GET and POST. */
    CHECK(platinum_route_match("GET", "/v1/muted-words", &id, &needs) == PLATINUM_ROUTE_FOUND);
    CHECK(id == PLATINUM_ROUTE_MUTED_WORDS_GET);
    CHECK(platinum_route_match("POST", "/v1/muted-words", &id, &needs) == PLATINUM_ROUTE_FOUND);
    CHECK(id == PLATINUM_ROUTE_MUTED_WORDS_SET);

    /* Wrong method on a known path is 405; unknown path is 404. */
    CHECK(platinum_route_match("DELETE", "/v1/timeline", &id, &needs) == PLATINUM_ROUTE_METHOD_NOT_ALLOWED);
    CHECK(platinum_route_match("GET", "/v1/nope", &id, &needs) == PLATINUM_ROUTE_NOT_FOUND);
    CHECK(platinum_route_match("GET", "/v1/timeline/", &id, &needs) == PLATINUM_ROUTE_NOT_FOUND);
    /* A known path under the wrong method is 405. server.ts answers 404 here; that is a deliberate divergence. */
    CHECK(platinum_route_match("get", "/health", &id, &needs) == PLATINUM_ROUTE_METHOD_NOT_ALLOWED);
    CHECK(platinum_route_match(NULL, "/health", &id, &needs) == PLATINUM_ROUTE_NOT_FOUND);

    /* Bearer parsing matches server.ts: exact "Bearer " prefix, rest is the token. */
    CHECK(platinum_route_bearer_token("Bearer abc123", tok, sizeof tok) == 0 && strcmp(tok, "abc123") == 0);
    CHECK(platinum_route_bearer_token("bearer abc123", tok, sizeof tok) == -1);
    CHECK(platinum_route_bearer_token("Bearer ", tok, sizeof tok) == -1);
    CHECK(platinum_route_bearer_token("Basic abc", tok, sizeof tok) == -1);
    CHECK(platinum_route_bearer_token(NULL, tok, sizeof tok) == -1);
    CHECK(platinum_route_bearer_token("Bearer abc", tok, 3) == -1);
    CHECK(platinum_route_bearer_token("Bearer abc", tok, 4) == 0);

    /* Names are stable and never echo input. */
    CHECK(strcmp(platinum_route_name(PLATINUM_ROUTE_TIMELINE), "timeline") == 0);
    CHECK(strcmp(platinum_route_name(PLATINUM_ROUTE_NONE), "unknown") == 0);

    if (failures) {
        fprintf(stderr, "test_routes: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_routes: %d checks passed\n", checks);
    return 0;
}
