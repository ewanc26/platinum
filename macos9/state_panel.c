/*
 * state_panel.c -- see state_panel.h.
 *
 * Text decisions, wrapping and rectangle arithmetic are pure C89 so they can be
 * host-tested; only platinum_panel_draw touches QuickDraw, and only through the
 * same helpers every window already uses.
 */

#include "state_panel.h"

#include "drawutil.h"
#include "screen_state.h"

#include <string.h>

#define kPanelInset 12
#define kPanelPadding 12
#define kPanelTitleHeight 18
#define kPanelLineHeight 16
#define kPanelGap 8
#define kPanelButtonHeight 24
#define kPanelButtonWidth 132
#define kPanelCharWidth 7

int platinum_panel_from_message(const char *message, platinum_panel *panel)
{
    if (panel == NULL) return 0;
    memset(panel, 0, sizeof *panel);
    if (message == NULL) return 0;

    if (strcmp(message,
               platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_SIGN_IN)) == 0) {
        panel->kind = PLATINUM_PANEL_NEEDS_SIGN_IN;
        panel->title = "Sign in to Platinum";
        panel->message = "This screen needs your account. Sign in with your "
                         "handle and an app password to load it.";
        panel->action = PLATINUM_PANEL_ACTION_SIGN_IN;
        panel->action_title = "Sign In";
        return 1;
    }

    if (strcmp(message,
               platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_BRIDGE)) == 0) {
        panel->kind = PLATINUM_PANEL_NEEDS_BRIDGE;
        panel->title = "This screen needs the bridge";
        panel->message = "This screen has not moved to the native client yet. "
                         "Pair a bridge under File > Pair Account to use it.";
        panel->action = PLATINUM_PANEL_ACTION_PAIR_BRIDGE;
        panel->action_title = "Pair a Bridge";
        return 1;
    }

    return 0;
}

void platinum_panel_set(platinum_panel *panel, int kind, const char *message)
{
    if (panel == NULL) return;
    memset(panel, 0, sizeof *panel);
    panel->kind = kind;
    panel->message = message;
    panel->action = PLATINUM_PANEL_ACTION_NONE;

    if (kind == PLATINUM_PANEL_EMPTY) {
        panel->title = "Nothing here yet";
    } else if (kind == PLATINUM_PANEL_LOADING) {
        panel->title = "Loading";
    } else if (kind == PLATINUM_PANEL_FAILED) {
        panel->title = "That did not work";
    }
}

static void append_line(char *line, short capacity, const char *start,
                        short length)
{
    short copy = length;
    if (copy > (short)(capacity - 1)) copy = (short)(capacity - 1);
    if (copy > 0) memcpy(line, start, (size_t)copy);
    line[copy] = '\0';
}

int platinum_panel_wrap(const char *message, short columns,
                        char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX],
                        int max_lines)
{
    int count = 0;
    short used = 0;
    const char *cursor;

    if (lines == NULL || columns <= 0 || max_lines <= 0 || message == NULL)
        return 0;
    if (max_lines > PLATINUM_PANEL_LINES) max_lines = PLATINUM_PANEL_LINES;
    lines[0][0] = '\0';

    cursor = message;
    while (*cursor != '\0' && count < max_lines) {
        const char *word;
        short word_length;

        while (*cursor == ' ') cursor++;
        if (*cursor == '\0') break;
        word = cursor;
        while (*cursor != '\0' && *cursor != ' ') cursor++;
        word_length = (short)(cursor - word);

        if (used > 0 && (short)(used + 1 + word_length) > columns) {
            lines[count][used] = '\0';
            count++;
            used = 0;
            if (count >= max_lines) break;
        }

        while (used == 0 && word_length > columns) {
            /* A word longer than the line fills it and continues below. */
            append_line(lines[count], PLATINUM_PANEL_LINE_MAX, word, columns);
            count++;
            if (count >= max_lines) return count;
            word += columns;
            word_length = (short)(word_length - columns);
        }
        if (word_length <= 0) continue;

        if (used > 0) lines[count][used++] = ' ';
        append_line(lines[count] + used, (short)(PLATINUM_PANEL_LINE_MAX - used),
                    word, word_length);
        used = (short)(used + word_length);
    }

    if (used > 0 && count < max_lines) {
        lines[count][used] = '\0';
        count++;
    }
    return count;
}

