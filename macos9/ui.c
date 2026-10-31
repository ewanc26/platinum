#include "ui.h"
#include "drawutil.h"
#include "screen_state.h"
#include "state_panel.h"
#include "timeline.h"
#include "scrollbar.h"

#include <Quickdraw.h>
#include <TextEdit.h>
#include <Windows.h>
#include <stdio.h>
#include <string.h>

static unsigned char kHome[] = { 4, 'H', 'o', 'm', 'e' };
static unsigned char kNotifications[] = {
    13, 'N', 'o', 't', 'i', 'f', 'i', 'c', 'a', 't', 'i', 'o', 'n', 's'
};
static unsigned char kProfile[] = { 7, 'P', 'r', 'o', 'f', 'i', 'l', 'e' };
static unsigned char kTimeline[] = { 8, 'T', 'i', 'm', 'e', 'l', 'i', 'n', 'e' };
static unsigned char kRefresh[] = { 7, 'R', 'e', 'f', 'r', 'e', 's', 'h' };
static unsigned char kPost[] = { 7, 'P', 'o', 's', 't', '.', '.', '.' };
static unsigned char kSelected[] = {
    13, 'S', 'e', 'l', 'e', 'c', 't', 'e', 'd', ' ', 'p', 'o', 's', 't'
};
static unsigned char kNotPaired[] = {
    35, 'C', 'o', 'n', 'n', 'e', 'c', 't', ' ', 't', 'o', ' ', 'P', 'l', 'a',
    't', 'i', 'n', 'u', 'm', ' ', 'B', 'r', 'i', 'd', 'g', 'e', ' ', 't', 'o',
    ' ', 'b', 'e', 'g', 'i', 'n'
};
static unsigned char kDetailPlaceholder[] = {
    30, 'S', 'e', 'l', 'e', 'c', 't', ' ', 'a', ' ', 'p', 'o', 's', 't', ' ',
    't', 'o', ' ', 'v', 'i', 'e', 'w', ' ', 'd', 'e', 't', 'a', 'i', 'l', 's',
    '.'
};
static unsigned char kMore[] = {
    6, 'M', 'o', 'r', 'e', '.', '.', '.'
};

static void platinum_ui_text(const char *text)
{
    if (text == NULL)
        return;
    DrawText((Ptr)text, 0, (short)strlen(text));
}

static void platinum_ui_button(const Rect *bounds, StringPtr title)
{
    long text_width;
    short baseline;

    FrameRect(bounds);
    text_width = StringWidth(title);
    baseline = bounds->top + 14;
    MoveTo(bounds->left + (short)((bounds->right - bounds->left - text_width) / 2),
           baseline);
    DrawString(title);
}

static void platinum_ui_draw_focus(const Rect *rect)
{
    Rect inner;

    inner = *rect;
    InsetRect(&inner, 2, 2);
    FrameRect(&inner);
}

static Rect platinum_ui_panel_content(const platinum_ui_layout *layout)
{
    Rect content;

    content = layout->timeline;
    InsetRect(&content, 8, 8);
    return content;
}

/*
 * The timeline is native-first but the bridge is still its fallback, so an
 * unpaired session is the one blocking state the window knows before it asks
 * the shared panel. Content always wins: a later failure keeps the rows.
 */
static void platinum_ui_build_panel(const platinum_session *session,
                                    const platinum_timeline *timeline,
                                    platinum_panel *panel)
{
    const char *message;
    int paired;

    paired = session != NULL && platinum_session_is_paired(session);
    message = platinum_timeline_status(timeline);
    if (!paired)
        message = platinum_screen_state_text(PLATINUM_SCREEN_NEEDS_SIGN_IN);

    (void)platinum_panel_for_screen(panel, message, timeline->loading,
                                    timeline->count != 0);
}

void platinum_ui_state_init(platinum_ui_state *state)
{
    if (state == NULL)
        return;

    state->navigation = 0;
    state->selected_post = 0;
    state->scroll_row = 0;
    state->show_detail = 1;
    state->focus = 0;
}

