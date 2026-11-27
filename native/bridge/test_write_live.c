/* Like, repost, follow, mute, block, post delete and the feed and list pages through
 * the real Wolfram calls, against a local mock PDS. The mock records only the last
 * request, so a write that should not happen is shown by the last request still
 * being the viewer-state read. */
#include <stdio.h>
#include <string.h>

#include <wolfram/agent.h>

#include "collection_live.h"
#include "collection_routes.h"
#include "content_routes.h"
#include "mock_pds.h"
#include "write_live.h"
#include "write_routes.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define POST_URI "at://did:plc:alice/app.bsky.feed.post/3kpost"
#define RECORD_JSON "{\"uri\":\"at://did:plc:bob/app.bsky.feed.like/3knew\",\"cid\":\"bafynew\"}"
#define AUTHOR_JSON "{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\"}"

/* The post with no like or repost by the viewer yet. */
#define POST_FRESH_JSON                                                                  \
    "{\"posts\":[{\"uri\":\"" POST_URI "\",\"cid\":\"bafypost\",\"author\":" AUTHOR_JSON \
    ",\"record\":{\"text\":\"hi\"},\"likeCount\":2,\"repostCount\":5,\"viewer\":{}}]}"
/* The same post already liked and reposted by the viewer. */
#define POST_DONE_JSON                                                                   \
    "{\"posts\":[{\"uri\":\"" POST_URI "\",\"cid\":\"bafypost\",\"author\":" AUTHOR_JSON \
    ",\"record\":{\"text\":\"hi\"},\"likeCount\":3,\"repostCount\":6,\"viewer\":{"       \
    "\"like\":\"at://did:plc:bob/app.bsky.feed.like/3klike\","                           \
    "\"repost\":\"at://did:plc:bob/app.bsky.feed.repost/3krepost\"}}]}"

#define PROFILE_FRESH_JSON "{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\",\"viewer\":{}}"
#define PROFILE_DONE_JSON                                                                 \
    "{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\",\"viewer\":{"                   \
    "\"following\":\"at://did:plc:bob/app.bsky.graph.follow/3kfollow\","                  \
    "\"blocking\":\"at://did:plc:bob/app.bsky.graph.block/3kblock\",\"muted\":true}}"

#define FEED_URI "at://did:plc:gen/app.bsky.feed.generator/cats"
#define LIST_URI "at://did:plc:bob/app.bsky.graph.list/3klist"

/* Two posts, the second muted by the preferences below. */
#define FEED_JSON                                                                         \
    "{\"feed\":["                                                                         \
    "{\"post\":{\"uri\":\"at://did:plc:alice/app.bsky.feed.post/3k1\",\"cid\":\"bafyone\"," \
    "\"author\":" AUTHOR_JSON ","                                                         \
    "\"record\":{\"$type\":\"app.bsky.feed.post\",\"text\":\"a cat on a mat\","          \
    "\"createdAt\":\"2026-10-01T10:00:00.000Z\"},\"likeCount\":4}},"                      \
    "{\"post\":{\"uri\":\"at://did:plc:alice/app.bsky.feed.post/3k2\",\"cid\":\"bafytwo\"," \
    "\"author\":" AUTHOR_JSON ","                                                         \
    "\"record\":{\"$type\":\"app.bsky.feed.post\",\"text\":\"spoilers for the film\","    \
    "\"createdAt\":\"2026-10-01T09:00:00.000Z\"}}}],"                                    \
    "\"cursor\":\"f1\"}"
#define PREFS_JSON                                                                    \
    "{\"preferences\":[{\"$type\":\"app.bsky.actor.defs#mutedWordsPref\","            \
    "\"items\":[{\"value\":\"spoilers\",\"targets\":[\"content\"]}]}]}"
#define LIST_JSON                                                                         \
    "{\"list\":{\"uri\":\"" LIST_URI "\",\"cid\":\"bafylist\",\"name\":\"Friends\","      \
    "\"purpose\":\"app.bsky.graph.defs#curatelist\",\"creator\":{\"did\":\"did:plc:bob\"," \
    "\"handle\":\"bob.test\"},\"indexedAt\":\"2026-10-01T10:00:00.000Z\"},"                \
    "\"items\":[{\"uri\":\"at://did:plc:bob/app.bsky.graph.listitem/1\","                  \
    "\"subject\":{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\","                    \
    "\"displayName\":\"Alice\"}},"                                                         \
    "{\"uri\":\"at://did:plc:bob/app.bsky.graph.listitem/2\","                             \
    "\"subject\":{\"did\":\"did:plc:carol\",\"handle\":\"carol.test\"}}],"                 \
    "\"cursor\":\"l1\"}"

