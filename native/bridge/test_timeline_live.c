/* Drives /v1/timeline through the real Wolfram fetch against a local mock PDS. */
#include <stdio.h>
#include <string.h>

#include <microhttpd.h>
#include <wolfram/agent.h>
#include <wolfram/xrpc.h>

#include "mock_pds.h"
#include "timeline_route.h"
#include "timeline_wolfram.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* Two posts, one of them muted, so the page comes back short. The first has an
 * images embed, an avatar and the viewer's like; the second has a link card. */
#define TIMELINE_JSON                                                                     \
    "{\"feed\":["                                                                         \
    "{\"post\":{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3k1\",\"cid\":\"bafyone\"," \
    "\"author\":{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\",\"displayName\":\"Bob\"," \
    "\"avatar\":\"https://cdn.bsky.app/img/avatar/plain/did:plc:bob/bafyav@jpeg\"},"      \
    "\"record\":{\"$type\":\"app.bsky.feed.post\",\"text\":\"a photo of the garden\","    \
    "\"createdAt\":\"2026-10-01T10:00:00.000Z\"},"                                       \
    "\"embed\":{\"$type\":\"app.bsky.embed.images#view\",\"images\":["                    \
    "{\"thumb\":\"https://cdn.bsky.app/img/feed_thumbnail/plain/did:plc:bob/bafyimg@jpeg\"," \
    "\"fullsize\":\"https://cdn.bsky.app/img/feed_fullsize/plain/did:plc:bob/bafyimg@jpeg\"," \
    "\"alt\":\"a cat in the sun\",\"aspectRatio\":{\"width\":4,\"height\":3}}]},"         \
    "\"likeCount\":7,\"repostCount\":3,\"replyCount\":1,\"quoteCount\":2,"                \
    "\"viewer\":{\"like\":\"at://did:plc:me/app.bsky.feed.like/1\"}}},"                  \
    "{\"post\":{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3k2\",\"cid\":\"bafytwo\"," \
    "\"author\":{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\"},"                      \
    "\"record\":{\"$type\":\"app.bsky.feed.post\",\"text\":\"spoilers for the film\","    \
    "\"createdAt\":\"2026-10-01T09:00:00.000Z\"}}},"                                     \
    "{\"post\":{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3k3\",\"cid\":\"bafythree\"," \
    "\"author\":{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\"},"                      \
    "\"record\":{\"$type\":\"app.bsky.feed.post\",\"text\":\"worth reading\","           \
    "\"createdAt\":\"2026-10-01T08:00:00.000Z\"},"                                       \
    "\"embed\":{\"$type\":\"app.bsky.embed.external#view\",\"external\":{"               \
    "\"uri\":\"https://example.com/story\",\"title\":\"A story\","                       \
    "\"description\":\"About things\"}}}}],"                                             \
    "\"cursor\":\"c1\"}"

#define PREFS_JSON                                                                    \
    "{\"preferences\":[{\"$type\":\"app.bsky.actor.defs#mutedWordsPref\","            \
    "\"items\":[{\"value\":\"spoilers\",\"targets\":[\"content\"]}]}]}"

int
main(void)
{
    wf_mock_pds *pds = NULL;
    int port = 0;
    char service[64];
    wf_agent *agent;
    char body[8192];
    int st;

    if (wf_mock_pds_start(&pds, &port) != WF_OK) {
        fprintf(stderr, "could not start the mock PDS\n");
        return 1;
    }
    wf_mock_pds_register(pds, "app.bsky.feed.getTimeline", TIMELINE_JSON);
    wf_mock_pds_register(pds, "app.bsky.actor.getPreferences", PREFS_JSON);
    snprintf(service, sizeof service, "http://127.0.0.1:%d", port);
    agent = wf_agent_new(service);
    CHECK(agent != NULL);
    if (agent == NULL) {
        wf_mock_pds_free(pds);
        return 1;
    }
    CHECK(wf_agent_set_bearer(agent, "tok", "me.test", "did:plc:me") == WF_OK);

    /* One page: the muted post is gone, the images and card are refs the image
     * route can serve, and the cursor moves on. */
    st = platinum_route_timeline(agent, platinum_timeline_fetch_live, NULL, "abc",
                                 body, sizeof body);
    CHECK(st == 200);
    CHECK(strstr(body, "\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3k1\"") != NULL);
    CHECK(strstr(body, "\"text\":\"a photo of the garden\"") != NULL);
    CHECK(strstr(body, "\"likeCount\":7") != NULL);
    CHECK(strstr(body, "\"liked\":true") != NULL);
    CHECK(strstr(body, "\"reposted\":false") != NULL);
    CHECK(strstr(body, "\"avatar\":\"avatar_thumbnail/plain/did:plc:bob/bafyav@jpeg\"") != NULL);
    CHECK(strstr(body, "\"ref\":\"feed_thumbnail/plain/did:plc:bob/bafyimg@jpeg\"") != NULL);
    CHECK(strstr(body, "\"alt\":\"a cat in the sun\"") != NULL);
    CHECK(strstr(body, "\"card\":{\"uri\":\"https://example.com/story\",\"title\":\"A story\",\"domain\":\"example.com\"}") != NULL);
    CHECK(strstr(body, "\"cursor\":\"c1\"") != NULL);
    CHECK(strstr(body, "spoilers") == NULL);
    CHECK(strstr(body, "3k2") == NULL);
    CHECK(strstr(body, "3k3") != NULL);

    /* A server that cannot be reached: the route answers 500, not a partial page. */
    wf_mock_pds_stop(pds);
    st = platinum_route_timeline(agent, platinum_timeline_fetch_live, NULL, NULL,
                                 body, sizeof body);
    CHECK(st == 500);
    CHECK(strstr(body, "\"error\":\"bridge_error\"") != NULL);

    wf_agent_free(agent);
    wf_mock_pds_free(pds);

    if (failures) {
        fprintf(stderr, "test_timeline_live: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_timeline_live: %d checks passed\n", checks);
    return 0;
}
