/*
 * test_bridge_client.c -- host-side tests for the Mac OS 9 bridge client.
 *
 * The client is compiled into the Wolfram wolfram-macos9-transport target and
 * cannot be linked on a development host without Open Transport, so the
 * Wolfram entry points it calls are stubbed here. That makes the pairing path
 * -- the one place where the client accepts a token from the network and
 * stores it -- testable against malformed and hostile responses, which is
 * exactly the code that must not be trusted to a hand-written parser.
 *
 * The stubs record what the client asked for, so the tests also assert the
 * request the client builds, not only how it reads the reply.
 *
 * This is a logic and dialect check on a modern compiler. It is not Classic
 * Mac OS 9 hardware validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge_client.h"
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

static void check_str(const char *got, const char *want, const char *what)
{
    checks++;
    if (got == NULL || strcmp(got, want) != 0) {
        failures++;
        printf("FAIL: %s (got \"%s\", want \"%s\")\n", what,
               got ? got : "(null)", want);
    }
}

static void check_status(wf_status got, wf_status want, const char *what)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s (got %d, want %d)\n", what, (int)got, (int)want);
    }
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

static void reset_fake(long http_status, const char *body, int status)
{
    fake_http_status = http_status;
    fake_body = body;
    fake_status = status;
    last_method = 0;
    last_url[0] = '\0';
    last_body[0] = '\0';
}

static const char *ok_pair =
    "{\"protocol\":1,\"token\":\"TOKENVALUE\",\"did\":\"did:plc:abc123\","
    "\"installationId\":\"11111111-2222-3333-4444-555555555555\"}";

static void test_pair_success(void)
{
    platinum_bridge_client *client;
    platinum_bridge_pairing pairing;
    wf_status status;

    client = platinum_bridge_client_new("https://bridge.example");
    check(client != NULL, "client created");
    if (client == NULL)
        return;

    reset_fake(200, ok_pair, WF_OK);
    status = platinum_bridge_pair(client, "K7Q2M9", &pairing);

    check_status(status, WF_OK, "pair succeeds");
    check_str(last_url, "https://bridge.example/v1/pair", "pair URL");
    check_str(last_body, "{\"code\":\"K7Q2M9\"}", "pair request body");
    check(pairing.protocol == 1, "pair protocol version");
    check_str(pairing.token, "TOKENVALUE", "pair token");
    check_str(pairing.did, "did:plc:abc123", "pair did");
    check_str(pairing.installation_id, "11111111-2222-3333-4444-555555555555",
              "pair installation id");

    platinum_bridge_pairing_free(&pairing);
    check(pairing.token == NULL && pairing.did == NULL,
          "pairing_free clears the struct");

    platinum_bridge_client_free(client);
}

static void test_pair_escapes_code(void)
{
    platinum_bridge_client *client;
    platinum_bridge_pairing pairing;

    client = platinum_bridge_client_new("https://bridge.example");
    if (client == NULL) {
        check(0, "client created for escaping");
        return;
    }

    /* A code carrying a quote must not be able to close the JSON string. */
    reset_fake(200, ok_pair, WF_OK);
    platinum_bridge_pair(client, "A\"B", &pairing);
    check_str(last_body, "{\"code\":\"A\\\"B\"}", "pair body escapes quotes");

    reset_fake(200, ok_pair, WF_OK);
    platinum_bridge_pair(client, "A\\B", &pairing);
    check_str(last_body, "{\"code\":\"A\\\\B\"}", "pair body escapes backslash");

    platinum_bridge_pairing_free(&pairing);
    platinum_bridge_client_free(client);
}

