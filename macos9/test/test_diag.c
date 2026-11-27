/*
 * test_diag.c -- host-side tests for the Connection Status model: the bridge
 * /health check, the native session's describeServer check, the case with
 * neither, and the lines shown. Links the real diag.c, bridge client and
 * native session client over the stubbed Wolfram transport. Not Classic Mac
 * OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "diag.h"
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

/* One scripted answer per transport call, in order. The script also records
 * what each call asked for: a URL for the bridge, an NSID for the session. */
typedef struct scripted_answer {
    int status;
    long http;
    const char *body;
} scripted_answer;

static scripted_answer answers[4];
static int answer_count;
static char asked[4][128];
static int asked_method[4];

static void script(int call, int method, const char *url, const char *body)
{
    (void)body;
    if (call >= 1 && call <= 4) {
        asked[call - 1][0] = '\0';
        if (url != NULL) {
            strncpy(asked[call - 1], url, sizeof(asked[0]) - 1);
            asked[call - 1][sizeof(asked[0]) - 1] = '\0';
        }
        asked_method[call - 1] = method;
    }
    if (call >= 1 && call <= answer_count) {
        fake_status = answers[call - 1].status;
        fake_http_status = answers[call - 1].http;
        fake_body = answers[call - 1].body;
    } else {
        fake_status = WF_ERR_NETWORK;
        fake_http_status = 0;
        fake_body = NULL;
    }
}

static void respond(int count, const scripted_answer *list)
{
    int i;

    for (i = 0; i < 4; ++i) {
        asked[i][0] = '\0';
        asked_method[i] = 0;
    }
    answer_count = count;
    for (i = 0; i < count && i < 4; ++i)
        answers[i] = list[i];
    fake_call_count = 0;
    fake_script = script;
}

static scripted_answer answer(int status, long http, const char *body)
{
    scripted_answer value;

    value.status = status;
    value.http = http;
    value.body = body;
    return value;
}

static char lines[PLATINUM_DIAG_LINES][PLATINUM_DIAG_LINE_MAX];

static const char *kHealthy =
    "{\"ok\":true,\"service\":\"platinum-bridge\",\"version\":\"0.4.0\"}";
static const char *kDescribed =
    "{\"did\":\"did:web:pds.example\",\"availableUserDomains\":[\".pds.example\"],"
    "\"inviteCodeRequired\":false}";

static platinum_atproto_client *native_client(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;

    client = platinum_atproto_client_new("https://pds.example");
    check(client != NULL, "native client created");
    if (client == NULL)
        return NULL;

    memset(&session, 0, sizeof(session));
    strcpy(session.access_jwt, "access-token-value");
    strcpy(session.refresh_jwt, "refresh-token-value");
    strcpy(session.did, "did:plc:alice");
    strcpy(session.handle, "alice.test");
    strcpy(session.pds_url, "https://pds.example");
    check(platinum_atproto_client_set_session(client, &session) == WF_OK,
          "native session installed");
    return client;
}

static int no_line_holds(const char *needle)
{
    int i;

    for (i = 0; i < PLATINUM_DIAG_LINES; ++i) {
        if (strstr(lines[i], needle) != NULL)
            return 0;
    }
    return 1;
}

static void test_bridge_only(void)
{
    platinum_diag diag;
    scripted_answer list[1];
    platinum_bridge_client *bridge = platinum_bridge_client_new("https://bridge.example");

    list[0] = answer(WF_OK, 200, kHealthy);
    respond(1, list);
    check(platinum_diag_check(&diag, bridge, "https://bridge.example", 1, NULL,
                              WF_OK, 2097152) == WF_OK,
          "a healthy bridge answers");
    check(fake_call_count == 1, "only the bridge is asked without a session");
    check(strstr(asked[0], "/health") != NULL && asked_method[0] == 1, "asks GET /health");
    check(diag.health_ok && strcmp(diag.service, "platinum-bridge") == 0 &&
              strcmp(diag.version, "0.4.0") == 0,
          "service and version are read");
    check(!diag.has_session && !diag.server_ok, "no session is recorded");
    platinum_diag_lines(&diag, lines);
    check(strcmp(lines[0], "Bridge: https://bridge.example") == 0, "bridge line");
    check(strstr(lines[1], "yes") != NULL && strstr(lines[1], "never shown") != NULL,
          "signed-in line");
    check(strcmp(lines[2], "Bridge health: OK, platinum-bridge 0.4.0") == 0, "health line");
    check(strstr(lines[3], "not signed in natively") != NULL, "service line says none");
    check(strstr(lines[4], "nothing to ask") != NULL, "service check is not made");
    check(strstr(lines[5], "none since") != NULL, "no error yet");
    check(strcmp(lines[6], "Free memory: 2048 KB") == 0, "memory in KB");
    check(strstr(lines[7], "Checked when") != NULL, "footer says when");
    platinum_bridge_client_free(bridge);
}