/* The mock wants every out-pointer, so both helpers ask for all three. */
static const char *
last_nsid(wf_mock_pds *pds)
{
    const char *nsid = NULL;
    const char *method = NULL;
    const char *sent = NULL;
    if (wf_mock_pds_get_last_request(pds, &nsid, &method, &sent) != WF_OK) return NULL;
    return nsid;
}

static const char *
last_body(wf_mock_pds *pds)
{
    const char *nsid = NULL;
    const char *method = NULL;
    const char *sent = NULL;
    if (wf_mock_pds_get_last_request(pds, &nsid, &method, &sent) != WF_OK) return NULL;
    return sent;
}

static int
nsid_is(wf_mock_pds *pds, const char *want)
{
    const char *nsid = last_nsid(pds);
    return nsid != NULL && strcmp(nsid, want) == 0;
}

static int
sent_has(wf_mock_pds *pds, const char *needle)
{
    const char *sent = last_body(pds);
    return sent != NULL && strstr(sent, needle) != NULL;
}

static void
test_toggle(wf_mock_pds *pds, wf_agent *agent)
{
    char body[512];
    int st;

    /* Like a post not liked yet: createRecord with the client's cid, count up one. */
    wf_mock_pds_register(pds, "app.bsky.feed.getPosts", POST_FRESH_JSON);
    st = platinum_route_toggle(agent, platinum_toggle_live, 0, POST_URI, "bafyclient", 1, 1,
                               body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"" POST_URI "\",\"on\":true,\"count\":3}") == 0);
    CHECK(nsid_is(pds, "com.atproto.repo.createRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.feed.like\""));
    CHECK(sent_has(pds, "\"cid\":\"bafyclient\""));

    /* Unliking a post not liked is a no-op: nothing is written. */
    st = platinum_route_toggle(agent, platinum_toggle_live, 0, POST_URI, "bafyclient", 1, 0,
                               body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"" POST_URI "\",\"on\":false,\"count\":2}") == 0);
    CHECK(nsid_is(pds, "app.bsky.feed.getPosts"));

    /* Repost a post not reposted yet. */
    st = platinum_route_toggle(agent, platinum_toggle_live, 1, POST_URI, "bafyclient", 1, 1,
                               body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"" POST_URI "\",\"on\":true,\"count\":6}") == 0);
    CHECK(nsid_is(pds, "com.atproto.repo.createRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.feed.repost\""));

    /* Already liked: liking again writes nothing and keeps the count. */
    wf_mock_pds_register(pds, "app.bsky.feed.getPosts", POST_DONE_JSON);
    st = platinum_route_toggle(agent, platinum_toggle_live, 0, POST_URI, "bafyclient", 1, 1,
                               body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"" POST_URI "\",\"on\":true,\"count\":3}") == 0);
    CHECK(nsid_is(pds, "app.bsky.feed.getPosts"));

    /* Unlike deletes the viewer's own like record, by its rkey. */
    st = platinum_route_toggle(agent, platinum_toggle_live, 0, POST_URI, "bafyclient", 1, 0,
                               body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"" POST_URI "\",\"on\":false,\"count\":2}") == 0);
    CHECK(nsid_is(pds, "com.atproto.repo.deleteRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.feed.like\""));
    CHECK(sent_has(pds, "\"rkey\":\"3klike\""));

    /* Un-repost deletes the repost record. */
    st = platinum_route_toggle(agent, platinum_toggle_live, 1, POST_URI, "bafyclient", 1, 0,
                               body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"" POST_URI "\",\"on\":false,\"count\":5}") == 0);
    CHECK(nsid_is(pds, "com.atproto.repo.deleteRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.feed.repost\""));
    CHECK(sent_has(pds, "\"rkey\":\"3krepost\""));

    /* A post that is gone: liking it is 404, unliking it is the state already. */
    wf_mock_pds_register(pds, "app.bsky.feed.getPosts", "{\"posts\":[]}");
    st = platinum_route_toggle(agent, platinum_toggle_live, 0, POST_URI, "bafyclient", 1, 1,
                               body, sizeof body);
    CHECK(st == 404);
    CHECK(strstr(body, "post_not_found") != NULL);
    st = platinum_route_toggle(agent, platinum_toggle_live, 0, POST_URI, "bafyclient", 1, 0,
                               body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"" POST_URI "\",\"on\":false,\"count\":0}") == 0);
}

