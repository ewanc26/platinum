/*
 * test_apppw.c -- host-side tests for the pure part of the app-password window:
 * the handle preparation and the focus-stop and Return rules. The window is
 * apppw.c. Not Classic Mac OS 9 validation.
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

    check(platinum_apppw_next_focus(PLATINUM_APPPW_FOCUS_HANDLE, 0, 0) ==
              PLATINUM_APPPW_FOCUS_PASSWORD &&
              platinum_apppw_next_focus(PLATINUM_APPPW_FOCUS_PASSWORD, 0, 0) ==
              PLATINUM_APPPW_FOCUS_CANCEL &&
              platinum_apppw_next_focus(PLATINUM_APPPW_FOCUS_CANCEL, 0, 0) ==
              PLATINUM_APPPW_FOCUS_SIGN_IN &&
              platinum_apppw_next_focus(PLATINUM_APPPW_FOCUS_SIGN_IN, 0, 0) ==
              PLATINUM_APPPW_FOCUS_HANDLE,
          "Tab moves handle, password, Cancel, Sign In and wraps");
    check(platinum_apppw_next_focus(PLATINUM_APPPW_FOCUS_HANDLE, 1, 0) ==
              PLATINUM_APPPW_FOCUS_SIGN_IN &&
              platinum_apppw_next_focus(PLATINUM_APPPW_FOCUS_SIGN_IN, 1, 0) ==
              PLATINUM_APPPW_FOCUS_CANCEL,
          "Shift-Tab moves backwards and wraps");
    check(platinum_apppw_next_focus(PLATINUM_APPPW_FOCUS_CANCEL, 0, 1) ==
              PLATINUM_APPPW_FOCUS_CANCEL &&
              platinum_apppw_next_focus(PLATINUM_APPPW_FOCUS_CANCEL, 1, 1) ==
              PLATINUM_APPPW_FOCUS_CANCEL,
          "the signed-in view has Close as its only stop");
    check(platinum_apppw_next_focus(-1, 0, 0) == PLATINUM_APPPW_FOCUS_PASSWORD &&
              platinum_apppw_next_focus(9, 1, 0) ==
                  PLATINUM_APPPW_FOCUS_SIGN_IN,
          "an out-of-range stop is treated as the handle");
    check(platinum_apppw_return_action(PLATINUM_APPPW_FOCUS_HANDLE, 0) ==
              PLATINUM_APPPW_SIGN_IN &&
              platinum_apppw_return_action(PLATINUM_APPPW_FOCUS_PASSWORD, 0) ==
              PLATINUM_APPPW_SIGN_IN,
          "Return from a field signs in");
    check(platinum_apppw_return_action(PLATINUM_APPPW_FOCUS_CANCEL, 0) ==
              PLATINUM_APPPW_CANCEL &&
              platinum_apppw_return_action(PLATINUM_APPPW_FOCUS_SIGN_IN, 0) ==
              PLATINUM_APPPW_SIGN_IN,
          "Return activates the focused button");
    check(platinum_apppw_return_action(PLATINUM_APPPW_FOCUS_SIGN_IN, 1) ==
              PLATINUM_APPPW_CANCEL,
          "Return closes the signed-in view");

    if (failures != 0) {
        printf("test_apppw: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_apppw: all %d checks passed\n", checks);
    return 0;
}
