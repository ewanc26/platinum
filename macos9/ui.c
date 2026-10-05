#include "ui.h"
#include "timeline.h"

#include <Quickdraw.h>
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
static unsigned char kPaired[] = {
    20, 'B', 'r', 'i', 'd', 'g', 'e', ' ', 'a', 'c', 'c', 'o', 'u', 'n', 't',
    ' ', 'r', 'e', 'a', 'd', 'y'
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

void platinum_ui_state_init(platinum_ui_state *state)
{
    if (state == NULL)
        return;

    state->navigation = 0;
    state->selected_post = 0;
    state->scroll_row = 0;
    state->show_detail = 1;
}

void platinum_ui_layout_compute(const Rect *content,
                                platinum_ui_layout *layout)
{
    short width;
    short height;
    short nav_width;
    short toolbar_height;
    short detail_height;

    if (content == NULL || layout == NULL)
        return;

    width = content->right - content->left;
    height = content->bottom - content->top;

    nav_width = 116;
    toolbar_height = 30;
    detail_height = 118;

    if (width < 480)
        nav_width = 96;
    if (height < 360)
        detail_height = 96;
    if (height < 300)
        detail_height = 72;

    layout->toolbar = *content;
    layout->toolbar.bottom = layout->toolbar.top + toolbar_height;

    layout->navigation = *content;
    layout->navigation.top = layout->toolbar.bottom + 1;
    layout->navigation.right = layout->navigation.left + nav_width;
    layout->navigation.bottom = content->bottom - detail_height - 1;

    layout->timeline = *content;
    layout->timeline.top = layout->toolbar.bottom + 1;
    layout->timeline.left = layout->navigation.right + 1;
    layout->timeline.bottom = content->bottom - detail_height - 1;

    layout->detail = *content;
    layout->detail.top = content->bottom - detail_height;
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

static void platinum_ui_draw_detail(GrafPtr port,
                                    const platinum_ui_layout *layout,
                                    const platinum_ui_state *state)
{
    const platinum_post_preview *posts;
    unsigned short count;
    const platinum_post_preview *post;
    Rect button;

    posts = platinum_timeline_posts();
    count = platinum_timeline_post_count();

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

    button = layout->detail;
    button.left = layout->detail.right - 78;
    button.right = layout->detail.right - 10;
    button.top = layout->detail.top + 12;
    button.bottom = button.top + 20;
    platinum_ui_button(&button, kMore);

    (void)port;
}

void platinum_ui_draw(GrafPtr port,
                      const platinum_ui_layout *layout,
                      const platinum_ui_state *state,
                      const platinum_session *session)
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
    short row_top;

    if (port == NULL || layout == NULL || state == NULL || session == NULL)
        return;

    GetPort(&old_port);
    SetPort(port);

    paired = platinum_session_is_paired(session);
    posts = platinum_timeline_posts();
    count = platinum_timeline_post_count();

    EraseRect(&port->portRect);
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

    button.left = button.right + 8;
    button.right = button.left + 62;
    platinum_ui_button(&button, kPost);

    row_top = layout->navigation.top + 12;
    (void)row_top;

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

    if (layout->detail.top < layout->detail.bottom) {
        MoveTo(layout->detail.left + 12, layout->detail.top + 16);
        if (paired)
            platinum_ui_draw_detail(port, layout, state);
        else {
            MoveTo(layout->detail.left + 12, layout->detail.top + 46);
            DrawString(kNotPaired);
        }
    }

    SetPort(old_port);
}

int platinum_ui_handle_mouse(const platinum_ui_layout *layout,
                             platinum_ui_state *state,
                             Point where)
{
    short row;
    short content_top;
    unsigned short count;

    if (layout == NULL || state == NULL)
        return PLATINUM_UI_ACTION_NONE;

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
            return PLATINUM_UI_ACTION_NONE;
        }
    }

    if (PtInRect(where, &layout->timeline)) {
        content_top = layout->timeline.top + 6;
        row = state->scroll_row +
              (where.v - content_top) / 64;
        count = platinum_timeline_post_count();
        if (row >= 0 && row < (short)count) {
            state->selected_post = row;
            state->show_detail = 1;
        }
    }

    return PLATINUM_UI_ACTION_NONE;
}

int platinum_ui_handle_key(const platinum_ui_layout *layout,
                           platinum_ui_state *state,
                           EventRecord *event)
{
    unsigned char key;
    unsigned short count;
    short visible;

    if (layout == NULL || state == NULL || event == NULL)
        return PLATINUM_UI_ACTION_NONE;

    if (event->what != keyDown && event->what != autoKey)
        return PLATINUM_UI_ACTION_NONE;

    key = (unsigned char)(event->message & charCodeMask);
    count = platinum_timeline_post_count();
    visible = (layout->timeline.bottom - layout->timeline.top - 24) / 64;
    if (visible < 1)
        visible = 1;

    if ((event->modifiers & cmdKey) != 0) {
        if (key == 'r' || key == 'R')
            return PLATINUM_UI_ACTION_REFRESH;
        if (key == 'n' || key == 'N')
            return PLATINUM_UI_ACTION_COMPOSE;
        if (key == 'q' || key == 'Q')
            return PLATINUM_UI_ACTION_QUIT;
    }

    switch (key) {
        case upArrow:
            if (state->selected_post > 0)
                --state->selected_post;
            if (state->selected_post < state->scroll_row)
                state->scroll_row = state->selected_post;
            break;

        case downArrow:
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
