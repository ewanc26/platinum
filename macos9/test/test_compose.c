/*
 * test_compose.c -- host-side tests for what a compose window is replying to or
 * quoting, and who may reply, and for the native publish path: the post record
 * body, its facets, the reply and quote shapes and the threadgate. Links the
 * real compose_model.c, the bridge client's gate names, atproto_post.c and the
 * native session layer over the Wolfram stub. Not Classic Mac OS 9 validation
 * and not a live PDS: the window is compose.c and the transport is a stub.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atproto_client.h"
#include "atproto_post.h"
#include "bridge_client.h"
#include "compose.h"
#include "wolfram_stub.h"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static platinum_compose compose;

static void test_quote_and_gate(void)
{
    memset(&compose, 0, sizeof(compose));
    check(platinum_compose_set_quote(&compose, "at://did:plc:a/app.bsky.feed.post/3k", "cq", "bob.test") == 1,
          "a quote is set");
    check(strcmp(compose.caption, "Quoting @bob.test") == 0, "the caption names who");
    check(strcmp(compose.quote_uri, "at://did:plc:a/app.bsky.feed.post/3k") == 0 &&
              strcmp(compose.quote_cid, "cq") == 0 && compose.reply_uri[0] == '\0',
          "the quote carries its exact identifiers and no reply");

    check(platinum_compose_cycle_gate(&compose) == 1 && compose.reply_gate == 1,
          "a quote can set a reply gate");
    compose.reply_gate = PLATINUM_REPLY_GATE_COUNT - 1;
    platinum_compose_cycle_gate(&compose);
    check(compose.reply_gate == 0, "the gate wraps back to everyone");

    compose.reply_gate = 3;
    check(platinum_compose_set_reply(&compose, "at://r", "cr", "@amy.test") == 1,
          "a reply replaces the quote");
    check(compose.quote_uri[0] == '\0' && compose.quote_cid[0] == '\0' && compose.reply_gate == 0,
          "and drops the quote and the gate");
    check(strcmp(compose.caption, "Replying to @amy.test") == 0, "the @ is not doubled");
    check(platinum_compose_cycle_gate(&compose) == 0 && compose.reply_gate == 0,
          "a reply has no gate to change");

    check(platinum_compose_set_quote(&compose, "at://q", "cq", NULL) == 1 &&
              strcmp(compose.caption, "Quoting a post") == 0 && compose.reply_uri[0] == '\0',
          "quoting replaces a reply, and a missing handle gets a plain caption");
}

static void test_refusals(void)
{
    static char big[600];

    memset(&compose, 0, sizeof(compose));
    check(platinum_compose_set_quote(&compose, "", "c", "x") == 0 &&
              platinum_compose_set_quote(&compose, "at://q", NULL, "x") == 0,
          "an empty uri or missing cid is refused");
    check(compose.quote_uri[0] == '\0' && compose.caption[0] == '\0', "and leaves a plain post");
    memset(big, 'a', 550);
    big[550] = '\0';
    check(platinum_compose_set_quote(&compose, big, "c", "x") == 0 &&
              platinum_compose_set_reply(&compose, big, "c", "x") == 0,
          "an identifier that does not fit is refused");
    check(platinum_compose_set_quote(NULL, "a", "b", "c") == 0 && platinum_compose_cycle_gate(NULL) == 0,
          "NULL is safe");
}


/* ------------------------------------------------------------------ */
/* Native publish                                                      */
/* ------------------------------------------------------------------ */

#define MAX_CALLS 8

/* What each scripted transport call asked for, and what it answers. */
static int call_count;
static char call_nsid[MAX_CALLS][128];
static char call_body[MAX_CALLS][4096];
static char call_param[MAX_CALLS][512];
static int reply_status[MAX_CALLS];
static const char *reply_body[MAX_CALLS];

static void script(int call, int method, const char *nsid, const char *body)
{
    int index = call - 1;

    (void)method;
    if (index < 0 || index >= MAX_CALLS) {
        fake_status = WF_ERR_STATE;
        fake_http_status = 500;
        fake_body = "{}";
        return;
    }
    call_count = call;
    strncpy(call_nsid[index], nsid != NULL ? nsid : "",
            sizeof(call_nsid[index]) - 1);
    strncpy(call_body[index], body != NULL ? body : "",
            sizeof(call_body[index]) - 1);
    strncpy(call_param[index], last_param_count > 0 ? last_param_values[0] : "",
            sizeof(call_param[index]) - 1);
    fake_status = reply_status[index];
    fake_http_status = reply_status[index] == WF_OK ? 200 : 400;
    fake_body = reply_body[index];
}

