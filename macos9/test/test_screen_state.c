/*
 * test_screen_state.c -- host-side tests for the screen availability rules.
 * Pure logic; no Toolbox, no transport. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "screen_state.h"

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
    const char *text;

    check(platinum_screen_native_state(0, 0) == PLATINUM_SCREEN_NEEDS_SIGN_IN,
          "a native screen with no session asks for sign-in");
    check(platinum_screen_native_state(1, 0) == PLATINUM_SCREEN_READY,
          "a native session is enough for a native screen");
    check(platinum_screen_native_state(0, 1) == PLATINUM_SCREEN_READY,
          "a bridge session still serves a native screen");
    check(platinum_screen_native_state(1, 1) == PLATINUM_SCREEN_READY,
          "both sessions are fine for a native screen");

    check(platinum_screen_bridge_state(0, 0) == PLATINUM_SCREEN_NEEDS_SIGN_IN,
          "a bridge screen with no session asks for sign-in");
    check(platinum_screen_bridge_state(1, 0) == PLATINUM_SCREEN_NEEDS_BRIDGE,
          "a native session alone does not serve a bridge screen");
    check(platinum_screen_bridge_state(0, 1) == PLATINUM_SCREEN_READY,
          "a bridge serves a bridge screen");
    check(platinum_screen_bridge_state(1, 1) == PLATINUM_SCREEN_READY,
          "both sessions are fine for a bridge screen");

    text = platinum_screen_state_text(PLATINUM_SCREEN_READY);
    check(text != NULL && text[0] == '\0', "ready has no text");

    text = platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_SIGN_IN);
    check(text != NULL && strstr(text, "Sign in") != NULL &&
              strstr(text, "File > Sign In") != NULL,
          "the sign-in text names the menu choice");

    text = platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_BRIDGE);
    check(text != NULL && strstr(text, "bridge") != NULL &&
              strstr(text, "File > Pair Account") != NULL,
          "the bridge text says what to do");

    text = platinum_screen_state_text((platinum_screen_state)99);
    check(text != NULL && text[0] != '\0',
          "an unknown state still says something");

    if (failures != 0) {
        printf("test_screen_state: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_screen_state: all %d checks passed\n", checks);
    return 0;
}
