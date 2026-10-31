/*
 * test_atproto_session.c -- host tests for the native AT Protocol session.
 *
 * The fixtures are synthetic: the JWTs carry a test-only signature and no
 * account exists behind them. They are shaped like real createSession and
 * refreshSession responses so the parser is exercised on the fields it reads.
 */

#include <stdio.h>
#include <string.h>

#include "atproto_session.h"

#define FIXTURE_DIR "macos9/test/fixtures/atproto/"

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

static void check_status(wf_status got, wf_status want, const char *what)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s (got %d, want %d)\n", what, (int)got, (int)want);
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

static void test_create_session(void)
{
    static char body[8192];
    platinum_atproto_session session;

    if (!load_fixture("create_session.json", body, sizeof(body)))
        return;

    platinum_atproto_session_init(&session);
    check_status(platinum_atproto_session_parse_create(
                     body, "https://PDS.Example/", &session),
                 WF_OK, "createSession parsed");
    check_str(session.did, "did:plc:testaccount", "session DID");
    check_str(session.handle, "example.test", "session handle");
    check_str(session.pds_url, "https://pds.example", "session PDS normalised");
    check(session.access_jwt[0] != '\0', "access token kept");
    check(session.refresh_jwt[0] != '\0', "refresh token kept");
    check(strcmp(session.access_jwt, session.refresh_jwt) != 0,
          "access and refresh tokens differ");

    check(platinum_atproto_session_access_expiry(&session) == 1900000000L,
          "expiry read from the access JWT");
    check(!platinum_atproto_session_access_expired(&session, 1800000000L, 0L),
          "not expired before exp");
    check(platinum_atproto_session_access_expired(&session, 1900000000L, 0L),
          "expired at exp");
    check(platinum_atproto_session_access_expired(&session, 1899999950L, 60L),
          "refresh skew brings the boundary forward");
    check(!platinum_atproto_session_access_expired(&session, 1899999900L, 60L),
          "skew boundary is exclusive before it");

    platinum_atproto_session_clear(&session);
}

static void test_expired_token(void)
{
    static char body[8192];
    platinum_atproto_session session;

    if (!load_fixture("create_session_expired.json", body, sizeof(body)))
        return;

    check_status(platinum_atproto_session_parse_create(body,
                                                       "https://pds.example",
                                                       &session),
                 WF_OK, "expired session still parses");
    check(platinum_atproto_session_access_expiry(&session) == 1000000000L,
          "expired token expiry read");
    check(platinum_atproto_session_access_expired(&session, 1700000000L, 0L),
          "old token is expired");
}

static void test_refresh(void)
{
    static char create_body[8192];
    static char refresh_body[8192];
    platinum_atproto_session session;
    platinum_atproto_session refreshed;
    char old_access[PLATINUM_ATPROTO_JWT_MAX + 1];

    if (!load_fixture("create_session.json", create_body, sizeof(create_body)) ||
        !load_fixture("refresh_session.json", refresh_body,
                      sizeof(refresh_body)))
        return;

    check_status(platinum_atproto_session_parse_create(create_body,
                                                       "https://pds.example",
                                                       &session),
                 WF_OK, "session before refresh");
    strcpy(old_access, session.access_jwt);

    check_status(platinum_atproto_session_parse_refresh(refresh_body, &session,
                                                        &refreshed),
                 WF_OK, "refreshSession parsed");
    check_str(refreshed.did, "did:plc:testaccount",
              "refresh keeps the DID");
    check_str(refreshed.pds_url, "https://pds.example",
              "refresh keeps the PDS");
    check_str(refreshed.handle, "moved.example.test",
              "refresh takes a new handle");
    check(strcmp(refreshed.access_jwt, old_access) != 0,
          "refresh replaces the access token");
    check(platinum_atproto_session_access_expiry(&refreshed) == 1900010000L,
          "refresh expiry read");

    strcpy(old_access, session.refresh_jwt);
    check_status(platinum_atproto_session_parse_refresh(refresh_body, &session,
                                                        &session),
                 WF_OK, "refresh may reuse the session object");
    check_str(session.did, "did:plc:testaccount",
              "in-place refresh keeps the DID");
    check(strcmp(session.refresh_jwt, old_access) != 0,
          "in-place refresh replaces the refresh token");

    check_status(platinum_atproto_session_parse_refresh(refresh_body, NULL,
                                                        &refreshed),
                 WF_ERR_INVALID_ARG, "refresh without a previous session refused");
}