static void test_pair_rejects_bad_input(void)
{
    platinum_bridge_client *client;
    platinum_bridge_pairing pairing;
    wf_status status;

    client = platinum_bridge_client_new("https://bridge.example");
    if (client == NULL) {
        check(0, "client created for rejection");
        return;
    }

    /* A code longer than the bridge issues is a user typo, not a network
     * condition, and must not be sent. */
    reset_fake(200, ok_pair, WF_OK);
    status = platinum_bridge_pair(client, "K7Q2M9EXTRA", &pairing);
    check_status(status, WF_ERR_INVALID_ARG, "over-long code rejected");
    check(last_method == 0, "over-long code never reaches the network");

    check_status(platinum_bridge_pair(client, NULL, &pairing),
                 WF_ERR_INVALID_ARG, "null code rejected");
    check_status(platinum_bridge_pair(NULL, "K7Q2M9", &pairing),
                 WF_ERR_INVALID_ARG, "null client rejected");
    check_status(platinum_bridge_pair(client, "K7Q2M9", NULL),
                 WF_ERR_INVALID_ARG, "null out rejected");

    platinum_bridge_client_free(client);
}

static void test_pair_rejects_bad_responses(void)
{
    platinum_bridge_client *client;
    platinum_bridge_pairing pairing;
    wf_status status;

    client = platinum_bridge_client_new("https://bridge.example");
    if (client == NULL) {
        check(0, "client created for bad responses");
        return;
    }

    /* An expired or mistyped code is a normal 401 with a reason in the body.
     * It must not be mistaken for a pairing. */
    reset_fake(401, "{\"error\":\"invalid_or_expired_code\"}", WF_ERR_HTTP);
    status = platinum_bridge_pair(client, "K7Q2M9", &pairing);
    check_status(status, WF_ERR_HTTP, "expired code reported as HTTP error");
    check(pairing.token == NULL && pairing.did == NULL &&
              pairing.installation_id == NULL,
          "expired code yields no pairing");

    reset_fake(400, "{\"error\":\"invalid_code\"}", WF_ERR_HTTP);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_HTTP, "invalid code reported as HTTP error");

    /* A 200 that is not the documented shape must not produce a half-built
     * pairing: the caller has to be able to trust that token and did were both
     * present and valid. */
    reset_fake(200, "{\"protocol\":1,\"did\":\"did:plc:abc\"}", WF_OK);
    status = platinum_bridge_pair(client, "K7Q2M9", &pairing);
    check_status(status, WF_ERR_NOT_FOUND, "missing token reported");
    check(pairing.token == NULL && pairing.did == NULL &&
              pairing.installation_id == NULL,
          "missing token yields no pairing");

    reset_fake(200, "{\"protocol\":1,\"token\":\"T\"}", WF_OK);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_NOT_FOUND, "missing did reported");

    reset_fake(200,
               "{\"protocol\":1,\"token\":\"T\",\"did\":\"did:plc:a\"}",
               WF_OK);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_NOT_FOUND, "missing installation id reported");
    check(pairing.token == NULL && pairing.did == NULL &&
              pairing.installation_id == NULL,
          "missing installation id yields no pairing");

    reset_fake(200, "{\"protocol\":1,\"token\":123,\"did\":\"d\"}", WF_OK);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_PARSE, "non-string token rejected");

    /* A truncated body must not yield the token it happens to contain. */
    reset_fake(200, "{\"protocol\":1,\"token\":\"T\"", WF_OK);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_PARSE, "truncated body rejected");

    reset_fake(200, "not json at all", WF_OK);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_PARSE, "non-JSON body rejected");

    reset_fake(200, NULL, WF_OK);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_PARSE, "empty body rejected");

    /* A future protocol version must be refused explicitly. */
    reset_fake(200,
               "{\"protocol\":2,\"token\":\"T\",\"did\":\"did:plc:a\","
               "\"installationId\":\"i\"}",
               WF_OK);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_UNSUPPORTED, "unknown protocol version refused");
    check(pairing.token == NULL,
          "refused version yields no pairing");

    /* Transport failures pass through unchanged. */
    reset_fake(0, NULL, WF_ERR_NETWORK);
    check_status(platinum_bridge_pair(client, "K7Q2M9", &pairing),
                 WF_ERR_NETWORK, "network failure passed through");

    platinum_bridge_client_free(client);
}

