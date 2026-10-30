/* Native auth: session files, token lifecycle, cache and revoke. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "auth.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void
clean_dir(const char *dir, const char *id)
{
    char path[512];

    snprintf(path, sizeof path, "%s/tokens.txt", dir);
    unlink(path);
    if (id != NULL) {
        snprintf(path, sizeof path, "%s/session-%s.json", dir, id);
        unlink(path);
    }
    rmdir(dir);
}

static void
test_node_session(void)
{
    char tmpl[] = "/tmp/platinum-auth-XXXXXX";
    char *dir = mkdtemp(tmpl);
    platinum_bridge_config cfg;
    platinum_bridge_auth *auth;
    platinum_bridge_auth *restarted;
    char token[64];
    char id[40];
    char out[4096];
    char auth_header[128];
    char path[512];
    int rc;

    CHECK(dir != NULL);
    if (dir == NULL) return;

    memset(&cfg, 0, sizeof cfg);
    snprintf(cfg.data_dir, sizeof cfg.data_dir, "%s", dir);
    cfg.max_body_bytes = 65536;
    cfg.allow_app_password = 1;
    snprintf(cfg.version, sizeof cfg.version, "test");

    auth = platinum_auth_new(&cfg);
    CHECK(auth != NULL);
    if (auth == NULL) return;
    rc = platinum_auth_load(auth);
    CHECK(rc == 1);

    /* Malformed sign-in input is refused before any network call. */
    CHECK(platinum_auth_app_password_login(auth, "{", 1, "peer", out,
                                          sizeof out) == 400);
    CHECK(strstr(out, "invalid_json") != NULL);
    {
        static const char missing_password[] = "{\"identifier\":\"bob.test\"}";
        static const char bad_service[] =
            "{\"identifier\":\"bob.test\",\"password\":\"pw\","
            "\"service\":\"http://localhost\"}";
        CHECK(platinum_auth_app_password_login(auth, missing_password,
                                              strlen(missing_password), "peer",
                                              out, sizeof out) == 400);
        CHECK(strstr(out, "invalid_request") != NULL);
        CHECK(platinum_auth_app_password_login(auth, bad_service,
                                              strlen(bad_service), "peer", out,
                                              sizeof out) == 400);
        CHECK(strstr(out, "invalid_service") != NULL);
    }

    /* Sign-in input that cannot work is refused before any network call. */
    CHECK(platinum_auth_login_page(auth, NULL, out, sizeof out, NULL) == 400);
    CHECK(strstr(out, "invalid_handle") != NULL);
    CHECK(platinum_auth_login_page(auth, "", out, sizeof out, NULL) == 400);
    CHECK(platinum_auth_login_page(auth, "bad\nhandle", out, sizeof out, NULL) == 400);
    CHECK(platinum_auth_pair(auth, "{", 1, out, sizeof out) == 400);
    CHECK(strstr(out, "invalid_json") != NULL);
    {
        static const char bad_code[] = "{\"code\":\"AAAAAA\"}";
        CHECK(platinum_auth_pair(auth, bad_code, strlen(bad_code), out,
                                 sizeof out) == 400);
        CHECK(strstr(out, "invalid_code") != NULL);
    }

    /* A node-backed installation exercises the same token, session-file and
     * lookup path as a password installation, without a live PDS. */
    CHECK(platinum_auth_add_node_session(auth, "https://oauth.croft.click",
                                         "node-token", "bob.test", "did:plc:bob",
                                         token, id) == 0);
    CHECK(strlen(token) == 43);
    CHECK(strlen(id) == 36);
    CHECK(platinum_auth_lookup(auth, token) != NULL);
    CHECK(platinum_auth_lookup(auth, "not-a-token") == NULL);
    CHECK(platinum_auth_cached_agents(auth) == 1);

    snprintf(path, sizeof path, "%s/session-%s.json", dir, id);
    CHECK(access(path, F_OK) == 0);
    CHECK(access("/tmp", F_OK) == 0);

    /* A restart reloads the token and the node session from disk. */
    platinum_auth_free(auth);
    auth = platinum_auth_new(&cfg);
    CHECK(auth != NULL);
    if (auth == NULL) return;
    CHECK(platinum_auth_load(auth) == 0);
    CHECK(platinum_auth_lookup(auth, token) != NULL);
    CHECK(platinum_auth_cached_agents(auth) == 1);

    /* Revoke removes the token, its session and its cached agent. */
    snprintf(auth_header, sizeof auth_header, "Bearer %s", token);
    CHECK(platinum_auth_revoke(auth, auth_header, out, sizeof out) == 200);
    CHECK(strstr(out, "\"ok\":true") != NULL);
    CHECK(platinum_auth_lookup(auth, token) == NULL);
    CHECK(platinum_auth_cached_agents(auth) == 0);
    CHECK(access(path, F_OK) != 0);
    CHECK(platinum_auth_revoke(auth, auth_header, out, sizeof out) == 404);
    CHECK(platinum_auth_revoke(auth, NULL, out, sizeof out) == 401);
    platinum_auth_free(auth);

    /* A malformed store is reported, not silently replaced. */
    {
        FILE *f;
        snprintf(path, sizeof path, "%s/tokens.txt", dir);
        f = fopen(path, "wb");
        CHECK(f != NULL);
        if (f != NULL) {
            fputs("not a token store\n", f);
            fclose(f);
        }
        restarted = platinum_auth_new(&cfg);
        CHECK(restarted != NULL);
        if (restarted != NULL) {
            CHECK(platinum_auth_load(restarted) == -1);
            platinum_auth_free(restarted);
        }
    }

    clean_dir(dir, NULL);
}

static void
test_disabled(void)
{
    char tmpl[] = "/tmp/platinum-auth-off-XXXXXX";
    char *dir = mkdtemp(tmpl);
    platinum_bridge_config cfg;
    platinum_bridge_auth *auth;
    char out[512];

    CHECK(dir != NULL);
    if (dir == NULL) return;
    memset(&cfg, 0, sizeof cfg);
    snprintf(cfg.data_dir, sizeof cfg.data_dir, "%s", dir);
    cfg.max_body_bytes = 1024;
    cfg.allow_app_password = 0;
    auth = platinum_auth_new(&cfg);
    CHECK(auth != NULL);
    if (auth == NULL) return;
    CHECK(platinum_auth_app_password_login(auth, "{}", 2, "peer", out,
                                          sizeof out) == 403);
    CHECK(strstr(out, "app_password_disabled") != NULL);
    platinum_auth_free(auth);
    clean_dir(dir, NULL);
}

int
main(void)
{
    test_node_session();
    test_disabled();
    if (failures) {
        fprintf(stderr, "test_auth: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_auth: %d checks passed\n", checks);
    return 0;
}
