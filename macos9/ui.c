#include "ui.h"

#include <Quickdraw.h>
#include <string.h>

static unsigned char kHome[] = { 4, 'H', 'o', 'm', 'e' };
static unsigned char kNotifications[] = {
    13, 'N', 'o', 't', 'i', 'f', 'i', 'c', 'a', 't', 'i', 'o', 'n', 's'
};
static unsigned char kProfile[] = { 7, 'P', 'r', 'o', 'f', 'i', 'l', 'e' };
static unsigned char kTimeline[] = { 8, 'T', 'i', 'm', 'e', 'l', 'i', 'n', 'e' };
static unsigned char kRefresh[] = { 7, 'R', 'e', 'f', 'r', 'e', 's', 'h' };
static unsigned char kPost[] = { 6, 'P', 'o', 's', 't', 0x85, 0 };
static unsigned char kSelected[] = {
    14, 'S', 'e', 'l', 'e', 'c', 't', 'e', 'd', ' ', 'p', 'o', 's', 't'
};
static unsigned char kPaired[] = {
    16, 'B', 'r', 'i', 'd', 'g', 'e', ' ', 'a', 'c', 'c', 'o', 'u', 'n', 't', ' ', 'r', 'e', 'a', 'd', 'y'
};
static unsigned char kNotPaired[] = {
    34, 'C', 'o', 'n', 'n', 'e', 'c', 't', ' ', 't', 'o', ' ', 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', 'B', 'r', 'i', 'd', 'g', 'e', ' ', 't', 'o', ' ', 'b', 'e', 'g', 'i', 'n'
};
static unsigned char kTimelinePlaceholder[] = {
    27, 'T', 'i', 'm', 'e', 'l', 'i', 'n', 'e', ' ', 'd', 'a', 't', 'a', ' ', 'w', 'i', 'l', 'l', ' ', 'a', 'p', 'p', 'e', 'a', 'r', ' ', 'h', 'e', 'r', 'e', '.'
};
static unsigned char kDetailPlaceholder[] = {
    31, 'S', 'e', 'l', 'e', 'c', 't', ' ', 'a', ' ', 'p', 'o', 's', 't', ' ', 't', 'o', ' ', 'v', 'i', 'e', 'w', ' ', 'd', 'e', 't', 'a', 'i', 'l', 's', '.'
};

static void platinum_ui_button(const Rect *bounds, StringPtr title)
{
    Rect text_rect;
    long text_width;

    FrameRect(bounds);
    text_width = StringWidth(title);
    text_rect.left = bounds->left + (short)((bounds->right - bounds->left - text_width) / 2);
    text_rect.right = text_rect.left + (short)text_width;
    text_rect.top = bounds->top + 4;
    text_rect.bottom = text_rect.top + 14;
    MoveTo(text_rect.left, text_rect.bottom);
    DrawString(title);
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

void platinum_ui_draw(GrafPtr port,
                      const platinum_ui_layout *layout,
                      const platinum_session *session)
{
    Rect button;
    Rect divider;
    GrafPtr old_port;
    int paired;

    if (port == NULL || layout == NULL || session == NULL)
        return;

    GetPort(&old_port);
    SetPort(port);

    paired = platinum_session_is_paired(session);

    EraseRect(&port->portRect);

    FrameRect(&layout->toolbar);
    FrameRect(&layout->navigation);
    FrameRect(&layout->timeline);
    FrameRect(&layout->detail);

    divider = layout->navigation;
    divider.left = divider.right;
    MoveTo(divider.left, divider.top);
    LineTo(divider.left, divider.bottom);

    divider = layout->detail;
    divider.top = layout->detail.top;
    MoveTo(divider.left, divider.top);
    LineTo(divider.right, divider.top);

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

    MoveTo(layout->navigation.left + 10, layout->navigation.top + 24);
    DrawString(kHome);
    MoveTo(layout->navigation.left + 10, layout->navigation.top + 52);
    DrawString(kNotifications);
    MoveTo(layout->navigation.left + 10, layout->navigation.top + 80);
    DrawString(kProfile);

    MoveTo(layout->timeline.left + 12, layout->timeline.top + 24);
    DrawString(kTimeline);

    MoveTo(layout->timeline.left + 12, layout->timeline.top + 52);
    DrawString(kTimelinePlaceholder);

    MoveTo(layout->detail.left + 12, layout->detail.top + 22);
    DrawString(kSelected);

    MoveTo(layout->detail.left + 12, layout->detail.top + 46);
    if (paired)
        DrawString(kPaired);
    else
        DrawString(kNotPaired);

    MoveTo(layout->detail.left + 12, layout->detail.top + 72);
    DrawString(kDetailPlaceholder);

    SetPort(old_port);
}
