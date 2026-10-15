/* Drives /v1/profile through the real Wolfram fetch against a local mock PDS. */
#include <stdio.h>
#include <string.h>

#include <microhttpd.h>
#include <wolfram/agent.h>
#include <wolfram/xrpc.h>

#include "mock_pds.h"
#include "profile_live.h"
#include "profile_route.h"

/* A PDS that answers every request with the 400 the Bluesky PDS gives for an unknown
 * account. */
static enum MHD_Result
unknown_account(void *cls, struct MHD_Connection *conn, const char *url, const char *method,
                const char *version, const char *upload, size_t *upload_size, void **con_cls)
{
    static const char body[] = "{\"error\":\"InvalidRequest\",\"message\":\"Profile not found\"}";
    struct MHD_Response *resp;
    enum MHD_Result ok;
    (void) cls; (void) url; (void) method; (void) version; (void) upload;
    if (*upload_size != 0) {
        *upload_size = 0;
        return MHD_YES;
    }
    (void) con_cls;
    resp = MHD_create_response_from_buffer(strlen(body), (void *) body, MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(resp, "content-type", "application/json");
    ok = MHD_queue_response(conn, MHD_HTTP_BAD_REQUEST, resp);
    MHD_destroy_response(resp);
    return ok;
}

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

    /* An unknown account, as the real PDS reports it (HTTP 400, "Profile not found"): the
     * route answers 404 actor_not_found, as api.ts does. The own profile is not treated as
     * not-found: a 400 there is a failure, so 500. */
    {
        struct MHD_Daemon *ghost = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, 0, NULL, NULL,
                                                    &unknown_account, NULL, MHD_OPTION_END);
        const union MHD_DaemonInfo *info;
        char gsvc[64];
        wf_agent *gagent;

        CHECK(ghost != NULL);
        if (ghost != NULL) {
            info = MHD_get_daemon_info(ghost, MHD_DAEMON_INFO_BIND_PORT);
            snprintf(gsvc, sizeof gsvc, "http://127.0.0.1:%d", (int) info->port);
            gagent = wf_agent_new(gsvc);
            CHECK(gagent != NULL);
            if (gagent != NULL) {
                st = platinum_route_profile(gagent, platinum_profile_fetch_live, "ghost.test", body, sizeof body);
                CHECK(st == 404);
                CHECK(strcmp(body, "{\"error\":\"actor_not_found\",\"message\":\"No such account.\"}") == 0);
                CHECK(wf_agent_set_bearer(gagent, "tok", "me.test", "did:plc:me") == WF_OK);
                st = platinum_route_profile(gagent, platinum_profile_fetch_live, NULL, body, sizeof body);
                CHECK(st == 500);
                wf_agent_free(gagent);
            }
            MHD_stop_daemon(ghost);
        }
    }

    if (failures) {
        fprintf(stderr, "test_profile_live: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_profile_live: %d checks passed\n", checks);
    return 0;
}