static void test_bad_responses(void)
{
    platinum_atproto_session session;

    platinum_atproto_session_init(&session);
    check_status(platinum_atproto_session_parse_create(
                     "{}", "https://pds.example", &session),
                 WF_ERR_PARSE, "empty object refused");
    check(session.access_jwt[0] == '\0' && session.did[0] == '\0',
          "failed create leaves nothing behind");

    check_status(platinum_atproto_session_parse_create(
                     "{\"accessJwt\":\"a.b.c\",\"did\":\"did:plc:x\","
                     "\"handle\":\"x.test\"}",
                     "https://pds.example", &session),
                 WF_ERR_PARSE, "missing refresh token refused");
    check(session.refresh_jwt[0] == '\0', "failed parse clears the refresh token");

    check_status(platinum_atproto_session_parse_create(
                     "{\"accessJwt\":\"not-a-jwt\",\"refreshJwt\":\"a.b.c\","
                     "\"did\":\"did:plc:x\",\"handle\":\"x.test\"}",
                     "https://pds.example", &session),
                 WF_ERR_PARSE, "malformed access token refused");

    check_status(platinum_atproto_session_parse_create(
                     "{\"accessJwt\":\"a.b.c\",\"refreshJwt\":\"a.b.c\","
                     "\"did\":\"did:plc:x\",\"handle\":\"x.test\"}",
                     "http://pds.example", &session),
                 WF_ERR_INVALID_ARG, "plain-http PDS refused");

    check_status(platinum_atproto_session_parse_create(
                     "{\"accessJwt\":", "https://pds.example", &session),
                 WF_ERR_PARSE, "truncated body refused");
}

static void test_error_mapping(void)
{
    static char body[1024];

    if (load_fixture("error_expired.json", body, sizeof(body)))
        check_status(platinum_atproto_error_status(body), WF_ERR_AUTH,
                     "ExpiredToken maps to auth");
    if (load_fixture("error_invalid.json", body, sizeof(body)))
        check_status(platinum_atproto_error_status(body), WF_ERR_VALIDATION,
                     "InvalidRequest maps to validation");
    if (load_fixture("error_unknown.json", body, sizeof(body)))
        check_status(platinum_atproto_error_status(body), WF_ERR_HTTP,
                     "unknown error maps to HTTP");
    if (load_fixture("not_an_error.json", body, sizeof(body)))
        check_status(platinum_atproto_error_status(body), WF_ERR_HTTP,
                     "body without an error maps to HTTP");
    check_status(platinum_atproto_error_status("not json"), WF_ERR_HTTP,
                 "non-JSON error body maps to HTTP");
    check_status(platinum_atproto_error_status(NULL), WF_ERR_HTTP,
                 "NULL error body maps to HTTP");
}

static void test_clear_wipes(void)
{
    platinum_atproto_session session;
    unsigned char *bytes;
    size_t index;

    memset(&session, 0xA5, sizeof(session));
    platinum_atproto_session_clear(&session);

    bytes = (unsigned char *)&session;
    for (index = 0; index < sizeof(session); index++) {
        if (bytes[index] != 0) {
            check(0, "clear wipes every byte");
            return;
        }
    }
    check(1, "clear wipes every byte");
}

int main(void)
{
    test_create_session();
    test_expired_token();
    test_refresh();
    test_bad_responses();
    test_error_mapping();
    test_clear_wipes();

    if (failures != 0) {
        printf("test_atproto_session: %d of %d checks failed\n", failures,
               checks);
        return 1;
    }
    printf("test_atproto_session: all %d checks passed\n", checks);
    return 0;
}
