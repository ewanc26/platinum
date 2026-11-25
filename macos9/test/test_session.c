/*
 * test_session.c -- host tests for signing out.
 *
 * wolfram_stub stands in for the transport and the File Manager side of
 * config.c is replaced below, so no PDS, bridge or Toolbox is needed.
 */

#include <stdio.h>
#include <string.h>

#include "session.h"
#include "wolfram_stub.h"

static int failures;
static int checks;
static int config_clears;
static int calls;
static char nsid_at_call[4][128];
static char auth_at_call[4][256];

void platinum_config_init(platinum_config *config)
{
    memset(config, 0, sizeof(*config));
}

int platinum_config_is_paired(const platinum_config *config)
{
    return config->bridge_token[0] != '\0';
}

OSErr platinum_config_load(platinum_config *config)
{
    platinum_config_init(config);
    return noErr;
}

OSErr platinum_config_save(const platinum_config *config)
{
    (void)config;
    return noErr;
}

OSErr platinum_config_clear(void)
{
    config_clears++;
    return noErr;
}

OSErr platinum_config_set_bridge_url(platinum_config *config,
                                     const char *value)
{
    strcpy(config->bridge_url, value);
    return noErr;
}

OSErr platinum_config_set_token(platinum_config *config, const char *value)
{
    strcpy(config->bridge_token, value);
    return noErr;
}

OSErr platinum_config_set_did(platinum_config *config, const char *value)
{
    strcpy(config->did, value);
    return noErr;
}

OSErr platinum_config_set_installation_id(platinum_config *config,
                                          const char *value)
{
    strcpy(config->installation_id, value);
    return noErr;
}

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static void record(int call, int method, const char *url, const char *body)
{
    (void)method;
    (void)body;
    calls = call;
    if (call <= 4) {
        strncpy(nsid_at_call[call - 1], url != NULL ? url : "",
                sizeof(nsid_at_call[0]) - 1);
        strcpy(auth_at_call[call - 1], last_auth);
    }
}

static void reset_stub(void)
{
    fake_status = WF_OK;
    fake_http_status = 200;
    fake_body = NULL;
    fake_body_len = 0;
    fake_call_count = 0;
    fake_script = record;
    calls = 0;
    config_clears = 0;
    memset(nsid_at_call, 0, sizeof(nsid_at_call));
    memset(auth_at_call, 0, sizeof(auth_at_call));
}

static void sign_in(platinum_session *session)
{
    platinum_atproto_session native;

    platinum_session_init(session);
    memset(&native, 0, sizeof(native));
    strcpy(native.access_jwt, "access-token");
    strcpy(native.refresh_jwt, "refresh-token");
    strcpy(native.did, "did:plc:abcdefghijklmnopqrstuvwx");
    strcpy(native.handle, "alice.example.com");
    strcpy(native.pds_url, "https://pds.example.com");
    session->client = platinum_atproto_client_new(native.pds_url);
    check(session->client != NULL &&
              platinum_atproto_client_set_session(session->client, &native) ==
                  WF_OK,
          "native session set up");
}

static void check_signed_out(const platinum_session *session)
{
    check(session->client == NULL, "native client is gone");
    check(session->bridge == NULL, "bridge client is gone");
    check(!platinum_session_is_paired(session), "not paired");
    check(config_clears == 1, "stored pairing cleared once");
}

static void test_native_sign_out(void)
{
    platinum_session session;

    reset_stub();
    sign_in(&session);
    check(platinum_session_sign_out(&session) == WF_OK, "sign-out succeeds");
    check(calls == 1, "one request");
    check(strcmp(nsid_at_call[0], "com.atproto.server.deleteSession") == 0,
          "deleteSession is called");
    check(strcmp(auth_at_call[0], "refresh-token") == 0,
          "with the refresh token");
    check(last_method == 2 && last_body[0] == '\0', "as a POST with no body");
    check(last_auth[0] == '\0', "the transport forgets the token");
    check_signed_out(&session);
}

static void test_refused_sign_out(long http_status)
{
    platinum_session session;

    reset_stub();
    sign_in(&session);
    fake_status = WF_ERR_HTTP;
    fake_http_status = http_status;
    check(platinum_session_sign_out(&session) != WF_OK,
          "failure is reported");
    check(calls == 1, "no refresh and no retry");
    check_signed_out(&session);
}

static void test_bridge_only(void)
{
    platinum_session session;

    reset_stub();
    platinum_session_init(&session);
    session.bridge = platinum_bridge_client_new("http://bridge.example.com");
    check(session.bridge != NULL, "bridge set up");
    strcpy(session.config.bridge_token, "bridge-token");
    check(platinum_session_sign_out(&session) == WF_OK, "sign-out succeeds");
    check(calls == 1, "one request");
    check(strstr(nsid_at_call[0], "/v1/revoke") != NULL,
          "only the bridge is told");
    check_signed_out(&session);
}

static void test_bridge_failure_still_deletes_native(void)
{
    platinum_session session;

    reset_stub();
    sign_in(&session);
    session.bridge = platinum_bridge_client_new("http://bridge.example.com");
    strcpy(session.config.bridge_token, "bridge-token");
    fake_status = WF_ERR_NETWORK;
    check(platinum_session_sign_out(&session) != WF_OK,
          "failure is reported");
    check(calls == 2, "both servers are told");
    check(strcmp(nsid_at_call[1], "com.atproto.server.deleteSession") == 0,
          "native session is deleted after the bridge fails");
    check_signed_out(&session);
}

int main(void)
{
    test_native_sign_out();
    test_refused_sign_out(401);
    test_refused_sign_out(500);
    test_bridge_only();
    test_bridge_failure_still_deletes_native();

    if (failures != 0) {
        printf("test_session: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_session: all %d checks passed\n", checks);
    return 0;
}