static void test_bridge_failures(void)
{
    platinum_diag diag;
    scripted_answer list[1];
    platinum_bridge_client *bridge = platinum_bridge_client_new("https://bridge.example");

    list[0] = answer(WF_ERR_NETWORK, 0, NULL);
    respond(1, list);
    platinum_diag_check(&diag, bridge, "https://bridge.example", 0, NULL, WF_ERR_AUTH, -1);
    platinum_diag_lines(&diag, lines);
    check(strstr(lines[2], "failed") != NULL && strstr(lines[2], "no network answer") != NULL,
          "a network failure is described");
    check(strcmp(lines[1], "Signed in: no") == 0, "not signed in");
    check(strstr(lines[5], "sign-in was refused") != NULL, "the last error is described");
    check(strcmp(lines[6], "Free memory: unknown") == 0, "unknown memory says so");

    list[0] = answer(WF_ERR_HTTP, 502, "<html>bad gateway</html>");
    respond(1, list);
    platinum_diag_check(&diag, bridge, "https://bridge.example", 1, NULL, WF_OK, 0);
    platinum_diag_lines(&diag, lines);
    check(strcmp(lines[2], "Bridge health: HTTP 502") == 0, "an HTTP error shows its status");

    list[0] = answer(WF_OK, 200, "<html>some other server</html>");
    respond(1, list);
    platinum_diag_check(&diag, bridge, "https://bridge.example", 1, NULL, WF_OK, 0);
    platinum_diag_lines(&diag, lines);
    check(!diag.health_ok && strstr(lines[2], "not like a Platinum bridge") != NULL,
          "a 200 that is not the bridge's health reply is not called healthy");

    list[0] = answer(WF_OK, 200, "{\"ok\":false}");
    respond(1, list);
    platinum_diag_check(&diag, bridge, "https://bridge.example", 1, NULL, WF_OK, 0);
    check(!diag.health_ok, "ok:false is not healthy");

    platinum_bridge_client_free(bridge);
}

static void test_native_only(void)
{
    platinum_diag diag;
    scripted_answer list[1];
    platinum_atproto_client *client = native_client();

    if (client == NULL)
        return;
    list[0] = answer(WF_OK, 200, kDescribed);
    respond(1, list);
    check(platinum_diag_check(&diag, NULL, "", 1, client, WF_OK, 1048576) == WF_OK,
          "a native session alone is enough to check");
    check(fake_call_count == 1, "one call, to the service");
    check(strcmp(asked[0], "com.atproto.server.describeServer") == 0 && asked_method[0] == 1,
          "asks GET describeServer");
    check(last_param_count == 0, "with no parameters");
    check(diag.has_session && diag.server_ok &&
              strcmp(diag.server_did, "did:web:pds.example") == 0,
          "the service's DID is read");
    check(diag.health_status == WF_ERR_CONFIG && !diag.health_ok,
          "the bridge is recorded as not configured");
    platinum_diag_lines(&diag, lines);
    check(strcmp(lines[0], "Bridge: none set") == 0, "no bridge line");
    check(strcmp(lines[1], "Signed in: yes, as alice.test (tokens are never shown)") == 0,
          "signed in natively, by handle");
    check(strstr(lines[2], "no bridge is configured") != NULL, "bridge health says not configured");
    check(strcmp(lines[3], "Service: https://pds.example") == 0, "the service is named");
    check(strcmp(lines[4], "Service check: OK, did:web:pds.example") == 0, "service check line");
    check(no_line_holds("access-token-value") && no_line_holds("refresh-token-value"),
          "no token appears in any line");

    /* A server error: the HTTP status is shown with what it meant. */
    list[0] = answer(WF_ERR_HTTP, 500,
                     "{\"error\":\"InternalServerError\",\"message\":\"secret detail\"}");
    respond(1, list);
    check(platinum_diag_check(&diag, NULL, "", 1, client, WF_OK, 0) != WF_OK,
          "a failing service is a failure");
    platinum_diag_lines(&diag, lines);
    check(!diag.server_ok && strstr(lines[4], "HTTP 500") != NULL, "an HTTP error shows its status");
    check(no_line_holds("secret detail"), "the upstream message is never copied");

    /* A 200 that is not an AT Protocol service. */
    list[0] = answer(WF_OK, 200, "<html>captive portal</html>");
    respond(1, list);
    platinum_diag_check(&diag, NULL, "", 1, client, WF_OK, 0);
    platinum_diag_lines(&diag, lines);
    check(!diag.server_ok && strstr(lines[4], "not like an AT Protocol service") != NULL,
          "an HTML 200 is not called healthy");

    list[0] = answer(WF_OK, 200, "{\"did\":\"not-a-did\"}");
    respond(1, list);
    platinum_diag_check(&diag, NULL, "", 1, client, WF_OK, 0);
    check(!diag.server_ok && diag.server_did[0] == '\0', "a value that is not a DID is refused");

    list[0] = answer(WF_OK, 200, "{\"availableUserDomains\":[]}");
    respond(1, list);
    platinum_diag_check(&diag, NULL, "", 1, client, WF_OK, 0);
    check(!diag.server_ok, "an answer with no DID is not healthy");

    list[0] = answer(WF_ERR_NETWORK, 0, NULL);
    respond(1, list);
    check(platinum_diag_check(&diag, NULL, "", 1, client, WF_OK, 0) == WF_ERR_NETWORK,
          "a network failure is returned");
    platinum_diag_lines(&diag, lines);
    check(strstr(lines[4], "failed") != NULL && strstr(lines[4], "no network answer") != NULL,
          "and described");

    platinum_atproto_client_free(client);
}

