#ifndef PLATINUM_STATE_PANEL_H
#define PLATINUM_STATE_PANEL_H

/*
 * One empty-and-state panel for every Platinum window.
 *
 * A window with nothing to show says why in the same framed panel, at the same
 * place, with the same kind of action button: sign in, or pair the bridge for a
 * screen that has not migrated yet. The panel is drawn with the existing
 * platinum_draw_text and platinum_draw_button helpers, so it looks like the rest
 * of the application rather than like an error dialog.
 *
 * The text, wrapping, action and rectangle arithmetic are pure and host-tested.
 * Drawing is Toolbox-only.
 */

#include <MacTypes.h>
#include <Quickdraw.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_PANEL_LINES 3
#define PLATINUM_PANEL_LINE_MAX 96

enum {
    PLATINUM_PANEL_NONE = 0,
    PLATINUM_PANEL_NEEDS_SIGN_IN = 1,
    PLATINUM_PANEL_NEEDS_BRIDGE = 2,
    PLATINUM_PANEL_EMPTY = 3,
    PLATINUM_PANEL_LOADING = 4,
    PLATINUM_PANEL_FAILED = 5
};

enum {
    PLATINUM_PANEL_ACTION_NONE = 0,
    PLATINUM_PANEL_ACTION_SIGN_IN = 1,
    PLATINUM_PANEL_ACTION_PAIR_BRIDGE = 2
};

typedef struct platinum_panel {
    int kind;
    const char *title;
    const char *message;
    int action;
    const char *action_title;
} platinum_panel;

typedef struct platinum_panel_rects {
    Rect panel;
    Rect title;
    Rect message;
    Rect button;
    short lines;
    int button_visible;
} platinum_panel_rects;

/* Fill `panel` for one of the screen-state sentences from screen_state.c.
 * Returns 1 when the message is one of those sentences (so the caller can draw
 * the panel instead of a bare status line), 0 for any other message. The
 * comparison is exact on purpose: screen_state.c stays the single source of the
 * sentences, and this module only gives them a shape. */
int platinum_panel_from_message(const char *message, platinum_panel *panel);

/* Fill a panel for an ordinary empty, loading or failed screen. `message` is
 * copied by pointer, not by value: it must outlive the panel. */
void platinum_panel_set(platinum_panel *panel, int kind, const char *message);

/* Wrap `message` at spaces into at most `max_lines` lines of at most `columns`
 * characters. A word longer than a line is split. Runs of spaces collapse.
 * Returns the line count (0 for no message or columns <= 0). The buffers are
 * always NUL-terminated and never overrun. */
int platinum_panel_wrap(const char *message, short columns,
                        char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX],
                        int max_lines);

/* Centre a panel in `content`. `lines` is the wrapped message line count. A
 * button is laid out only when there is room for it; `button_visible` says
 * whether it is. All rectangles are non-negative and inside `content` (an
 * empty or inverted content rect yields zeroed rectangles). */
void platinum_panel_rects_compute(const Rect *content, short lines,
                                  int has_button,
                                  platinum_panel_rects *out);

/* Draw the panel in the current port, using the panel's own text. Does nothing
 * for PLATINUM_PANEL_NONE. The caller restores the port. */
void platinum_panel_draw(const Rect *content, const platinum_panel *panel);

#ifdef __cplusplus
}
#endif

#endif /* PLATINUM_STATE_PANEL_H */