static void zero_rect(Rect *rect)
{
    SetRect(rect, 0, 0, 0, 0);
}

void platinum_panel_rects_compute(const Rect *content, short lines,
                                  int has_button,
                                  platinum_panel_rects *out)
{
    short width, height, panel_width, panel_height, needed, left, top;
    short button_width;

    if (out == NULL) return;
    memset(out, 0, sizeof *out);
    zero_rect(&out->panel);
    zero_rect(&out->title);
    zero_rect(&out->message);
    zero_rect(&out->button);
    if (lines < 0) lines = 0;
    if (lines > PLATINUM_PANEL_LINES) lines = PLATINUM_PANEL_LINES;

    if (content == NULL) return;
    width = (short)(content->right - content->left);
    height = (short)(content->bottom - content->top);
    if (width <= 0 || height <= 0) return;

    panel_width = (short)(width - 2 * kPanelInset);
    if (panel_width > 360) panel_width = 360;
    if (panel_width < 120) panel_width = width;
    if (panel_width <= 0) return;

    needed = (short)(2 * kPanelPadding + kPanelTitleHeight + kPanelGap +
                     lines * kPanelLineHeight);
    if (has_button)
        needed = (short)(needed + kPanelGap + kPanelButtonHeight);
    panel_height = needed;
    if (panel_height > (short)(height - 2 * kPanelInset))
        panel_height = (short)(height - 2 * kPanelInset);
    if (panel_height <= 0) return;

    left = (short)(content->left + (width - panel_width) / 2);
    top = (short)(content->top + (height - panel_height) / 2);
    SetRect(&out->panel, left, top, (short)(left + panel_width),
            (short)(top + panel_height));

    SetRect(&out->title, (short)(left + kPanelPadding),
            (short)(top + kPanelPadding),
            (short)(left + panel_width - kPanelPadding),
            (short)(top + kPanelPadding + kPanelTitleHeight));
    SetRect(&out->message, out->title.left,
            (short)(out->title.bottom + kPanelGap), out->title.right,
            (short)(out->title.bottom + kPanelGap + lines * kPanelLineHeight));

    out->lines = lines;
    if (out->message.bottom > (short)(out->panel.bottom - kPanelPadding))
        out->message.bottom = (short)(out->panel.bottom - kPanelPadding);

    button_width = (short)(panel_width - 2 * kPanelPadding);
    if (button_width > kPanelButtonWidth) button_width = kPanelButtonWidth;
    if (has_button &&
        out->panel.bottom - kPanelPadding - kPanelButtonHeight >=
            out->message.bottom + kPanelGap) {
        short button_left = (short)(left + (panel_width - button_width) / 2);
        SetRect(&out->button, button_left,
                (short)(out->panel.bottom - kPanelPadding - kPanelButtonHeight),
                (short)(button_left + button_width),
                (short)(out->panel.bottom - kPanelPadding));
        out->button_visible = 1;
    }
}

void platinum_panel_draw(const Rect *content, const platinum_panel *panel)
{
    platinum_panel_rects rects;
    char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX];
    short columns, i, count;

    if (content == NULL || panel == NULL || panel->kind == PLATINUM_PANEL_NONE)
        return;

    columns = (short)((360 - 2 * kPanelPadding) / kPanelCharWidth);
    count = platinum_panel_wrap(panel->message, columns, lines,
                                PLATINUM_PANEL_LINES);
    platinum_panel_rects_compute(content, count,
                                 panel->action != PLATINUM_PANEL_ACTION_NONE,
                                 &rects);
    if (rects.panel.right <= rects.panel.left) return;

    FrameRect(&rects.panel);

    platinum_draw_text(panel->title, (short)(rects.title.left + 2),
                       (short)(rects.title.top + 13));

    for (i = 0; i < count; i++) {
        platinum_draw_text(lines[i], (short)(rects.message.left + 2),
                           (short)(rects.message.top + 13 +
                                   i * kPanelLineHeight));
    }

    if (rects.button_visible && panel->action_title != NULL) {
        unsigned char label[64];
        size_t length = strlen(panel->action_title);
        if (length > sizeof label - 1) length = sizeof label - 1;
        label[0] = (unsigned char)length;
        if (length > 0) memcpy(label + 1, panel->action_title, length);
        platinum_draw_button(&rects.button, label);
    }
}