static void test_both(void)
{
    platinum_diag diag;
    scripted_answer list[2];
    platinum_bridge_client *bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_atproto_client *client = native_client();

    if (client == NULL || bridge == NULL)
        return;
    list[0] = answer(WF_OK, 200, kHealthy);
    list[1] = answer(WF_OK, 200, kDescribed);
    respond(2, list);
    check(platinum_diag_check(&diag, bridge, "https://bridge.example", 1, client,
                              WF_OK, 0) == WF_OK,
          "both sources answer");
    check(fake_call_count == 2, "both are asked");
    check(strstr(asked[0], "/health") != NULL, "the bridge first");
    check(strcmp(asked[1], "com.atproto.server.describeServer") == 0, "then the service");
    platinum_diag_lines(&diag, lines);
    check(strcmp(lines[2], "Bridge health: OK, platinum-bridge 0.4.0") == 0, "bridge line kept");
    check(strcmp(lines[4], "Service check: OK, did:web:pds.example") == 0,
          "service line alongside it");

    /* One down, one up: each line reports its own source. */
    list[0] = answer(WF_ERR_NETWORK, 0, NULL);
    list[1] = answer(WF_OK, 200, kDescribed);
    respond(2, list);
    check(platinum_diag_check(&diag, bridge, "https://bridge.example", 1, client,
                              WF_OK, 0) == WF_ERR_NETWORK,
          "the bridge's failure is returned");
    check(fake_call_count == 2, "a bridge failure does not stop the service check");
    platinum_diag_lines(&diag, lines);
    check(strstr(lines[2], "failed") != NULL && diag.server_ok, "the service still reads OK");

    list[0] = answer(WF_OK, 200, kHealthy);
    list[1] = answer(WF_ERR_TIMEOUT, 0, NULL);
    respond(2, list);
    check(platinum_diag_check(&diag, bridge, "https://bridge.example", 1, client,
                              WF_OK, 0) == WF_ERR_TIMEOUT,
          "the service's failure is returned");
    platinum_diag_lines(&diag, lines);
    check(diag.health_ok && strstr(lines[4], "timed out") != NULL,
          "the bridge still reads OK and the service timed out");

    platinum_atproto_client_free(client);
    platinum_bridge_client_free(bridge);
}

static void test_neither_and_bounds(void)
{
    platinum_diag diag;
    static char long_url[400];

    respond(0, NULL);
    check(platinum_diag_check(&diag, NULL, "", 0, NULL, WF_OK, 0) == WF_ERR_CONFIG,
          "neither source is a config error");
    check(fake_call_count == 0, "nothing is sent");
    platinum_diag_lines(&diag, lines);
    check(strcmp(lines[0], "Bridge: none set") == 0 && strstr(lines[2], "no bridge is configured") != NULL,
          "it says no bridge is set");
    check(strcmp(lines[1], "Signed in: no") == 0, "and nobody is signed in");
    check(strstr(lines[3], "not signed in natively") != NULL &&
              strstr(lines[4], "nothing to ask") != NULL,
          "and there is no service");
    check(strstr(lines[7], "pair a bridge or sign in") != NULL, "the footer says what to do");

    platinum_diag_init(&diag);
    platinum_diag_lines(&diag, lines);
    check(strstr(lines[2], "not checked yet") != NULL &&
              strstr(lines[4], "not checked yet") != NULL &&
              strstr(lines[7], "Check Again") != NULL,
          "an unchecked snapshot says so");

    memset(long_url, 'a', 390);
    long_url[390] = '\0';
    platinum_diag_check(&diag, NULL, long_url, 1, NULL, WF_OK, 0);
    check(strlen(diag.url) == PLATINUM_DIAG_URL_MAX, "an over-long address is clipped, not overrun");
    platinum_diag_lines(&diag, lines);
    check(strlen(lines[0]) < PLATINUM_DIAG_LINE_MAX, "and so is its line");

    check(strcmp(platinum_status_text(WF_OK), "no error") == 0 &&
              platinum_status_text((wf_status)999)[0] != '\0',
          "every status has some text");
    check(platinum_diag_check(NULL, NULL, "", 0, NULL, WF_OK, 0) == WF_ERR_INVALID_ARG,
          "NULL is refused");
    fake_script = NULL;
}

int main(void)
{
    test_bridge_only();
    test_bridge_failures();
    test_native_only();
    test_both();
    test_neither_and_bounds();
    if (failures != 0) {
        printf("test_diag: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_diag: all %d checks passed\n", checks);
    return 0;
}
