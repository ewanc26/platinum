/* Drives /v1/notifications and /v1/notifications/seen through the real Wolfram
 * calls against a local mock PDS. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <wolfram/agent.h>

#include "account_routes.h"
#include "mock_pds.h"
#include "notifications_route.h"
#include "notifications_wolfram.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* A like on a CDN-avatar account, unread, and a read follow with no optional
 * author fields. */
#define NOTIFICATIONS_JSON                                                              \
    "{\"notifications\":["                                                             \
    "{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3k1\",\"cid\":\"bafyone\","        \
    "\"author\":{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\",\"displayName\":\"Bob\"," \
    "\"avatar\":\"https://cdn.bsky.app/img/avatar/plain/did:plc:bob/bafyav@jpeg\"},"    \
    "\"reason\":\"like\",\"indexedAt\":\"2026-10-01T10:00:00.000Z\",\"isRead\":false}," \
    "{\"uri\":\"at://did:plc:carol/app.bsky.feed.post/3k2\",\"cid\":\"bafytwo\","      \
    "\"author\":{\"did\":\"did:plc:carol\"},\"reason\":\"follow\","                      \
    "\"indexedAt\":\"2026-10-01T09:00:00.000Z\",\"isRead\":true}],"                    \
    "\"cursor\":\"n1\"}"

int
main(void)
{
    wf_mock_pds *pds = NULL;
    int port = 0;
    char service[64];
    char body[8192];
    wf_agent *agent;
    int st;

    if (wf_mock_pds_start(&pds, &port) != WF_OK) {
        fprintf(stderr, "could not start the mock PDS\n");
        return 1;
    }
    wf_mock_pds_register(pds, "app.bsky.notification.listNotifications", NOTIFICATIONS_JSON);
    wf_mock_pds_register(pds, "app.bsky.notification.updateSeen", "{}");
    snprintf(service, sizeof service, "http://127.0.0.1:%d", port);
    agent = wf_agent_new(service);
    CHECK(agent != NULL);
    if (agent == NULL) {
        wf_mock_pds_free(pds);
        return 1;
    }
    CHECK(wf_agent_set_bearer(agent, "tok", "me.test", "did:plc:me") == WF_OK);

    /* The list: both reasons, the unread flag, the avatar as an image ref, and
     * the cursor for the next page. */
    st = platinum_route_notifications(agent, platinum_notifications_fetch_live, NULL, NULL,
                                      body, sizeof body);
    CHECK(st == 200);
    CHECK(strstr(body, "\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3k1\"") != NULL);
    CHECK(strstr(body, "\"cid\":\"bafyone\"") != NULL);
    CHECK(strstr(body, "\"author\":{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\",") != NULL);
    CHECK(strstr(body, "\"displayName\":\"Bob\"") != NULL);
    CHECK(strstr(body, "\"avatar\":\"avatar_thumbnail/plain/did:plc:bob/bafyav@jpeg\"") != NULL);
    CHECK(strstr(body, "\"reason\":\"like\"") != NULL);
    CHECK(strstr(body, "\"indexedAt\":\"2026-10-01T10:00:00.000Z\"") != NULL);
    CHECK(strstr(body, "\"isRead\":false") != NULL);
    CHECK(strstr(body, "\"reason\":\"follow\"") != NULL);
    CHECK(strstr(body, "\"isRead\":true") != NULL);
    CHECK(strstr(body, "\"cursor\":\"n1\"") != NULL);

    /* Seen: the timestamp the route normalises reaches Wolfram, whose call the
     * mock answers, so the route reports 200. */
    st = platinum_route_seen(agent, platinum_notifications_seen_live,
                             "2026-10-05T00:01:00.000Z",
                             (double) time(NULL) * 1000.0, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"seenAt\":\"2026-10-05T00:01:00.000Z\"}") == 0);

    /* A time that is not a timestamp is refused before any request. */
    st = platinum_route_seen(agent, platinum_notifications_seen_live, "not-a-time",
                             (double) time(NULL) * 1000.0, body, sizeof body);
    CHECK(st == 400);
    CHECK(strstr(body, "invalid_seen_at") != NULL);

    /* A server that cannot be reached: the list is a 500, not an empty page. */
    wf_mock_pds_stop(pds);
    st = platinum_route_notifications(agent, platinum_notifications_fetch_live, NULL, NULL,
                                      body, sizeof body);
    CHECK(st == 500);
    CHECK(strstr(body, "\"error\":\"bridge_error\"") != NULL);

    wf_agent_free(agent);
    wf_mock_pds_free(pds);

    if (failures) {
        fprintf(stderr, "test_notifications_live: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_notifications_live: %d checks passed\n", checks);
    return 0;
}
