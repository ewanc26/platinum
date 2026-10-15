/* POST /v1/post through the real Wolfram publish, against a local mock PDS. */
#include <stdio.h>
#include <string.h>

#include <wolfram/agent.h>

#include "mock_pds.h"
#include "post_routes.h"
#include "publish_live.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define RECORD_JSON "{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3kpost\",\"cid\":\"bafyreiabc\"}"

int
main(void)
{
    wf_mock_pds *pds = NULL;
    int port = 0;
    char service[64];
    wf_agent *agent;
    platinum_post_request req;
    char body[512];
    int st;

    if (wf_mock_pds_start(&pds, &port) != WF_OK) {
        fprintf(stderr, "could not start the mock PDS\n");
        return 1;
    }
    wf_mock_pds_register(pds, "com.atproto.repo.createRecord", RECORD_JSON);
    wf_mock_pds_register(pds, "com.atproto.repo.putRecord", RECORD_JSON);
    snprintf(service, sizeof service, "http://127.0.0.1:%d", port);
    agent = wf_agent_new(service);
    CHECK(agent != NULL);
    if (agent == NULL) {
        wf_mock_pds_free(pds);
        return 1;
    }
    CHECK(wf_agent_set_bearer(agent, "tok", "bob.test", "did:plc:bob") == WF_OK);

    /* A plain post: 200, with the uri and cid the PDS returned. */
    memset(&req, 0, sizeof req);
    req.text = "hello from the native bridge";
    st = platinum_route_post(agent, platinum_publish_live, &req, body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3kpost\",\"cid\":\"bafyreiabc\"}") == 0);

    /* The gate "nobody" is applied as a threadgate: no replyGateApplied field. */
    req.has_gate = 1;
    req.gate = "nobody";
    st = platinum_route_post(agent, platinum_publish_live, &req, body, sizeof body);
    CHECK(st == 200);
    CHECK(strstr(body, "replyGateApplied") == NULL);

    /* A gate Wolfram cannot express: posted, and reported as not applied. */
    req.gate = "following";
    st = platinum_route_post(agent, platinum_publish_live, &req, body, sizeof body);
    CHECK(st == 200);
    CHECK(strstr(body, "\"replyGateApplied\":false") != NULL);

    /* A reply is not wired, so it is refused as a server error, not posted with the wrong root. */
    memset(&req, 0, sizeof req);
    req.text = "a reply";
    req.has_reply = 1;
    req.reply_is_object = 1;
    req.reply.uri = "at://did:plc:bob/app.bsky.feed.post/3kparent";
    req.reply.cid = "bafyparent";
    st = platinum_route_post(agent, platinum_publish_live, &req, body, sizeof body);
    CHECK(st == 500);

    /* A quote goes through wf_agent_quote. */
    memset(&req, 0, sizeof req);
    req.text = "quoting";
    req.has_quote = 1;
    req.quote_is_object = 1;
    req.quote.uri = "at://did:plc:bob/app.bsky.feed.post/3kq";
    req.quote.cid = "bafyq";
    st = platinum_route_post(agent, platinum_publish_live, &req, body, sizeof body);
    CHECK(st == 200);

    /* With the PDS stopped the post fails, and the route answers 500 with no body. */
    wf_mock_pds_stop(pds);
    memset(&req, 0, sizeof req);
    req.text = "offline";
    st = platinum_route_post(agent, platinum_publish_live, &req, body, sizeof body);
    CHECK(st == 500);
    CHECK(strstr(body, "bridge_error") != NULL);

    wf_agent_free(agent);
    wf_mock_pds_free(pds);

    if (failures) {
        fprintf(stderr, "test_publish_live: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_publish_live: %d checks passed\n", checks);
    return 0;
}