static void test_pair_is_repeatable(void)
{
    platinum_bridge_client *client;
    platinum_bridge_pairing pairing;

    client = platinum_bridge_client_new("https://bridge.example");
    if (client == NULL) {
        check(0, "client created for repeat");
        return;
    }

    /* Reusing one output struct must not leak the previous token, and a failed
     * second attempt must not leave the first pairing in place looking valid.
     */
    reset_fake(200, ok_pair, WF_OK);
    platinum_bridge_pair(client, "K7Q2M9", &pairing);
    check_str(pairing.token, "TOKENVALUE", "first pairing token");

    reset_fake(401, "{\"error\":\"invalid_or_expired_code\"}", WF_ERR_HTTP);
    platinum_bridge_pair(client, "K7Q2M9", &pairing);
    check(pairing.token == NULL && pairing.did == NULL &&
              pairing.installation_id == NULL,
          "failed retry clears the previous pairing");

    platinum_bridge_client_free(client);
}

static void test_token_and_paths(void)
{
    platinum_bridge_client *client;
    wf_response response;

    memset(&response, 0, sizeof(response));

    client = platinum_bridge_client_new("https://bridge.example/");
    if (client == NULL) {
        check(0, "client created for paths");
        return;
    }

    check_status(platinum_bridge_client_set_token(client, "BRIDGETOKEN"),
                 WF_OK, "set token");
    check(auth_set, "token reached the transport");
    /* Wolfram adds the "Bearer " prefix itself, so the client must pass the
     * raw token. */
    check_str(last_auth, "BRIDGETOKEN", "raw token passed to transport");

    reset_fake(200, "{}", WF_OK);
    check_status(platinum_bridge_get(client, "/v1/profile", &response),
                 WF_OK, "get profile");
    check_str(last_url, "https://bridge.example/v1/profile", "get URL");
    check_str(last_body, "", "get sends no body");
    wf_response_free(&response);

    check_status(platinum_bridge_revoke(client, &response), WF_OK,
                 "revoke");
    check_str(last_url, "https://bridge.example/v1/revoke", "revoke URL");

    check_status(platinum_bridge_client_set_token(NULL, "T"),
                 WF_ERR_INVALID_ARG, "set token on null client");

    platinum_bridge_client_free(client);
}

static void test_post_validates_body(void)
{
    platinum_bridge_client *client;
    wf_response response;

    memset(&response, 0, sizeof(response));

    client = platinum_bridge_client_new("https://bridge.example");
    if (client == NULL) {
        check(0, "client created for post validation");
        return;
    }

    reset_fake(200, "{}", WF_OK);
    check_status(platinum_bridge_post(client, "/v1/post", "{\"text\":\"hi\"}",
                                      &response),
                 WF_OK, "valid body accepted");
    check_str(last_body, "{\"text\":\"hi\"}", "valid body sent");
    wf_response_free(&response);

    /* Malformed JSON must not reach the network. */
    reset_fake(200, "{}", WF_OK);
    check_status(platinum_bridge_post(client, "/v1/post", "{\"text\":",
                                      &response),
                 WF_ERR_PARSE, "malformed body rejected");
    check(last_method == 0, "malformed body never reaches the network");

    check_status(platinum_bridge_post(client, "/v1/post", "{}{}", &response),
                 WF_ERR_PARSE, "trailing content rejected");

    platinum_bridge_client_free(client);
}

static void test_client_lifecycle(void)
{
    platinum_bridge_client *client;

    check(platinum_bridge_client_new(NULL) == NULL, "null base URL refused");
    check(platinum_bridge_client_new("") == NULL, "empty base URL refused");

    /*
     * The cap has to be set on the transport, not merely intended: a bridge
     * response is attacker-reachable, so an uncapped read is a denial of
     * service on a machine with a few megabytes of memory.
     */
    last_max_response_bytes = 0;
    client = platinum_bridge_client_new("https://bridge.example");
    check(client != NULL, "client created for response cap");
    if (client != NULL) {
        check(last_max_response_bytes == 262144,
              "client caps bridge responses at 256 KiB");
        platinum_bridge_client_free(client);
    }

    platinum_bridge_client_free(NULL);
    check(1, "freeing null client is safe");
}