void platinum_ui_layout_compute(const Rect *content,
                                platinum_ui_layout *layout)
{
    short width;
    short height;
    short nav_width;
    short toolbar_height;
    short detail_height;
    short available;

    if (content == NULL || layout == NULL)
        return;

    width = content->right - content->left;
    height = content->bottom - content->top;
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;

    nav_width = 116;
    toolbar_height = 30;
    detail_height = 118;

    if (width < 480)
        nav_width = 96;
    if (nav_width > width)
        nav_width = width;
    if (height < 360)
        detail_height = 96;
    if (height < 300)
        detail_height = 72;
    if (toolbar_height > height)
        toolbar_height = height;

    /* The inspector is the part that gives way when the window is shorter than
     * the documented minimum: it may reach zero height, but it may not push the
     * navigation or the timeline inside out. A negative rect is a drawing bug
     * waiting to happen, so every region is kept non-negative and ordered here
     * rather than at each of its uses. */
    available = height - toolbar_height - 2;
    if (available < 0)
        available = 0;
    if (detail_height > available)
        detail_height = available;

    layout->toolbar = *content;
    layout->toolbar.bottom = layout->toolbar.top + toolbar_height;

    layout->navigation = *content;
    layout->navigation.top = layout->toolbar.bottom + 1;
    layout->navigation.right = layout->navigation.left + nav_width;
    layout->navigation.bottom = content->bottom - detail_height - 1;
    if (layout->navigation.bottom < layout->navigation.top)
        layout->navigation.bottom = layout->navigation.top;

    layout->timeline = *content;
    layout->timeline.top = layout->navigation.top;
    layout->timeline.left = layout->navigation.right + 1;
    layout->timeline.bottom = layout->navigation.bottom;

    layout->timeline_scrollbar = layout->timeline;
    layout->timeline_scrollbar.left = layout->timeline_scrollbar.right - 15;
    if (layout->timeline_scrollbar.left < layout->timeline.left)
        layout->timeline_scrollbar.left = layout->timeline.left;
    layout->timeline.right = layout->timeline_scrollbar.left - 1;
    if (layout->timeline.right < layout->timeline.left)
        layout->timeline.right = layout->timeline.left;

    layout->detail = *content;
    layout->detail.top = layout->navigation.bottom + 1;
    if (layout->detail.top > layout->detail.bottom)
        layout->detail.top = layout->detail.bottom;
}

static void platinum_ui_draw_post(const platinum_ui_layout *layout,
                                  const platinum_ui_state *state,
                                  const platinum_post_preview *post,
                                  short index)
{
    Rect row;
    short top;

    top = layout->timeline.top + 6 +
          (short)((index - state->scroll_row) * 64);

    row = layout->timeline;
    row.top = top;
    row.bottom = top + 62;

    if (index == state->selected_post) {
        InsetRect(&row, 2, 2);
        FrameRect(&row);
        InsetRect(&row, 2, 2);
    }

    MoveTo(row.left + 8, row.top + 14);
    platinum_ui_text(post->author);

    MoveTo(row.left + 120, row.top + 14);
    platinum_ui_text(post->handle);

    MoveTo(row.right - 60, row.top + 14);
    platinum_ui_text(post->time);

    MoveTo(row.left + 8, row.top + 34);
    platinum_ui_text(post->line1);

    if (post->line2[0] != 0) {
        MoveTo(row.left + 8, row.top + 48);
        platinum_ui_text(post->line2);
    }

    if (post->line3[0] != 0) {
        MoveTo(row.left + 8, row.top + 60);
        platinum_ui_text(post->line3);
    }
}