static void reset_script(void)
{
    int index;

    call_count = 0;
    fake_call_count = 0;
    fake_script = script;
    fake_body_len = 0;
    memset(call_nsid, 0, sizeof(call_nsid));
    memset(call_body, 0, sizeof(call_body));
    memset(call_param, 0, sizeof(call_param));
    for (index = 0; index < MAX_CALLS; index++) {
        reply_status[index] = WF_OK;
        reply_body[index] = "{}";
    }
}

static platinum_atproto_client *native_client(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;

    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL)
        return NULL;
    memset(&session, 0, sizeof(session));
    strcpy(session.access_jwt, "access");
    strcpy(session.refresh_jwt, "refresh");
    strcpy(session.did, "did:plc:me");
    strcpy(session.handle, "me.test");
    strcpy(session.pds_url, "https://pds.example");
    if (platinum_atproto_client_set_session(client, &session) != WF_OK) {
        platinum_atproto_client_free(client);
        return NULL;
    }
    return client;
}

static int test_resolve(const char *handle, char *did, size_t cap,
                        void *userdata)
{
    (void)userdata;
    if (strcmp(handle, "amy.test") != 0 || cap < 16)
        return 0;
    strcpy(did, "did:plc:amy");
    return 1;
}

static void test_facets(void)
{
    char *facets;

    facets = platinum_atproto_post_facets("plain words only", test_resolve,
                                          NULL);
    check(facets != NULL && facets[0] == '\0', "plain text has no facets");
    free(facets);

    /* "caf\xc3\xa9 " is six bytes but five characters: offsets are bytes. */
    facets = platinum_atproto_post_facets(
        "caf\xc3\xa9 https://example.com/a.", NULL, NULL);
    check(facets != NULL &&
              strcmp(facets,
                     "[{\"index\":{\"byteStart\":6,\"byteEnd\":27},"
                     "\"features\":[{\"$type\":\"app.bsky.richtext.facet#link\","
                     "\"uri\":\"https://example.com/a\"}]}]") == 0,
          "a link after non-ASCII text has byte offsets and no trailing dot");
    free(facets);

    facets = platinum_atproto_post_facets("hi @Amy.test and @nobody.test #mac9",
                                          test_resolve, NULL);
    check(facets != NULL &&
              strstr(facets,
                     "{\"index\":{\"byteStart\":3,\"byteEnd\":12},"
                     "\"features\":[{\"$type\":\"app.bsky.richtext.facet#mention\","
                     "\"did\":\"did:plc:amy\"}]}") != NULL,
          "a mention that resolves carries its DID");
    check(facets != NULL && strstr(facets, "nobody") == NULL,
          "a mention that does not resolve is left as plain text");
    check(facets != NULL &&
              strstr(facets,
                     "{\"index\":{\"byteStart\":30,\"byteEnd\":35},"
                     "\"features\":[{\"$type\":\"app.bsky.richtext.facet#tag\","
                     "\"tag\":\"mac9\"}]}]") != NULL,
          "a tag carries its text without the #");
    free(facets);

    facets = platinum_atproto_post_facets("#123 a#b x@y.test", test_resolve,
                                          NULL);
    check(facets != NULL && facets[0] == '\0',
          "digits-only tags and mid-word marks are not facets");
    free(facets);
}