static void test_mark_seen(void)
{
    platinum_bridge_client *client;
    static char longer[80];

    client = platinum_bridge_client_new("https://bridge.example");
    reset_fake(200, "{\"seenAt\":\"2026-10-05T00:01:00.000Z\"}", WF_OK);
    check_status(platinum_bridge_mark_seen(client, "2026-10-05T00:01:00.000Z"),
                 WF_OK, "mark seen succeeds");
    check(last_method == 2, "mark seen is a POST");
    check_str(last_url, "https://bridge.example/v1/notifications/seen",
              "mark seen path");
    check_str(last_body, "{\"seenAt\":\"2026-10-05T00:01:00.000Z\"}",
              "mark seen body is the timestamp as sent");

    reset_fake(200, "{}", WF_OK);
    check_status(platinum_bridge_mark_seen(client, ""), WF_ERR_INVALID_ARG,
                 "empty seenAt refused");
    memset(longer, '9', 70);
    longer[70] = '\0';
    check_status(platinum_bridge_mark_seen(client, longer), WF_ERR_INVALID_ARG,
                 "over-long seenAt refused");
    check(last_method == 0, "refused seenAt makes no request");

    reset_fake(500, "{}", WF_ERR_HTTP);
    check_status(platinum_bridge_mark_seen(client, "2026-10-05T00:01:00.000Z"),
                 WF_ERR_HTTP, "bridge failure is reported");
    platinum_bridge_client_free(client);
}

static void test_muted_word(void)
{
    platinum_bridge_client *client;
    static char longer[500];

    client = platinum_bridge_client_new("https://bridge.example");
    reset_fake(200, "{\"value\":\"x\",\"on\":true}", WF_OK);
    check_status(platinum_bridge_set_muted_word(client, "spoil\"ers", 1), WF_OK,
                 "adding a muted word succeeds");
    check(last_method == 2, "a muted word is a POST");
    check_str(last_url, "https://bridge.example/v1/muted-words", "muted word path");
    check_str(last_body, "{\"value\":\"spoil\\\"ers\",\"on\":true}",
              "the word is escaped and on is true");
    reset_fake(200, "{}", WF_OK);
    platinum_bridge_set_muted_word(client, "x", 0);
    check_str(last_body, "{\"value\":\"x\",\"on\":false}", "removal sends on false");

    reset_fake(200, "{}", WF_OK);
    check_status(platinum_bridge_set_muted_word(client, "", 1), WF_ERR_INVALID_ARG,
                 "an empty word is refused");
    memset(longer, 'a', 450);
    longer[450] = '\0';
    check_status(platinum_bridge_set_muted_word(client, longer, 1), WF_ERR_INVALID_ARG,
                 "an over-long word is refused");
    check(last_method == 0, "a refused word makes no request");
    platinum_bridge_client_free(client);
}

