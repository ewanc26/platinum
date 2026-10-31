/*
 * test_state_panel.c -- host-side tests for the shared state panel.
 * Pure layout, text and wrapping logic; drawing is Toolbox-only.
 */

#include <stdio.h>
#include <string.h>

#include "screen_state.h"
#include "state_panel.h"

/* Drawing is not checked on a host; these satisfy state_panel.c's calls so the
 * test links without the Window Manager or the rest of drawutil. */
void platinum_draw_text(const char *text, short x, short y)
{
    (void)text;
    (void)x;
    (void)y;
}

void platinum_draw_button(const Rect *bounds, StringPtr title)
{
    (void)bounds;
    (void)title;
}

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

    if (failures != 0) {
        printf("test_state_panel: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_state_panel: all %d checks passed\n", checks);
    return 0;
}
