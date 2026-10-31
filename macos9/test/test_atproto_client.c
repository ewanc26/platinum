/*
 * test_atproto_client.c -- host tests for the native XRPC session layer.
 *
 * wolfram_stub stands in for the transport, so these tests drive exact URLs,
 * auth headers and a refresh-and-retry sequence without a PDS. The fixtures
 * are synthetic; no real account or token is present.
 */

#include <stdio.h>
#include <string.h>

#include "atproto_client.h"
#include "wolfram_stub.h"

#define FIXTURE_DIR "macos9/test/fixtures/atproto/"

static int failures;
static int checks;
static int saves;
static char auth_at_call[8][256];

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
               got != NULL ? got : "(null)", want);
    }
}

static int load_fixture(const char *name, char *buffer, size_t capacity)
{
    char path[256];
    FILE *file;
    size_t length;

    strcpy(path, FIXTURE_DIR);
    strcat(path, name);
    file = fopen(path, "rb");
    if (file == NULL) {
        printf("FAIL: cannot open %s (run from the repository root)\n", path);
        failures++;
        checks++;
        return 0;
    }
    length = fread(buffer, 1, capacity - 1, file);
    fclose(file);
    buffer[length] = '\0';
    return 1;
}

static int save_session(const platinum_atproto_session *session, void *userdata)
{
    (void)session;
    (void)userdata;
    saves++;
    return 0;
}

static void reset_stub(void)
{
    fake_status = WF_OK;
    fake_http_status = 200;
    fake_body = NULL;
    fake_body_len = 0;
    fake_call_count = 0;
    fake_script = NULL;
    auth_set = 0;
    last_auth[0] = '\0';
    last_url[0] = '\0';
    last_body[0] = '\0';
    saves = 0;
    memset(auth_at_call, 0, sizeof(auth_at_call));
}

static void remember_auth(int call)
{
    if (call >= 0 && call < 8) {
        strncpy(auth_at_call[call], last_auth,
                sizeof(auth_at_call[call]) - 1);
        auth_at_call[call][sizeof(auth_at_call[call]) - 1] = '\0';
    }
}

static void record_auth_script(int call, int method, const char *url,
                               const char *body)
{
    (void)method;
    (void)url;
    (void)body;
    remember_auth(call);
}

static int make_session(platinum_atproto_session *session)
{
    static char body[8192];

    if (!load_fixture("create_session.json", body, sizeof(body)))
        return 0;
    return platinum_atproto_session_parse_create(body, "https://pds.example",
                                                 session) == WF_OK;
}

static void test_sign_in_success(void)
{
    static char body[8192];
    platinum_atproto_client *client;

    reset_stub();
    if (!load_fixture("create_session.json", body, sizeof(body)))
        return;

    fake_body = body;
    fake_script = record_auth_script;
    client = platinum_atproto_client_new("https://PDS.Example/");
    check(client != NULL, "client created");
    if (client == NULL)
        return;
    platinum_atproto_client_set_save(client, save_session, NULL);

    check(platinum_atproto_client_sign_in(client, "example.test",
                                          "app-password") == WF_OK,
          "sign-in succeeds");
    check_str(platinum_atproto_client_session(client)->did,
              "did:plc:testaccount", "session DID");
    check(strstr(last_url, "/xrpc/com.atproto.server.createSession") != NULL,
          "createSession URL");
    check(strstr(last_body, "\"identifier\":\"example.test\"") != NULL,
          "identifier in sign-in body");
    check(strstr(last_body, "app-password") != NULL,
          "password in the request body");
    check(auth_at_call[1][0] == '\0', "transport saw no auth on sign-in");
    check(saves == 1, "save hook called after sign-in");
    check(platinum_atproto_client_scratch_clear(),
          "sign-in scratch wiped after success");

    platinum_atproto_client_free(client);
}