static void
test_subject(wf_mock_pds *pds, wf_agent *agent)
{
    char body[512];
    int st;

    wf_mock_pds_register(pds, "app.bsky.actor.getProfile", PROFILE_FRESH_JSON);

    /* Follow: a follow record naming the subject. */
    st = platinum_route_subject(agent, platinum_subject_live, 0, "did:plc:alice", 1, 1,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"did\":\"did:plc:alice\",\"on\":true}") == 0);
    CHECK(nsid_is(pds, "com.atproto.repo.createRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.graph.follow\""));
    CHECK(sent_has(pds, "\"subject\":\"did:plc:alice\""));

    /* Mute: muteActor, no record. */
    st = platinum_route_subject(agent, platinum_subject_live, 1, "did:plc:alice", 1, 1,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(nsid_is(pds, "app.bsky.graph.muteActor"));
    CHECK(sent_has(pds, "\"actor\":\"did:plc:alice\""));

    /* Block: a block record naming the subject. */
    st = platinum_route_subject(agent, platinum_subject_live, 2, "did:plc:alice", 1, 1,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(nsid_is(pds, "com.atproto.repo.createRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.graph.block\""));
    CHECK(sent_has(pds, "\"subject\":\"did:plc:alice\""));

    /* Turning off what is not on writes nothing. */
    st = platinum_route_subject(agent, platinum_subject_live, 0, "did:plc:alice", 1, 0,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"did\":\"did:plc:alice\",\"on\":false}") == 0);
    CHECK(nsid_is(pds, "app.bsky.actor.getProfile"));
    st = platinum_route_subject(agent, platinum_subject_live, 1, "did:plc:alice", 1, 0,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(nsid_is(pds, "app.bsky.actor.getProfile"));

    /* Already following, muting and blocking: on again writes nothing. */
    wf_mock_pds_register(pds, "app.bsky.actor.getProfile", PROFILE_DONE_JSON);
    {
        int kind;
        for (kind = 0; kind < 3; kind++) {
            st = platinum_route_subject(agent, platinum_subject_live, kind, "did:plc:alice",
                                        1, 1, body, sizeof body);
            CHECK(st == 200);
            CHECK(nsid_is(pds, "app.bsky.actor.getProfile"));
        }
    }

    /* Off undoes each one: the viewer's own follow and block records by rkey, and
     * unmuteActor for the mute. */
    st = platinum_route_subject(agent, platinum_subject_live, 0, "did:plc:alice", 1, 0,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(nsid_is(pds, "com.atproto.repo.deleteRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.graph.follow\""));
    CHECK(sent_has(pds, "\"rkey\":\"3kfollow\""));
    st = platinum_route_subject(agent, platinum_subject_live, 1, "did:plc:alice", 1, 0,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(nsid_is(pds, "app.bsky.graph.unmuteActor"));
    st = platinum_route_subject(agent, platinum_subject_live, 2, "did:plc:alice", 1, 0,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"did\":\"did:plc:alice\",\"on\":false}") == 0);
    CHECK(nsid_is(pds, "com.atproto.repo.deleteRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.graph.block\""));
    CHECK(sent_has(pds, "\"rkey\":\"3kblock\""));
}

static void
test_delete(wf_mock_pds *pds, wf_agent *agent)
{
    char body[512];
    int st;

    /* Your own post: deleteRecord by its rkey. */
    st = platinum_route_post_delete(agent, platinum_delete_live,
                                    "at://did:plc:bob/app.bsky.feed.post/3kmine", body, sizeof body);
    CHECK(st == 200);
    CHECK(strcmp(body, "{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3kmine\",\"deleted\":true}") == 0);
    CHECK(nsid_is(pds, "com.atproto.repo.deleteRecord"));
    CHECK(sent_has(pds, "\"collection\":\"app.bsky.feed.post\""));
    CHECK(sent_has(pds, "\"rkey\":\"3kmine\""));

    /* Someone else's post is refused before any request. A read first makes the last
     * request a known one, so a delete sent anyway would show. */
    wf_mock_pds_register(pds, "app.bsky.actor.getProfile", PROFILE_FRESH_JSON);
    st = platinum_route_subject(agent, platinum_subject_live, 0, "did:plc:alice", 1, 0,
                                body, sizeof body);
    CHECK(st == 200);
    CHECK(nsid_is(pds, "app.bsky.actor.getProfile"));
    st = platinum_route_post_delete(agent, platinum_delete_live, POST_URI, body, sizeof body);
    CHECK(st == 403);
    CHECK(strstr(body, "not_your_post") != NULL);
    CHECK(nsid_is(pds, "app.bsky.actor.getProfile"));

    /* A DID that only starts with yours is not yours. */
    st = platinum_route_post_delete(agent, platinum_delete_live,
                                    "at://did:plc:bobby/app.bsky.feed.post/3kx", body, sizeof body);
    CHECK(st == 403);
}

static void
test_collections(wf_mock_pds *pds, wf_agent *agent)
{
    char body[4096];
    int st;

    /* A feed: a page of posts in the timeline shape, the muted one left out, and the
     * cursor passed on. */
    wf_mock_pds_register(pds, "app.bsky.feed.getFeed", FEED_JSON);
    wf_mock_pds_register(pds, "app.bsky.actor.getPreferences", PREFS_JSON);
    st = platinum_route_collection(agent, platinum_collection_named_live, 1, FEED_URI, "10",
                                   NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(strstr(body, "\"posts\":[") != NULL);
    CHECK(strstr(body, "a cat on a mat") != NULL);
    CHECK(strstr(body, "at://did:plc:alice/app.bsky.feed.post/3k1") != NULL);
    CHECK(strstr(body, "spoilers") == NULL);
    CHECK(strstr(body, "\"cursor\":\"f1\"") != NULL);

    /* A list: its members as actor rows, with the cursor. */
    wf_mock_pds_register(pds, "app.bsky.graph.getList", LIST_JSON);
    st = platinum_route_collection(agent, platinum_collection_named_live, 0, LIST_URI, NULL,
                                   NULL, body, sizeof body);
    CHECK(st == 200);
    CHECK(nsid_is(pds, "app.bsky.graph.getList"));
    CHECK(strcmp(body, "{\"actors\":[{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\","
                       "\"displayName\":\"Alice\"},{\"did\":\"did:plc:carol\","
                       "\"handle\":\"carol.test\"}],\"cursor\":\"l1\"}") == 0);
}

/* A PDS that serves none of the calls answers 404, which is not the 400 api.ts reads
 * as "no such account" or "no such feed": each route is a 500, not a 404. */
static void
test_unserved(void)
{
    wf_mock_pds *pds = NULL;
    int port = 0;
    char service[64];
    char body[512];
    wf_agent *agent;
    int st;

    if (wf_mock_pds_start(&pds, &port) != WF_OK) {
        CHECK(0);
        return;
    }
    snprintf(service, sizeof service, "http://127.0.0.1:%d", port);
    agent = wf_agent_new(service);
    CHECK(agent != NULL);
    if (agent != NULL) {
        CHECK(wf_agent_set_bearer(agent, "tok", "bob.test", "did:plc:bob") == WF_OK);
        st = platinum_route_subject(agent, platinum_subject_live, 0, "did:plc:alice", 1, 1,
                                    body, sizeof body);
        CHECK(st == 500);
        CHECK(strstr(body, "bridge_error") != NULL);
        st = platinum_route_toggle(agent, platinum_toggle_live, 0, POST_URI, "bafy", 1, 1,
                                   body, sizeof body);
        CHECK(st == 500);
        st = platinum_route_collection(agent, platinum_collection_named_live, 1, FEED_URI,
                                       NULL, NULL, body, sizeof body);
        CHECK(st == 500);
        st = platinum_route_collection(agent, platinum_collection_named_live, 0, LIST_URI,
                                       NULL, NULL, body, sizeof body);
        CHECK(st == 500);
        st = platinum_route_post_delete(agent, platinum_delete_live,
                                        "at://did:plc:bob/app.bsky.feed.post/3kmine",
                                        body, sizeof body);
        CHECK(st == 500);

        /* A write that fails after the read is a 500 too. */
        wf_mock_pds_register(pds, "app.bsky.feed.getPosts", POST_FRESH_JSON);
        st = platinum_route_toggle(agent, platinum_toggle_live, 0, POST_URI, "bafy", 1, 1,
                                   body, sizeof body);
        CHECK(st == 500);
        wf_agent_free(agent);
    }
    wf_mock_pds_free(pds);
}

int
main(void)
{
    wf_mock_pds *pds = NULL;
    int port = 0;
    char service[64];
    wf_agent *agent;

    if (wf_mock_pds_start(&pds, &port) != WF_OK) {
        fprintf(stderr, "could not start the mock PDS\n");
        return 1;
    }
    wf_mock_pds_register(pds, "com.atproto.repo.createRecord", RECORD_JSON);
    wf_mock_pds_register(pds, "com.atproto.repo.deleteRecord", "{}");
    wf_mock_pds_register(pds, "app.bsky.graph.muteActor", "{}");
    wf_mock_pds_register(pds, "app.bsky.graph.unmuteActor", "{}");
    snprintf(service, sizeof service, "http://127.0.0.1:%d", port);
    agent = wf_agent_new(service);
    CHECK(agent != NULL);
    if (agent == NULL) {
        wf_mock_pds_free(pds);
        return 1;
    }
    CHECK(wf_agent_set_bearer(agent, "tok", "bob.test", "did:plc:bob") == WF_OK);

    test_toggle(pds, agent);
    test_subject(pds, agent);
    test_delete(pds, agent);
    test_collections(pds, agent);

    wf_agent_free(agent);
    wf_mock_pds_free(pds);

    test_unserved();

    if (failures) {
        fprintf(stderr, "test_write_live: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_write_live: %d checks passed\n", checks);
    return 0;
}
