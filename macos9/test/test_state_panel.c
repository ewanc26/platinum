/*
 * test_state_panel.c -- host-side tests for the shared state panel.
 * Pure layout, text and wrapping logic; drawing is Toolbox-only.
 */

#include <stdio.h>
#include <string.h>

#include "screen_state.h"
#include "state_panel.h"

/* Drawing is not checked on a host; ui_stubs.c satisfies state_panel.c's
 * calls so the test links without the Window Manager or the rest of drawutil. */

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

static int inside(const Rect *inner, const Rect *outer)
{
    return inner->left >= outer->left && inner->right <= outer->right &&
           inner->top >= outer->top && inner->bottom <= outer->bottom;
}

int main(void)
{
    platinum_panel panel;
    platinum_panel_rects rects;
    Rect content;
    char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX];
    int count, i;

    /* The two screen-state sentences become panels, with an action. */
    check(platinum_panel_from_message(
              platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_SIGN_IN),
              &panel) == 1,
          "the sign-in sentence becomes a panel");
    check(panel.kind == PLATINUM_PANEL_NEEDS_SIGN_IN, "the sign-in kind is set");
    check(panel.action == PLATINUM_PANEL_ACTION_SIGN_IN, "sign-in offers Sign In");
    check(panel.action_title != NULL && strcmp(panel.action_title, "Sign In") == 0,
          "the sign-in button is labelled");
    check(panel.title != NULL && panel.title[0] != '\0', "the sign-in panel has a title");
    check(panel.message != NULL && strstr(panel.message, "handle") != NULL,
          "the sign-in panel says what it needs");

    check(platinum_panel_from_message(
              platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_BRIDGE),
              &panel) == 1,
          "the bridge sentence becomes a panel");
    check(panel.kind == PLATINUM_PANEL_NEEDS_BRIDGE, "the bridge kind is set");
    check(panel.action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE,
          "the bridge panel offers Pair a Bridge");
    check(panel.action_title != NULL &&
              strstr(panel.action_title, "Pair") != NULL,
          "the bridge button is labelled");
    check(panel.message != NULL &&
              strstr(panel.message, "File > Pair Account") != NULL,
          "the bridge panel names the menu choice");

    check(platinum_panel_from_message("Loading posts...", &panel) == 0,
          "ordinary status text is not a state panel");
    check(panel.kind == PLATINUM_PANEL_NONE, "a refused message leaves no panel");
    check(platinum_panel_from_message(NULL, &panel) == 0, "a NULL message is refused");
    check(platinum_panel_from_message("", &panel) == 0, "an empty message is refused");

    platinum_panel_set(&panel, PLATINUM_PANEL_EMPTY, "No posts yet.");
    check(panel.kind == PLATINUM_PANEL_EMPTY, "an empty panel kind is kept");
    check(panel.title != NULL && panel.title[0] != '\0', "an empty panel has a title");
    check(panel.action == PLATINUM_PANEL_ACTION_NONE, "an empty panel has no action");
    check(platinum_panel_is_active(&panel), "an empty panel is active");
    check(!platinum_panel_is_active(NULL), "no panel is not active");

    /* Ordinary screens: content wins over every panel; otherwise loading,
     * empty and failed are told apart. */
    check(platinum_panel_for_screen(&panel, "No posts to show.", 0, 0) == 1,
          "an empty list gets a panel");
    check(panel.kind == PLATINUM_PANEL_EMPTY, "an empty sentence is an empty panel");
    check(platinum_panel_for_screen(&panel, "Loading thread...", 1, 0) == 1,
          "a loading list gets a panel");
    check(panel.kind == PLATINUM_PANEL_LOADING, "a loading sentence is a loading panel");
    check(platinum_panel_for_screen(&panel, "The thread could not be loaded.",
                                    0, 0) == 1,
          "a failed list gets a panel");
    check(panel.kind == PLATINUM_PANEL_FAILED, "a failed sentence is a failed panel");
    check(platinum_panel_for_screen(&panel, "No posts to show.", 0, 1) == 0,
          "content keeps its rows and no panel");
    check(panel.kind == PLATINUM_PANEL_NONE, "content leaves no panel");
    check(platinum_panel_for_screen(
              &panel,
              platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_SIGN_IN),
              0, 0) == 1,
          "a sign-in sentence still becomes its own panel");
    check(platinum_panel_for_screen(
              &panel,
              platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_SIGN_IN),
              0, 1) == 0,
          "content keeps its rows even when the session is missing");
    check(platinum_panel_message_is_empty("The timeline is empty."),
          "an empty timeline sentence is recognised");
    check(platinum_panel_message_is_empty("Nobody to show."),
          "an empty people sentence is recognised");
    check(!platinum_panel_message_is_empty("The thread could not be loaded."),
          "a failure sentence is not empty");

    /* Wrapping. */
    count = platinum_panel_wrap("short message", 40, lines, PLATINUM_PANEL_LINES);
    check(count == 1 && strcmp(lines[0], "short message") == 0,
          "a short message is one line");

    count = platinum_panel_wrap("one two three four five six seven eight", 12,
                                lines, PLATINUM_PANEL_LINES);
    check(count > 1, "a long message wraps");
    for (i = 0; i < count; i++)
        check((int)strlen(lines[i]) <= 12, "a wrapped line fits its column");
    check(strstr(lines[0], "one two") != NULL, "wrapping keeps word order");

    count = platinum_panel_wrap("  spaced   out  ", 40, lines, PLATINUM_PANEL_LINES);
    check(count == 1 && strcmp(lines[0], "spaced out") == 0,
          "wrapping collapses runs of spaces");

    count = platinum_panel_wrap("abcdefghij", 4, lines, PLATINUM_PANEL_LINES);
    check(count >= 2, "a word longer than the line is split");
    check(strlen(lines[0]) == 4, "the split uses the full column width");

    count = platinum_panel_wrap("", 20, lines, PLATINUM_PANEL_LINES);
    check(count == 0, "an empty message has no lines");
    count = platinum_panel_wrap("text", 0, lines, PLATINUM_PANEL_LINES);
    check(count == 0, "a zero column count has no lines");

    /* Layout at the documented sizes. */
    SetRect(&content, 0, 0, 640, 480);
    platinum_panel_rects_compute(&content, 2, 1, &rects);
    check(rects.panel.right > rects.panel.left && rects.panel.bottom > rects.panel.top,
          "640x480 gives a non-empty panel");
    check(inside(&rects.panel, &content), "640x480 panel stays inside the content");
    check(inside(&rects.title, &rects.panel), "640x480 title stays inside the panel");
    check(inside(&rects.message, &rects.panel), "640x480 message stays inside the panel");
    check(rects.title.bottom <= rects.message.top, "the title is above the message");
    check(rects.button_visible, "640x480 has room for the button");
    check(inside(&rects.button, &rects.panel), "640x480 button stays inside the panel");
    check(rects.message.bottom <= rects.button.top, "the message is above the button");
    check(rects.button.right > rects.button.left, "the button has a width");

    SetRect(&content, 0, 0, 800, 600);
    platinum_panel_rects_compute(&content, 3, 1, &rects);
    check(inside(&rects.panel, &content) && rects.button_visible,
          "800x600 lays out the panel and button");

    SetRect(&content, 0, 0, 220, 200);
    platinum_panel_rects_compute(&content, 3, 1, &rects);
    check(inside(&rects.panel, &content), "a narrow content rect keeps the panel inside");
    check(rects.panel.right > rects.panel.left, "a narrow content rect still has a panel");
    if (rects.button_visible)
        check(inside(&rects.button, &rects.panel), "a narrow button stays inside the panel");

    SetRect(&content, 0, 0, 320, 70);
    platinum_panel_rects_compute(&content, 2, 1, &rects);
    check(inside(&rects.panel, &content), "a short content rect keeps the panel inside");
    check(!rects.button_visible, "a short content rect drops the button instead of clipping it");
    check(rects.button.bottom == rects.button.top, "a hidden button has no height");

    SetRect(&content, 0, 0, 0, 0);
    platinum_panel_rects_compute(&content, 2, 1, &rects);
    check(rects.panel.right == rects.panel.left && rects.panel.bottom == rects.panel.top,
          "an empty content rect yields no panel");

    platinum_panel_rects_compute(NULL, 2, 1, &rects);
    check(rects.panel.right == rects.panel.left, "a NULL content rect yields no panel");

    /* Narrow content must wrap inside the panel rather than at a fixed 48. */
    SetRect(&content, 0, 0, 220, 240);
    check(platinum_panel_columns(&content) > 0 &&
              platinum_panel_columns(&content) < 48,
          "a narrow content rect gets fewer columns");
    platinum_panel_set(&panel, PLATINUM_PANEL_EMPTY,
                       "A message that is long enough to need more than one line "
                       "in a narrow window.");
    count = platinum_panel_wrap(panel.message,
                                platinum_panel_columns(&content), lines,
                                PLATINUM_PANEL_LINES);
    check(count > 1, "the narrow panel wraps its message");
    for (i = 0; i < count; i++)
        check((int)strlen(lines[i]) <=
                  (int)platinum_panel_columns(&content),
              "a narrow wrapped line fits its columns");

    /* The action button is where the helper says it is. */
    SetRect(&content, 0, 0, 640, 480);
    platinum_panel_set(&panel, PLATINUM_PANEL_EMPTY, "No posts to show.");
    panel.action = PLATINUM_PANEL_ACTION_SIGN_IN;
    panel.action_title = "Sign In";
    count = platinum_panel_wrap(panel.message,
                                platinum_panel_columns(&content), lines,
                                PLATINUM_PANEL_LINES);
    platinum_panel_rects_compute(&content, count, 1, &rects);
    check(rects.button_visible, "the action button is laid out");
    {
        Point on_button;
        Point away;
        on_button.h = (short)(rects.button.left + 2);
        on_button.v = (short)(rects.button.top + 2);
        away.h = (short)(content.right - 2);
        away.v = (short)(content.bottom - 2);
        check(platinum_panel_action_at(&content, &panel, on_button) ==
                  PLATINUM_PANEL_ACTION_SIGN_IN,
              "a click in the button returns its action");
        check(platinum_panel_action_at(&content, &panel, away) ==
                  PLATINUM_PANEL_ACTION_NONE,
              "a click outside the button returns no action");
        panel.action = PLATINUM_PANEL_ACTION_NONE;
        check(platinum_panel_action_at(&content, &panel, on_button) ==
                  PLATINUM_PANEL_ACTION_NONE,
              "a panel with no action has no hit target");
    }

    /* Return, Escape and Command-period. */
    {
        EventRecord event;
        memset(&event, 0, sizeof event);
        event.what = keyDown;
        event.message = '\r';
        check(platinum_panel_key_is_return(&event), "Return is the default action");
        event.message = 3;
        check(platinum_panel_key_is_return(&event), "Enter is the default action");
        event.modifiers = cmdKey;
        check(!platinum_panel_key_is_return(&event),
              "Command-Return is not the default action");
        event.modifiers = 0;
        event.message = 27;
        check(platinum_panel_key_is_cancel(&event), "Escape cancels");
        event.message = '.';
        event.modifiers = cmdKey;
        check(platinum_panel_key_is_cancel(&event), "Command-period cancels");
        event.modifiers = 0;
        check(!platinum_panel_key_is_cancel(&event),
              "a bare period does not cancel");
    }

    if (failures != 0) {
        printf("test_state_panel: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_state_panel: all %d checks passed\n", checks);
    return 0;
}