static void test_post_body_ex(void)
{
    char *body;

    body = platinum_bridge_post_body_ex("look", NULL, NULL, "at://did:plc:a/app.bsky.feed.post/3k",
                                        "cq", 0);
    check(body != NULL &&
              strcmp(body, "{\"text\":\"look\",\"quote\":{\"uri\":\"at://did:plc:a/app.bsky.feed.post/3k\","
                           "\"cid\":\"cq\"}}") == 0,
          "a quote adds a quote member");
    free(body);

    body = platinum_bridge_post_body_ex("x", NULL, NULL, NULL, NULL, 3);
    check(body != NULL && strcmp(body, "{\"text\":\"x\",\"replyGate\":\"following\"}") == 0,
          "a gate adds replyGate by the bridge's name");
    free(body);

    body = platinum_bridge_post_body_ex("x", "at://r", "cr", "at://q", "cq", 0);
    check(body != NULL && strstr(body, "\"replyTo\"") != NULL && strstr(body, "\"quote\"") != NULL,
          "a reply can quote");
    free(body);

    body = platinum_bridge_post_body_ex("a\"b", NULL, NULL, "at://q\"", "c\\", 1);
    check(body != NULL && strstr(body, "\"at://q\\\"\"") != NULL && strstr(body, "\"c\\\\\"") != NULL &&
              strstr(body, "\"nobody\"") != NULL,
          "identifiers are escaped");
    free(body);

    check(platinum_bridge_post_body_ex("x", NULL, NULL, "at://q", NULL, 0) == NULL,
          "a quote needs both uri and cid");
    check(platinum_bridge_post_body_ex("x", "at://r", "cr", NULL, NULL, 2) == NULL,
          "a gate on a reply is refused");
    check(platinum_bridge_post_body_ex("x", NULL, NULL, NULL, NULL, 9) == NULL &&
              platinum_bridge_post_body_ex("x", NULL, NULL, NULL, NULL, -1) == NULL,
          "an unknown gate is refused");
    body = platinum_bridge_post_body_ex("x", NULL, NULL, NULL, NULL, 0);
    check(body != NULL && strcmp(body, "{\"text\":\"x\"}") == 0, "the default gate sends nothing");
    free(body);

    check(platinum_bridge_reply_gate_failed("{\"uri\":\"a\",\"cid\":\"b\",\"replyGateApplied\":false}") == 1 &&
              platinum_bridge_reply_gate_failed("{\"uri\":\"a\",\"replyGateApplied\":true}") == 0 &&
              platinum_bridge_reply_gate_failed("{\"uri\":\"a\"}") == 0 &&
              platinum_bridge_reply_gate_failed("not json") == 0 &&
              platinum_bridge_reply_gate_failed(NULL) == 0,
          "only an explicit false means the gate failed");
    check(strcmp(platinum_bridge_reply_gate_name(4), "followers") == 0 &&
              platinum_bridge_reply_gate_name(5) == NULL &&
              strcmp(platinum_bridge_reply_gate_label(0), "Everyone") == 0 &&
              platinum_bridge_reply_gate_label(-1) == NULL,
          "gate names and labels, bounded");
}

static void test_post_body(void)
{
    char *body;

    body = platinum_bridge_post_body("hello", NULL, NULL);
    check(body != NULL, "plain body built");
    check_str(body, "{\"text\":\"hello\"}", "plain post body is just text");
    free(body);

    body = platinum_bridge_post_body("say \"hi\"\n\\", "", "");
    check_str(body, "{\"text\":\"say \\\"hi\\\"\\n\\\\\"}",
              "text is escaped; empty reply fields mean a plain post");
    free(body);

    body = platinum_bridge_post_body("yes", "at://did:plc:a/app.bsky.feed.post/3k",
                                     "bafyabc");
    check_str(body,
              "{\"text\":\"yes\",\"replyTo\":{\"uri\":"
              "\"at://did:plc:a/app.bsky.feed.post/3k\",\"cid\":\"bafyabc\"}}",
              "reply body names the parent");
    free(body);

    body = platinum_bridge_post_body("yes", "at://x\"y", "c\\d");
    check_str(body,
              "{\"text\":\"yes\",\"replyTo\":{\"uri\":\"at://x\\\"y\","
              "\"cid\":\"c\\\\d\"}}",
              "reply identifiers are escaped too");
    free(body);

    check(platinum_bridge_post_body("yes", "at://x", NULL) == NULL,
          "a reply with only a uri is refused, not sent as a plain post");
    check(platinum_bridge_post_body("yes", NULL, "cid") == NULL,
          "a reply with only a cid is refused");
    check(platinum_bridge_post_body(NULL, NULL, NULL) == NULL, "NULL text refused");
}

int main(void)
{
    test_post_body();
    test_post_body_ex();
    test_mark_seen();
    test_muted_word();
    test_pair_success();
    test_pair_escapes_code();
    test_pair_rejects_bad_input();
    test_pair_rejects_bad_responses();
    test_pair_is_repeatable();
    test_token_and_paths();
    test_post_validates_body();
    test_client_lifecycle();

    if (failures != 0) {
        printf("test_bridge_client: %d of %d checks failed\n", failures,
               checks);
        return 1;
    }

    printf("test_bridge_client: all checks passed\n");
    return 0;
}