static void test_sign_in_refused(void)
{
    static char body[1024];
    platinum_atproto_client *client;

    reset_stub();
    if (!load_fixture("error_expired.json", body, sizeof(body)))
        return;

    fake_status = WF_ERR_HTTP;
    fake_http_status = 401;
    fake_body = body;
    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL) {
        check(0, "client created");
        return;
    }
    platinum_atproto_client_set_save(client, save_session, NULL);

    check(platinum_atproto_client_sign_in(client, "example.test", "wrong") ==
              WF_ERR_AUTH,
          "refused sign-in maps to auth");
    check(platinum_atproto_client_session(client)->access_jwt[0] == '\0',
          "no session after a refused sign-in");
    check(saves == 0, "save hook not called after a refusal");
    check(platinum_atproto_client_scratch_clear(),
          "sign-in scratch wiped after failure");
    platinum_atproto_client_free(client);
}

static void test_authenticated_get(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;
    wf_response response;
    const char *names[2];
    const char *values[2];

    reset_stub();
    if (!make_session(&session))
        return;

    names[0] = "limit";
    values[0] = "20";
    names[1] = "cursor";
    values[1] = "c1";
    fake_body = "{\"posts\":[]}";
    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL) {
        check(0, "client created");
        return;
    }
    check(platinum_atproto_client_set_session(client, &session) == WF_OK,
          "session installed");
    memset(&response, 0, sizeof(response));

    check(platinum_atproto_client_get(client, "app.bsky.feed.getTimeline",
                                      names, values, 2, &response) == WF_OK,
          "authenticated GET succeeds");
    check_str(response.body, "{\"posts\":[]}", "response body kept");
    check(strcmp(last_url,
                 "https://pds.example/xrpc/app.bsky.feed.getTimeline"
                 "?limit=20&cursor=c1") == 0,
          "XRPC URL with parameters");
    check(strcmp(last_auth, session.access_jwt) == 0, "access token attached");
    check(last_method == 1, "GET used");
    wf_response_free(&response);
    platinum_atproto_client_free(client);
}

static void retry_script(int call, int method, const char *url,
                         const char *body)
{
    (void)method;
    (void)url;
    (void)body;
    remember_auth(call);
    if (call == 1) {
        fake_status = WF_ERR_HTTP;
        fake_http_status = 401;
        fake_body = "{\"error\":\"ExpiredToken\"}";
    } else if (call == 2) {
        fake_status = WF_OK;
        fake_http_status = 200;
        fake_body = "{\"accessJwt\":\"new.access.token\","
                    "\"refreshJwt\":\"new.refresh.token\","
                    "\"did\":\"did:plc:testaccount\","
                    "\"handle\":\"example.test\"}";
    } else {
        fake_status = WF_OK;
        fake_http_status = 200;
        fake_body = "{\"posts\":[{\"uri\":\"at://x\"}]}";
    }
}

static void test_refresh_and_retry(void)
{
    platinum_atproto_session session;
    platinum_atproto_session refreshed;
    platinum_atproto_client *client;
    wf_response response;
    char old_access[PLATINUM_ATPROTO_JWT_MAX + 1];
    char old_refresh[PLATINUM_ATPROTO_JWT_MAX + 1];

    reset_stub();
    if (!make_session(&session))
        return;
    strcpy(old_access, session.access_jwt);
    strcpy(old_refresh, session.refresh_jwt);

    fake_script = retry_script;
    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL) {
        check(0, "client created");
        return;
    }
    platinum_atproto_client_set_save(client, save_session, NULL);
    check(platinum_atproto_client_set_session(client, &session) == WF_OK,
          "session installed before retry");
    memset(&response, 0, sizeof(response));

    check(platinum_atproto_client_get(client, "app.bsky.feed.getTimeline",
                                      NULL, NULL, 0, &response) == WF_OK,
          "expired token is refreshed and the call retried");
    check(fake_call_count == 3, "one call, one refresh, one retry");
    check(strcmp(auth_at_call[1], old_access) == 0,
          "first call used the old access token");
    check(strcmp(auth_at_call[2], old_refresh) == 0,
          "refresh used the refresh token");
    check(strcmp(auth_at_call[3], "new.access.token") == 0,
          "retry used the new access token");
    check(strcmp(platinum_atproto_client_session(client)->access_jwt,
                 "new.access.token") == 0,
          "session updated after refresh");
    check(saves == 1, "refresh saved the new session");
    check(strstr(response.body, "\"posts\"") != NULL, "retry response kept");
    wf_response_free(&response);

    refreshed = *platinum_atproto_client_session(client);
    check(strcmp(refreshed.did, "did:plc:testaccount") == 0,
          "refresh kept the DID");
    check(strcmp(refreshed.pds_url, "https://pds.example") == 0,
          "refresh kept the PDS");
    platinum_atproto_client_free(client);
}

