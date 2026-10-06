/*
 * test_diag.c -- host-side tests for the Connection Status model: the health
 * check and the lines shown. Links the real diag.c and bridge client over the
 * stubbed Wolfram transport. Not Classic Mac OS 9 validation.
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

static void respond(int status, long http, const char *body)
{
    fake_status = status;
    fake_http_status = http;
    fake_body = body;
    last_url[0] = '\0';
    last_method = 0;
}

static char lines[PLATINUM_DIAG_LINES][PLATINUM_DIAG_LINE_MAX];

static void test_healthy(void)
{
    platinum_diag diag;
    platinum_bridge_client *bridge = platinum_bridge_client_new("https://bridge.example");

    respond(WF_OK, 200, "{\"ok\":true,\"service\":\"platinum-bridge\",\"version\":\"0.4.0\"}");
    check(platinum_diag_check(&diag, bridge, "https://bridge.example", 1, WF_OK, 2097152) == WF_OK,
          "a healthy bridge answers");
    check(strstr(last_url, "/health") != NULL && last_method == 1, "asks GET /health");
    check(diag.health_ok && strcmp(diag.service, "platinum-bridge") == 0 && strcmp(diag.version, "0.4.0") == 0,
          "service and version are read");
    platinum_diag_lines(&diag, lines);
    check(strcmp(lines[0], "Bridge: https://bridge.example") == 0, "bridge line");
    check(strstr(lines[1], "yes") != NULL && strstr(lines[1], "never shown") != NULL, "signed-in line");
    check(strcmp(lines[2], "Bridge health: OK, platinum-bridge 0.4.0") == 0, "health line");
    check(strstr(lines[3], "none since") != NULL, "no error yet");
    check(strcmp(lines[4], "Free memory: 2048 KB") == 0, "memory in KB");
    platinum_bridge_client_free(bridge);
}

static void test_failures(void)
{
    platinum_diag diag;
    platinum_bridge_client *bridge = platinum_bridge_client_new("https://bridge.example");

    respond(WF_ERR_NETWORK, 0, NULL);
    platinum_diag_check(&diag, bridge, "https://bridge.example", 0, WF_ERR_AUTH, -1);
    platinum_diag_lines(&diag, lines);
    check(strstr(lines[2], "failed") != NULL && strstr(lines[2], "no network answer") != NULL,
          "a network failure is described");
    check(strcmp(lines[1], "Signed in: no") == 0, "not signed in");
    check(strstr(lines[3], "sign-in was refused") != NULL, "the last error is described");
    check(strcmp(lines[4], "Free memory: unknown") == 0, "unknown memory says so");

    respond(WF_ERR_HTTP, 502, "<html>bad gateway</html>");
    platinum_diag_check(&diag, bridge, "https://bridge.example", 1, WF_OK, 0);
    platinum_diag_lines(&diag, lines);
    check(strcmp(lines[2], "Bridge health: HTTP 502") == 0, "an HTTP error shows its status");

    respond(WF_OK, 200, "<html>some other server</html>");
    platinum_diag_check(&diag, bridge, "https://bridge.example", 1, WF_OK, 0);
    platinum_diag_lines(&diag, lines);
    check(!diag.health_ok && strstr(lines[2], "not like a Platinum bridge") != NULL,
          "a 200 that is not the bridge's health reply is not called healthy");

    respond(WF_OK, 200, "{\"ok\":false}");
    platinum_diag_check(&diag, bridge, "https://bridge.example", 1, WF_OK, 0);
    check(!diag.health_ok, "ok:false is not healthy");

    platinum_bridge_client_free(bridge);
}

static void test_no_bridge_and_secrets(void)
{
    platinum_diag diag;
    static char long_url[400];

    check(platinum_diag_check(&diag, NULL, "", 0, WF_OK, 0) == WF_ERR_CONFIG, "no bridge is a config error");
    platinum_diag_lines(&diag, lines);
    check(strcmp(lines[0], "Bridge: none set") == 0 && strstr(lines[2], "no bridge is configured") != NULL,
          "it says no bridge is set");

    platinum_diag_init(&diag);
    platinum_diag_lines(&diag, lines);
    check(strstr(lines[2], "not checked yet") != NULL && strstr(lines[5], "Check Again") != NULL,
          "an unchecked snapshot says so");

    memset(long_url, 'a', 390);
    long_url[390] = '\0';
    platinum_diag_check(&diag, NULL, long_url, 1, WF_OK, 0);
    check(strlen(diag.url) == PLATINUM_DIAG_URL_MAX, "an over-long address is clipped, not overrun");
    platinum_diag_lines(&diag, lines);
    check(strlen(lines[0]) < PLATINUM_DIAG_LINE_MAX, "and so is its line");

    check(strcmp(platinum_status_text(WF_OK), "no error") == 0 &&
              platinum_status_text((wf_status)999)[0] != '\0',
          "every status has some text");
    check(platinum_diag_check(NULL, NULL, "", 0, WF_OK, 0) == WF_ERR_INVALID_ARG, "NULL is refused");
}

int main(void)
{
    test_healthy();
    test_failures();
    test_no_bridge_and_secrets();
    if (failures != 0) {
        printf("test_diag: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_diag: all %d checks passed\n", checks);
    return 0;
}
