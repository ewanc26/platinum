/* Drives /v1/profile through the real Wolfram fetch against a local mock PDS. */
#include <stdio.h>
#include <string.h>

#include <wolfram/agent.h>
#include <wolfram/xrpc.h>

#include "mock_pds.h"
#include "profile_live.h"
#include "profile_route.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define PROFILE_JSON \
    "{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\",\"displayName\":\"Bob\"," \
    "\"description\":\"Gardener\",\"followersCount\":3,\"followsCount\":4,\"postsCount\":5," \
    "\"viewer\":{\"following\":\"at://did:plc:me/app.bsky.graph.follow/3k\",\"muted\":false}}"

int
main(void)
{
    wf_mock_pds *pds = NULL;
    int port = 0;
    char service[64];
    wf_agent *agent;
    char body[1024];
    int st;

    if (wf_mock_pds_start(&pds, &port) != WF_OK) {
        fprintf(stderr, "could not start the mock PDS\n");
        return 1;
    }
    wf_mock_pds_register(pds, "app.bsky.actor.getProfile", PROFILE_JSON);
    snprintf(service, sizeof service, "http://127.0.0.1:%d", port);
    agent = wf_agent_new(service);
    CHECK(agent != NULL);
    if (agent == NULL) {
        wf_mock_pds_free(pds);
        return 1;
    }

    /* Another account, live: 200 with the fields Wolfram sent. */
    st = platinum_route_profile(agent, platinum_profile_fetch_live, "bob.test", body, sizeof body);
    CHECK(st == 200);
    CHECK(strstr(body, "\"handle\":\"bob.test\"") != NULL);
    CHECK(strstr(body, "\"followersCount\":3") != NULL);
    CHECK(strstr(body, "\"following\":true") != NULL);
    CHECK(strstr(body, "\"followedBy\"") == NULL);

    /* Your own profile, with no actor parameter: the DID comes from the agent, and
     * the viewer fields are left out. */
    CHECK(wf_agent_set_bearer(agent, "tok", "bob.test", "did:plc:bob") == WF_OK);
    st = platinum_route_profile(agent, platinum_profile_fetch_live, NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(strstr(body, "\"following\"") == NULL);
    CHECK(strstr(body, "\"muted\"") == NULL);

    /* A server that cannot be reached: the route answers 500, not a partial profile. */
    wf_mock_pds_stop(pds);
    st = platinum_route_profile(agent, platinum_profile_fetch_live, "bob.test", body, sizeof body);
    CHECK(st == 500);
    CHECK(strstr(body, "\"error\":\"bridge_error\"") != NULL);

    wf_agent_free(agent);
    wf_mock_pds_free(pds);

    if (failures) {
        fprintf(stderr, "test_profile_live: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_profile_live: %d checks passed\n", checks);
    return 0;
}
