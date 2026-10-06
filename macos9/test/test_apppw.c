/*
 * test_apppw.c -- host-side tests for the pure parts of the app-password window:
 * the plain-http check and the handle preparation. The window is apppw.c. Not
 * Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "apppw.h"

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

int main(void)
{
    char out[64];
    static char big[400];

    check(platinum_apppw_is_plain_http("http://192.168.1.2:8787") &&
              platinum_apppw_is_plain_http("HTTP://x") &&
              !platinum_apppw_is_plain_http("https://bridge.example") &&
              !platinum_apppw_is_plain_http("") && !platinum_apppw_is_plain_http(NULL) &&
              !platinum_apppw_is_plain_http("htt") && !platinum_apppw_is_plain_http(" http://x"),
          "only an address that starts http:// is plain http");

    check(platinum_apppw_prepare_handle("  @me.bsky.social  ", out, sizeof(out)) == 14 &&
              strcmp(out, "me.bsky.social") == 0,
          "spaces and one leading @ are dropped");
    check(platinum_apppw_prepare_handle("@@x", out, sizeof(out)) == 2 && strcmp(out, "@x") == 0,
          "only one @ is dropped");
    check(platinum_apppw_prepare_handle("caf\x8e.test", out, sizeof(out)) == 10 &&
              strcmp(out, "caf\xc3\xa9.test") == 0,
          "MacRoman becomes UTF-8");
    check(platinum_apppw_prepare_handle("", out, sizeof(out)) == -1 &&
              platinum_apppw_prepare_handle("   ", out, sizeof(out)) == -1 &&
              platinum_apppw_prepare_handle("@", out, sizeof(out)) == -1,
          "an empty handle is refused");
    check(platinum_apppw_prepare_handle("a\tb", out, sizeof(out)) == -1,
          "control characters are refused");
    memset(big, 'a', 300);
    big[300] = '\0';
    check(platinum_apppw_prepare_handle(big, out, sizeof(out)) == -1 &&
              platinum_apppw_prepare_handle("me.bsky.social", out, 5) == -1,
          "too long or too big for the output is refused");
    check(platinum_apppw_prepare_handle(NULL, out, sizeof(out)) == -1 &&
              platinum_apppw_prepare_handle("a", NULL, 5) == -1,
          "NULL is refused");

    if (failures != 0) {
        printf("test_apppw: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_apppw: all %d checks passed\n", checks);
    return 0;
}