static void test_record_body(void)
{
    char *body;
    char gate[1024];

    body = platinum_atproto_post_record_body(
        "did:plc:me", "say \"hi\"\n", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        "2026-11-27T12:00:00Z");
    check(body != NULL &&
              strcmp(body,
                     "{\"repo\":\"did:plc:me\",\"collection\":\"app.bsky.feed.post\","
                     "\"record\":{\"$type\":\"app.bsky.feed.post\","
                     "\"text\":\"say \\\"hi\\\"\\n\","
                     "\"createdAt\":\"2026-11-27T12:00:00Z\"}}") == 0,
          "a plain post record is exact, with its text escaped");
    free(body);

    body = platinum_atproto_post_record_body(
        "did:plc:me", "re", NULL, "at://did:plc:r/app.bsky.feed.post/root",
        "croot", "at://did:plc:p/app.bsky.feed.post/par", "cpar", NULL, NULL,
        "2026-11-27T12:00:00Z");
    check(body != NULL &&
              strstr(body,
                     "\"reply\":{\"root\":{\"uri\":\"at://did:plc:r/app.bsky.feed.post/root\","
                     "\"cid\":\"croot\"},\"parent\":{\"uri\":"
                     "\"at://did:plc:p/app.bsky.feed.post/par\",\"cid\":\"cpar\"}}") != NULL,
          "a reply names its root and parent");
    free(body);

    body = platinum_atproto_post_record_body(
        "did:plc:me", "q", "[]", NULL, NULL, NULL, NULL,
        "at://did:plc:q/app.bsky.feed.post/qq", "cq", "2026-11-27T12:00:00Z");
    check(body != NULL &&
              strstr(body,
                     "\"facets\":[],\"embed\":{\"$type\":\"app.bsky.embed.record\","
                     "\"record\":{\"uri\":\"at://did:plc:q/app.bsky.feed.post/qq\","
                     "\"cid\":\"cq\"}}") != NULL,
          "a quote is a record embed");
    free(body);

    check(platinum_atproto_post_record_body(
              "did:plc:me", "x", NULL, "at://did:plc:r/app.bsky.feed.post/r",
              "c", NULL, NULL, NULL, NULL, "t") == NULL,
          "half a reply reference is refused");
    check(platinum_atproto_post_record_body(
              "did:plc:me", "x", NULL, NULL, NULL, NULL, NULL,
              "at://did:plc:q/app.bsky.feed.post/q", "c-q!", "t") == NULL,
          "a malformed cid is refused");

    check(platinum_atproto_threadgate_body(
              "did:plc:me", "at://did:plc:me/app.bsky.feed.post/3abc",
              "nobody", "2026-11-27T12:00:00Z", gate, sizeof(gate)) == WF_OK &&
              strcmp(gate,
                     "{\"repo\":\"did:plc:me\",\"collection\":\"app.bsky.feed.threadgate\","
                     "\"rkey\":\"3abc\",\"record\":{\"$type\":\"app.bsky.feed.threadgate\","
                     "\"post\":\"at://did:plc:me/app.bsky.feed.post/3abc\","
                     "\"allow\":[],\"createdAt\":\"2026-11-27T12:00:00Z\"}}") == 0,
          "a nobody gate uses the post's rkey and an empty allow list");
    check(platinum_atproto_threadgate_body(
              "did:plc:me", "at://did:plc:me/app.bsky.feed.post/3abc",
              "followers", "t", gate, sizeof(gate)) == WF_OK &&
              strstr(gate, "\"allow\":[{\"$type\":"
                           "\"app.bsky.feed.threadgate#followerRule\"}]") != NULL,
          "a followers gate allows followers");
    check(platinum_atproto_threadgate_body(
              "did:plc:me", "at://did:plc:other/app.bsky.feed.post/3abc",
              "nobody", "t", gate, sizeof(gate)) == WF_ERR_INVALID_ARG &&
              platinum_atproto_threadgate_body(
                  "did:plc:me", "at://did:plc:me/app.bsky.feed.post/3abc",
                  "everyone", "t", gate, sizeof(gate)) == WF_ERR_INVALID_ARG,
          "a gate on another repo's post, or an everyone gate, is refused");
}