static void failed_refresh_script(int call, int method, const char *url,
                                  const char *body)
{
    (void)url;
    (void)body;
    remember_auth(call);
    fake_status = WF_ERR_HTTP;
    fake_http_status = 401;
    fake_body = "{\"error\":\"InvalidToken\"}";
    (void)method;
}

static void test_refresh_failure(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;
    wf_response response;

    reset_stub();
    if (!make_session(&session))
        return;

    fake_script = failed_refresh_script;
    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL) {
        check(0, "client created");
        return;
    }
    platinum_atproto_client_set_save(client, save_session, NULL);
    (void)platinum_atproto_client_set_session(client, &session);
    memset(&response, 0, sizeof(response));

    check(platinum_atproto_client_get(client, "app.bsky.feed.getTimeline",
                                      NULL, NULL, 0, &response) == WF_ERR_AUTH,
          "failed refresh returns auth");
    check(fake_call_count == 2, "no retry after a failed refresh");
    check(platinum_atproto_client_session(client)->access_jwt[0] == '\0',
          "failed refresh clears the session");
    check(saves == 0, "failed refresh does not save");
    wf_response_free(&response);
    platinum_atproto_client_free(client);
}

static void test_non_auth_error_is_not_retried(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;
    wf_response response;

    reset_stub();
    if (!make_session(&session))
        return;

    fake_status = WF_ERR_HTTP;
    fake_http_status = 500;
    fake_body = "{\"error\":\"InternalServerError\"}";
    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL) {
        check(0, "client created");
        return;
    }
    (void)platinum_atproto_client_set_session(client, &session);
    memset(&response, 0, sizeof(response));

    check(platinum_atproto_client_get(client, "app.bsky.feed.getTimeline",
                                      NULL, NULL, 0, &response) == WF_ERR_HTTP,
          "server error is not retried");
    check(fake_call_count == 1, "exactly one call for a non-auth error");
    check(platinum_atproto_client_session(client)->access_jwt[0] != '\0',
          "non-auth error keeps the session");
    wf_response_free(&response);
    platinum_atproto_client_free(client);
}

static void test_post_body(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;
    wf_response response;

    reset_stub();
    if (!make_session(&session))
        return;

    fake_body = "{\"ok\":true}";
    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL) {
        check(0, "client created");
        return;
    }
    (void)platinum_atproto_client_set_session(client, &session);
    memset(&response, 0, sizeof(response));

    check(platinum_atproto_client_post(client, "app.bsky.feed.like",
                                       "{\"uri\":\"at://x\"}", &response) ==
              WF_OK,
          "POST succeeds");
    check(last_method == 2, "POST used");
    check(strcmp(last_body, "{\"uri\":\"at://x\"}") == 0,
          "POST body forwarded");
    check(platinum_atproto_client_post(client, "app.bsky.feed.like", "{",
                                       &response) == WF_ERR_PARSE,
          "malformed JSON refused before sending");
    wf_response_free(&response);
    platinum_atproto_client_free(client);
}

int main(void)
{
    test_sign_in_success();
    test_sign_in_refused();
    test_authenticated_get();
    test_refresh_and_retry();
    test_refresh_failure();
    test_non_auth_error_is_not_retried();
    test_post_body();

    if (failures != 0) {
        printf("test_atproto_client: %d of %d checks failed\n", failures,
               checks);
        return 1;
    }
    printf("test_atproto_client: all %d checks passed\n", checks);
    return 0;
}