static void platinum_ui_draw_detail(WindowPtr window,
                                    const platinum_ui_layout *layout,
                                    const platinum_ui_state *state,
                                    const platinum_timeline *timeline)
{
    const platinum_post_preview *posts;
    unsigned short count;
    const platinum_post_preview *post;
    Rect button;

    posts = platinum_timeline_posts(timeline);
    count = platinum_timeline_post_count(timeline);

    if (count == 0 || state->selected_post >= (short)count) {
        MoveTo(layout->detail.left + 12, layout->detail.top + 22);
        DrawString(kSelected);
        MoveTo(layout->detail.left + 12, layout->detail.top + 46);
        DrawString(kDetailPlaceholder);
        return;
    }

    post = &posts[state->selected_post];

    MoveTo(layout->detail.left + 12, layout->detail.top + 20);
    platinum_ui_text(post->author);

    MoveTo(layout->detail.left + 12, layout->detail.top + 36);
    platinum_ui_text(post->handle);

    MoveTo(layout->detail.left + 12, layout->detail.top + 56);
    platinum_ui_text(post->line1);

    if (post->line2[0] != 0) {
        MoveTo(layout->detail.left + 12, layout->detail.top + 70);
        platinum_ui_text(post->line2);
    }

    /* State is spelt out in words, never shown by colour or a glyph alone. */
    {
        char line[96];
        sprintf(line, "%ld %s%s   %ld %s%s",
                post->like_count, post->like_count == 1 ? "like" : "likes",
                post->liked ? " (you liked it)" : "",
                post->repost_count,
                post->repost_count == 1 ? "repost" : "reposts",
                post->reposted ? " (you reposted it)" : "");
        MoveTo(layout->detail.left + 12, layout->detail.top + 90);
        platinum_ui_text(line);
    }

    button = layout->detail;
    button.left = layout->detail.right - 78;
    button.right = layout->detail.right - 10;
    button.top = layout->detail.top + 12;
    button.bottom = button.top + 20;
    platinum_ui_button(&button, kMore);
}

void platinum_ui_draw(WindowPtr window,
                      const platinum_ui_layout *layout,
                      const platinum_ui_state *state,
                      const platinum_session *session,
                      const platinum_timeline *timeline)
{
    Rect button;
    Rect divider;
    GrafPtr old_port;
    int paired;
    const platinum_post_preview *posts;
    unsigned short count;
    short first;
    short visible;
    short index;
    const char *status;
    platinum_panel panel;

    if (window == NULL || layout == NULL || state == NULL ||
        session == NULL || timeline == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)window);

    paired = platinum_session_is_paired(session);
    posts = platinum_timeline_posts(timeline);
    count = platinum_timeline_post_count(timeline);

    EraseRect(&window->portRect);
    FrameRect(&layout->toolbar);
    FrameRect(&layout->navigation);
    FrameRect(&layout->timeline);
    if (layout->detail.top < layout->detail.bottom)
        FrameRect(&layout->detail);

    divider = layout->navigation;
    divider.left = divider.right;
    MoveTo(divider.left, divider.top);
    LineTo(divider.left, divider.bottom);

    if (layout->detail.top < layout->detail.bottom) {
        divider = layout->detail;
        divider.top = layout->detail.top;
        MoveTo(divider.left, divider.top);
        LineTo(divider.right, divider.top);
    }

    TextFont(systemFont);
    TextSize(12);

    button = layout->toolbar;
    button.left += 8;
    button.top += 5;
    button.right = button.left + 82;
    button.bottom = button.top + 20;
    platinum_ui_button(&button, kHome);

    button.left = button.right + 8;
    button.right = button.left + 78;
    platinum_ui_button(&button, kRefresh);
    if (state->focus == 1)
        platinum_ui_draw_focus(&button);

    button.left = button.right + 8;
    button.right = button.left + 62;
    platinum_ui_button(&button, kPost);
    if (state->focus == 2)
        platinum_ui_draw_focus(&button);

    {
        Rect selection;
        selection = layout->navigation;
        selection.left += 4;
        selection.right -= 4;
        selection.top += 5 + (short)(state->navigation * 28);
        selection.bottom = selection.top + 24;
        FrameRect(&selection);
    }

    MoveTo(layout->navigation.left + 10, layout->navigation.top + 24);
    DrawString(kHome);
    MoveTo(layout->navigation.left + 10, layout->navigation.top + 52);
    DrawString(kNotifications);
    MoveTo(layout->navigation.left + 10, layout->navigation.top + 80);
    DrawString(kProfile);

    platinum_ui_build_panel(session, timeline, &panel);
    if (platinum_panel_is_active(&panel)) {
        Rect content;

        content = platinum_ui_panel_content(layout);
        platinum_panel_draw(&content, &panel);
        if (state->focus == 3) {
            platinum_panel_rects rects;
            char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX];
            short count_lines;

            count_lines = platinum_panel_wrap(
                panel.message, platinum_panel_columns(&content), lines,
                PLATINUM_PANEL_LINES);
            platinum_panel_rects_compute(
                &content, count_lines,
                panel.action != PLATINUM_PANEL_ACTION_NONE, &rects);
            if (rects.button_visible)
                platinum_ui_draw_focus(&rects.button);
        }
    } else {
        MoveTo(layout->timeline.left + 12, layout->timeline.top + 18);
        DrawString(kTimeline);

        first = state->scroll_row;
        visible = (layout->timeline.bottom - layout->timeline.top - 24) / 64;
        if (visible < 1)
            visible = 1;

        for (index = first;
             index < (short)count && index < first + visible;
             ++index) {
            platinum_ui_draw_post(layout, state, &posts[index], index);
        }

        status = platinum_timeline_status(timeline);
        if (count != 0 && status != NULL && status[0] != '\0')
            platinum_draw_text(status, layout->timeline.left + 12,
                               layout->timeline.bottom - 6);
        if (timeline->capped)
            platinum_draw_text(
                "Showing 40 posts; older pages replace the newest.",
                layout->timeline.left + 12, layout->timeline.bottom - 22);
    }

    if (layout->detail.top < layout->detail.bottom) {
        MoveTo(layout->detail.left + 12, layout->detail.top + 16);
        if (paired)
            platinum_ui_draw_detail(window, layout, state, timeline);
        else {
            MoveTo(layout->detail.left + 12, layout->detail.top + 46);
            DrawString(kNotPaired);
        }
    }

    SetPort(old_port);
}