static void test_publish(void)
{
    platinum_atproto_client *client;
    int gate_failed;
    wf_status status;

    client = native_client();
    check(client != NULL, "a native client is ready");
    if (client == NULL)
        return;

    /* A plain post with a mention and a gate: resolve, post, gate. */
    reset_script();
    reply_body[0] = "{\"did\":\"did:plc:amy\"}";
    reply_body[1] = "{\"uri\":\"at://did:plc:me/app.bsky.feed.post/3new\","
                    "\"cid\":\"cnew\"}";
    status = platinum_atproto_publish(client, "hello @amy.test", NULL, NULL,
                                      NULL, NULL, "mentioned", &gate_failed);
    check(status == WF_OK && gate_failed == 0 && call_count == 3,
          "a gated post resolves, posts and gates in three calls");
    check(strcmp(call_nsid[0], "com.atproto.identity.resolveHandle") == 0 &&
              strcmp(call_param[0], "amy.test") == 0,
          "the mention is resolved through the PDS");
    check(strcmp(call_nsid[1], "com.atproto.repo.createRecord") == 0 &&
              strstr(call_body[1], "\"collection\":\"app.bsky.feed.post\"") != NULL &&
              strstr(call_body[1], "\"$type\":\"app.bsky.feed.post\",\"text\":\"hello @amy.test\"") != NULL &&
              strstr(call_body[1], "\"did\":\"did:plc:amy\"") != NULL,
          "the post record carries its text and mention facet");
    check(strstr(call_body[1], "\"createdAt\":\"") != NULL &&
              strstr(call_body[1], "Z\"}}") != NULL,
          "the post record has a UTC timestamp");
    check(strcmp(call_nsid[2], "com.atproto.repo.createRecord") == 0 &&
              strstr(call_body[2], "\"collection\":\"app.bsky.feed.threadgate\","
                                   "\"rkey\":\"3new\"") != NULL &&
              strstr(call_body[2], "\"post\":\"at://did:plc:me/app.bsky.feed.post/3new\"") != NULL &&
              strstr(call_body[2], "#mentionRule") != NULL,
          "the threadgate uses the new post's own rkey");

    /* Everyone is no gate: one call. */
    reset_script();
    reply_body[0] = "{\"uri\":\"at://did:plc:me/app.bsky.feed.post/3b\","
                    "\"cid\":\"cb\"}";
    status = platinum_atproto_publish(client, "hi", NULL, NULL, NULL, NULL,
                                      "everyone", &gate_failed);
    check(status == WF_OK && call_count == 1 && gate_failed == 0,
          "an everyone gate writes only the post");

    /* The gate fails: the post stands and says so. */
    reset_script();
    reply_body[0] = "{\"uri\":\"at://did:plc:me/app.bsky.feed.post/3c\","
                    "\"cid\":\"cc\"}";
    reply_status[1] = WF_ERR_HTTP;
    reply_body[1] = "{\"error\":\"InvalidRequest\"}";
    status = platinum_atproto_publish(client, "hi", NULL, NULL, NULL, NULL,
                                      "nobody", &gate_failed);
    check(status == WF_OK && gate_failed == 1 && call_count == 2,
          "a failed gate leaves the post published and reports it");

    /* A post URI from another repo is never used as the gate's rkey. */
    reset_script();
    reply_body[0] = "{\"uri\":\"at://did:plc:other/app.bsky.feed.post/3d\","
                    "\"cid\":\"cd\"}";
    status = platinum_atproto_publish(client, "hi", NULL, NULL, NULL, NULL,
                                      "nobody", &gate_failed);
    check(status == WF_OK && gate_failed == 1 && call_count == 1,
          "a createRecord URI outside the repo gets no gate");

    /* A reply to a reply: the root comes from the parent's own record. */
    reset_script();
    reply_body[0] =
        "{\"posts\":[{\"uri\":\"at://did:plc:p/app.bsky.feed.post/par\","
        "\"cid\":\"cpar\",\"record\":{\"text\":\"x\",\"reply\":{"
        "\"root\":{\"uri\":\"at://did:plc:r/app.bsky.feed.post/root\",\"cid\":\"croot\"},"
        "\"parent\":{\"uri\":\"at://did:plc:r/app.bsky.feed.post/root\",\"cid\":\"croot\"}}}}]}";
    reply_body[1] = "{\"uri\":\"at://did:plc:me/app.bsky.feed.post/3e\","
                    "\"cid\":\"ce\"}";
    status = platinum_atproto_publish(
        client, "yes", "at://did:plc:p/app.bsky.feed.post/par", "cstale", NULL,
        NULL, NULL, &gate_failed);
    check(status == WF_OK && call_count == 2 &&
              strcmp(call_nsid[0], "app.bsky.feed.getPosts") == 0 &&
              strcmp(call_param[0], "at://did:plc:p/app.bsky.feed.post/par") == 0,
          "a reply reads its parent back first");
    check(strstr(call_body[1],
                 "\"reply\":{\"root\":{\"uri\":\"at://did:plc:r/app.bsky.feed.post/root\","
                 "\"cid\":\"croot\"},\"parent\":{\"uri\":"
                 "\"at://did:plc:p/app.bsky.feed.post/par\",\"cid\":\"cpar\"}}") != NULL,
          "a reply to a reply keeps the thread root and the AppView's cid");

    /* A reply to a top-level post: the parent is the root. */
    reset_script();
    reply_body[0] = "{\"posts\":[{\"uri\":\"at://did:plc:p/app.bsky.feed.post/top\","
                    "\"cid\":\"ctop\",\"record\":{\"text\":\"x\"}}]}";
    reply_body[1] = "{\"uri\":\"at://did:plc:me/app.bsky.feed.post/3f\","
                    "\"cid\":\"cf\"}";
    status = platinum_atproto_publish(
        client, "yes", "at://did:plc:p/app.bsky.feed.post/top", "ctop", NULL,
        NULL, NULL, &gate_failed);
    check(status == WF_OK &&
              strstr(call_body[1],
                     "\"reply\":{\"root\":{\"uri\":\"at://did:plc:p/app.bsky.feed.post/top\","
                     "\"cid\":\"ctop\"},\"parent\":{\"uri\":"
                     "\"at://did:plc:p/app.bsky.feed.post/top\",\"cid\":\"ctop\"}}") != NULL,
          "a reply to a top-level post names it as root and parent");

    /* A parent that is gone writes nothing. */
    reset_script();
    reply_body[0] = "{\"posts\":[]}";
    status = platinum_atproto_publish(
        client, "yes", "at://did:plc:p/app.bsky.feed.post/gone", "cg", NULL,
        NULL, NULL, &gate_failed);
    check(status == WF_ERR_NOT_FOUND && call_count == 1,
          "a missing parent sends no createRecord");

    /* A parent naming a root that cannot be read writes nothing. */
    reset_script();
    reply_body[0] = "{\"posts\":[{\"uri\":\"at://did:plc:p/app.bsky.feed.post/par\","
                    "\"cid\":\"cpar\",\"record\":{\"reply\":{\"root\":{\"uri\":1}}}}]}";
    status = platinum_atproto_publish(
        client, "yes", "at://did:plc:p/app.bsky.feed.post/par", "cpar", NULL,
        NULL, NULL, &gate_failed);
    check(status == WF_ERR_PARSE && call_count == 1,
          "an unreadable thread root sends no createRecord");

    /* A quote takes the AppView's cid, and may carry a gate. */
    reset_script();
    reply_body[0] = "{\"posts\":[{\"uri\":\"at://did:plc:q/app.bsky.feed.post/qq\","
                    "\"cid\":\"cfresh\",\"record\":{\"text\":\"q\"}}]}";
    reply_body[1] = "{\"uri\":\"at://did:plc:me/app.bsky.feed.post/3g\","
                    "\"cid\":\"cg\"}";
    status = platinum_atproto_publish(
        client, "look", NULL, NULL, "at://did:plc:q/app.bsky.feed.post/qq",
        "cstale", "following", &gate_failed);
    check(status == WF_OK && call_count == 3 && gate_failed == 0 &&
              strstr(call_body[1],
                     "\"embed\":{\"$type\":\"app.bsky.embed.record\","
                     "\"record\":{\"uri\":\"at://did:plc:q/app.bsky.feed.post/qq\","
                     "\"cid\":\"cfresh\"}}") != NULL &&
              strstr(call_body[1], "\"reply\"") == NULL &&
              strstr(call_body[2], "#followingRule") != NULL,
          "a quote embeds the AppView's cid and keeps its gate");

    /* Refusals before any request. */
    reset_script();
    check(platinum_atproto_publish(client, "x",
                                   "at://did:plc:p/app.bsky.feed.post/par",
                                   "cpar", NULL, NULL, "nobody",
                                   &gate_failed) == WF_ERR_INVALID_ARG &&
              platinum_atproto_publish(client, "x", NULL, NULL, NULL, NULL,
                                       "somebody", &gate_failed) ==
                  WF_ERR_INVALID_ARG &&
              platinum_atproto_publish(client, "x",
                                       "at://did:plc:p/app.bsky.feed.post/par",
                                       "", NULL, NULL, NULL,
                                       &gate_failed) == WF_ERR_INVALID_ARG &&
              call_count == 0,
          "a gate on a reply, an unknown gate and half a reply send nothing");

    /* A failed post is reported and not retried. */
    reset_script();
    reply_status[0] = WF_ERR_NETWORK;
    reply_body[0] = NULL;
    status = platinum_atproto_publish(client, "x", NULL, NULL, NULL, NULL,
                                      NULL, &gate_failed);
    check(status == WF_ERR_NETWORK && call_count == 1,
          "a failed createRecord is reported once");

    fake_script = NULL;
    platinum_atproto_client_free(client);
}

int main(void)
{
    test_quote_and_gate();
    test_refusals();
    test_facets();
    test_record_body();
    test_publish();
    if (failures != 0) {
        printf("test_compose: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_compose: all %d checks passed\n", checks);
    return 0;
}