int platinum_ui_handle_mouse(const platinum_ui_layout *layout,
                             platinum_ui_state *state,
                             const platinum_session *session,
                             const platinum_timeline *timeline,
                             Point where)
{
    short row;
    short content_top;
    unsigned short count;
    platinum_panel panel;
    Rect panel_content;
    int panel_action;

    if (layout == NULL || state == NULL || timeline == NULL)
        return PLATINUM_UI_ACTION_NONE;

    platinum_ui_build_panel(session, timeline, &panel);
    panel_content = platinum_ui_panel_content(layout);

    if (PtInRect(where, &layout->toolbar)) {
        if (where.h >= layout->toolbar.left + 96 &&
            where.h < layout->toolbar.left + 182)
            return PLATINUM_UI_ACTION_REFRESH;

        if (where.h >= layout->toolbar.left + 190)
            return PLATINUM_UI_ACTION_COMPOSE;
    }

    if (PtInRect(where, &layout->navigation)) {
        row = (where.v - layout->navigation.top - 6) / 28;
        if (row >= 0 && row <= 2) {
            state->navigation = row;
            state->selected_post = 0;
            state->scroll_row = 0;
            if (row == 1)
                return PLATINUM_UI_ACTION_NOTIFICATIONS;
            if (row == 2)
                return PLATINUM_UI_ACTION_PROFILE;
            return PLATINUM_UI_ACTION_NONE;
        }
    }

    if (PtInRect(where, &layout->timeline)) {
        if (platinum_panel_is_active(&panel)) {
            panel_action = platinum_panel_action_at(&panel_content, &panel,
                                                    where);
            if (panel_action == PLATINUM_PANEL_ACTION_SIGN_IN)
                return PLATINUM_UI_ACTION_SIGN_IN;
            if (panel_action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                return PLATINUM_UI_ACTION_PAIR;
            return PLATINUM_UI_ACTION_NONE;
        }

        content_top = layout->timeline.top + 6;
        /* The six points above the first row hold the "Timeline" heading.
         * Integer division would fold a click there into row zero, silently
         * selecting a post the reader did not click. */
        if (where.v >= content_top) {
            row = state->scroll_row + (where.v - content_top) / 64;
            count = platinum_timeline_post_count(timeline);
            if (row >= 0 && row < (short)count) {
                state->selected_post = row;
                state->show_detail = 1;
            }
        }
    }

    return PLATINUM_UI_ACTION_NONE;
}

int platinum_ui_handle_key(const platinum_ui_layout *layout,
                           platinum_ui_state *state,
                           const platinum_session *session,
                           const platinum_timeline *timeline,
                           EventRecord *event)
{
    unsigned char key;
    unsigned short count;
    short visible;
    platinum_panel panel;

    if (layout == NULL || state == NULL || timeline == NULL ||
        event == NULL)
        return PLATINUM_UI_ACTION_NONE;

    if (event->what != keyDown && event->what != autoKey)
        return PLATINUM_UI_ACTION_NONE;

    key = (unsigned char)(event->message & charCodeMask);
    count = platinum_timeline_post_count(timeline);
    visible = (layout->timeline.bottom - layout->timeline.top - 24) / 64;
    if (visible < 1)
        visible = 1;

    platinum_ui_build_panel(session, timeline, &panel);

    if ((event->modifiers & cmdKey) != 0) {
        if (key == 'r' || key == 'R')
            return PLATINUM_UI_ACTION_REFRESH;
        if (key == 'n' || key == 'N')
            return PLATINUM_UI_ACTION_COMPOSE;
        if (key == 'q' || key == 'Q')
            return PLATINUM_UI_ACTION_QUIT;
        if (key == 'l' || key == 'L')
            return PLATINUM_UI_ACTION_LIKE;
        if (key == 'e' || key == 'E')
            return PLATINUM_UI_ACTION_REPOST;
        if (key == 'j' || key == 'J')
            return PLATINUM_UI_ACTION_REPLY;
        if (key == 't' || key == 'T')
            return PLATINUM_UI_ACTION_THREAD;
        if (key == 'i' || key == 'I')
            return PLATINUM_UI_ACTION_AUTHOR;
        if (key == 'y' || key == 'Y')
            return PLATINUM_UI_ACTION_FOLLOW;
    }

    if (key == '\t') {
        short last;

        last = platinum_panel_is_active(&panel) ? 3 : 2;
        if ((event->modifiers & shiftKey) != 0)
            state->focus = state->focus <= 0
                               ? last
                               : (short)(state->focus - 1);
        else
            state->focus = state->focus >= last
                               ? 0
                               : (short)(state->focus + 1);
        return PLATINUM_UI_ACTION_NONE;
    }

    if (platinum_panel_key_is_return(event)) {
        if (panel.action == PLATINUM_PANEL_ACTION_SIGN_IN)
            return PLATINUM_UI_ACTION_SIGN_IN;
        if (panel.action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
            return PLATINUM_UI_ACTION_PAIR;
        if (state->focus == 2)
            return PLATINUM_UI_ACTION_COMPOSE;
        return PLATINUM_UI_ACTION_REFRESH;
    }

    switch (key) {
        case upArrow:
            if (state->selected_post > 0)
                --state->selected_post;
            if (state->selected_post < state->scroll_row)
                state->scroll_row = state->selected_post;
            break;

        case downArrow:
            /* Down from the last row asks for the next page. It is an explicit
             * keystroke, not a scroll side effect, so a fetch never starts
             * from inside scroll tracking. */
            if (state->selected_post + 1 >= (short)count &&
                platinum_timeline_has_older(timeline))
                return PLATINUM_UI_ACTION_LOAD_OLDER;
            if (state->selected_post + 1 < (short)count)
                ++state->selected_post;
            if (state->selected_post >= state->scroll_row + visible)
                state->scroll_row = state->selected_post - visible + 1;
            break;

        case leftArrow:
            if (state->scroll_row > 0)
                --state->scroll_row;
            break;

        case rightArrow:
            if (state->scroll_row + visible < (short)count)
                ++state->scroll_row;
            break;

        case pageUp:
            state->scroll_row -= visible;
            if (state->scroll_row < 0)
                state->scroll_row = 0;
            break;

        case pageDown:
            state->scroll_row += visible;
            if (state->scroll_row + visible > (short)count)
                state->scroll_row = (short)count - visible;
            if (state->scroll_row < 0)
                state->scroll_row = 0;
            break;

        default:
            break;
    }

    return PLATINUM_UI_ACTION_NONE;
}
