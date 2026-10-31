#include "thread.h"
#include "drawutil.h"
#include "scrollbar.h"

#include <Quickdraw.h>
#include <string.h>

#define kThreadRowHeight 46
#define kThreadIndent 12

static unsigned char kThreadTitle[] = {
    6, 'T', 'h', 'r', 'e', 'a', 'd'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};

static short thread_visible_rows(const platinum_thread *thread)
{
    short visible;

    visible = (thread->window->portRect.bottom - 78) / kThreadRowHeight;
    return visible < 1 ? 1 : visible;
}

static Rect thread_close_rect(const platinum_thread *thread)
{
    Rect rect;

    rect = thread->window->portRect;
    rect.left = rect.right - 78;
    rect.right -= 10;
    rect.top = rect.bottom - 34;
    rect.bottom -= 10;
    return rect;
}

static Rect thread_content_rect(const platinum_thread *thread)
{
    Rect rect;

    rect = thread->window->portRect;
    rect.left += 8;
    rect.right -= 8;
    rect.top += 8;
    rect.bottom -= 44;
    return rect;
}

static void thread_panel(const platinum_thread *thread, platinum_panel *panel)
{
    (void)platinum_panel_for_screen(panel, thread->status, thread->loading,
                                    thread->count != 0);
}

static void thread_draw_focus(const Rect *rect)
{
    Rect inner;

    inner = *rect;
    InsetRect(&inner, 2, 2);
    FrameRect(&inner);
}

OSErr platinum_thread_open(platinum_thread *thread)
{
    Rect bounds;
    Rect scrollbar_bounds;

    if (thread == NULL)
        return paramErr;
    if (thread->window != NULL) {
        SelectWindow(thread->window);
        return noErr;
    }

    SetRect(&bounds, 76, 64, 676, 444);
    if (platinum_window_open(&thread->window, &bounds, kThreadTitle) != noErr)
        return memFullErr;

    scrollbar_bounds = thread->window->portRect;
    scrollbar_bounds.left = scrollbar_bounds.right - 15;
    scrollbar_bounds.top = 28;
    scrollbar_bounds.bottom -= 40;
    if (platinum_scrollbar_open(&thread->scrollbar, thread->window,
                                &scrollbar_bounds) != noErr) {
        DisposeWindow(thread->window);
        thread->window = NULL;
        return memFullErr;
    }

    SetPort((GrafPtr)thread->window);
    platinum_thread_draw(thread);
    SelectWindow(thread->window);
    return noErr;
}

void platinum_thread_close(platinum_thread *thread)
{
    if (thread == NULL)
        return;
    platinum_scrollbar_close(&thread->scrollbar);
    if (thread->window != NULL) {
        DisposeWindow(thread->window);
        thread->window = NULL;
    }
}

static void thread_draw_row(const platinum_thread *thread, short index,
                            short row)
{
    const platinum_thread_post *item;
    short top;
    short indent;
    short depth;

    item = &thread->items[index];
    top = 36 + (row * kThreadRowHeight);

    /* Replies indent by depth; ancestors and the post itself sit at the left.
     * The post asked for is marked in words, not only by position. */
    depth = item->depth > 0 ? item->depth : 0;
    if (depth > 6)
        depth = 6;
    indent = (short)(12 + depth * kThreadIndent);

    platinum_draw_text(item->post.author, indent, top);
    platinum_draw_text(item->post.handle, indent + 130, top);
    platinum_draw_text(item->post.time, indent + 260, top);
    if (item->depth == 0)
        platinum_draw_text("This post", indent + 320, top);
    else if (item->depth < 0)
        platinum_draw_text("Earlier", indent + 320, top);
    platinum_draw_text(item->post.line1, indent, top + 14);
    platinum_draw_text(item->post.line2, indent, top + 28);
}

void platinum_thread_draw(platinum_thread *thread)
{
    GrafPtr old_port;
    Rect close_rect;
    platinum_panel panel;
    short index;
    short row;
    short visible;

    if (thread == NULL || thread->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)thread->window);
    EraseRect(&thread->window->portRect);

    thread_panel(thread, &panel);
    close_rect = thread_close_rect(thread);
    if (platinum_panel_is_active(&panel)) {
        Rect content;

        content = thread_content_rect(thread);
        platinum_panel_draw(&content, &panel);
        if (thread->key_focus == 2) {
            platinum_panel_rects rects;
            char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX];
            short count;

            count = platinum_panel_wrap(panel.message,
                                        platinum_panel_columns(&content),
                                        lines, PLATINUM_PANEL_LINES);
            platinum_panel_rects_compute(
                &content, count,
                panel.action != PLATINUM_PANEL_ACTION_NONE, &rects);
            if (rects.button_visible)
                thread_draw_focus(&rects.button);
        }
        platinum_draw_button(&close_rect, kClose);
        if (thread->key_focus == 1)
            thread_draw_focus(&close_rect);
        SetPort(old_port);
        return;
    }

    platinum_draw_text(thread->heading[0] != '\0' ? thread->heading : "Thread", 12, 20);

    visible = thread_visible_rows(thread);
    for (row = 0; row < visible; ++row) {
        index = thread->scroll_row + row;
        if (index >= (short)thread->count)
            break;
        thread_draw_row(thread, index, row);
    }

    if (thread->status[0] != '\0')
        platinum_draw_text(thread->status, 12,
                           thread->window->portRect.bottom - 48);

    platinum_draw_button(&close_rect, kClose);
    if (thread->key_focus == 1)
        thread_draw_focus(&close_rect);

    platinum_scrollbar_set_range(&thread->scrollbar, (short)thread->count,
                                 visible, thread->scroll_row);
    platinum_scrollbar_draw(&thread->scrollbar);
    SetPort(old_port);
}

int platinum_thread_handle_event(platinum_thread *thread, EventRecord *event)
{
    Point where;
    Rect close_rect;
    short visible;

    if (thread == NULL || event == NULL || thread->window == NULL)
        return PLATINUM_THREAD_NONE;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == thread->window) {
                BeginUpdate(thread->window);
                platinum_thread_draw(thread);
                EndUpdate(thread->window);
            }
            return PLATINUM_THREAD_NONE;

        case activateEvt:
            if ((WindowPtr)(long)event->message == thread->window)
                HiliteWindow(thread->window,
                             (event->modifiers & activeFlag) != 0);
            return PLATINUM_THREAD_NONE;

        case mouseDown:
            where = event->where;
            GlobalToLocal(&where);

            {
                platinum_panel panel;
                Rect content;
                int panel_action;

                thread_panel(thread, &panel);
                content = thread_content_rect(thread);
                panel_action = platinum_panel_action_at(&content, &panel, where);
                if (panel_action == PLATINUM_PANEL_ACTION_SIGN_IN)
                    return PLATINUM_THREAD_SIGN_IN;
                if (panel_action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                    return PLATINUM_THREAD_PAIR;
                if (platinum_panel_is_active(&panel)) {
                    close_rect = thread_close_rect(thread);
                    if (PtInRect(where, &close_rect))
                        return PLATINUM_THREAD_CLOSE;
                    return PLATINUM_THREAD_NONE;
                }
            }

            if (platinum_scrollbar_handle_mouse(&thread->scrollbar, event,
                                                &thread->scroll_row)) {
                InvalRect(&thread->window->portRect);
                return PLATINUM_THREAD_NONE;
            }
            close_rect = thread_close_rect(thread);
            if (PtInRect(where, &close_rect))
                return PLATINUM_THREAD_CLOSE;
            return PLATINUM_THREAD_NONE;

        case keyDown:
        case autoKey:
            visible = thread_visible_rows(thread);
            if (platinum_panel_key_is_cancel(event))
                return PLATINUM_THREAD_CLOSE;

            {
                platinum_panel panel;
                unsigned char key;

                thread_panel(thread, &panel);
                key = (unsigned char)(event->message & charCodeMask);
                if (key == '\t') {
                    short last = platinum_panel_is_active(&panel) ? 2 : 1;
                    if ((event->modifiers & shiftKey) != 0)
                        thread->key_focus = thread->key_focus <= 0
                                            ? last
                                            : (short)(thread->key_focus - 1);
                    else
                        thread->key_focus = thread->key_focus >= last
                                            ? 0
                                            : (short)(thread->key_focus + 1);
                    InvalRect(&thread->window->portRect);
                    return PLATINUM_THREAD_NONE;
                }
                if (platinum_panel_key_is_return(event)) {
                    if (panel.action == PLATINUM_PANEL_ACTION_SIGN_IN)
                        return PLATINUM_THREAD_SIGN_IN;
                    if (panel.action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                        return PLATINUM_THREAD_PAIR;
                    if (thread->key_focus == 1)
                        return PLATINUM_THREAD_CLOSE;
                }
            }

            if ((event->modifiers & cmdKey) != 0 &&
                ((event->message & charCodeMask) == 'w' ||
                 (event->message & charCodeMask) == 'W'))
                return PLATINUM_THREAD_CLOSE;

            switch (event->message & charCodeMask) {
                case upArrow:
                    if (thread->scroll_row > 0)
                        --thread->scroll_row;
                    break;
                case downArrow:
                    /* Down at the bottom of a paged list asks for the next page. */
                    if (thread->scroll_row + visible >= (short)thread->count &&
                        platinum_thread_has_more(thread))
                        return PLATINUM_THREAD_LOAD_MORE;
                    if (thread->scroll_row + visible < (short)thread->count)
                        ++thread->scroll_row;
                    break;
                case pageUp:
                    thread->scroll_row -= visible;
                    if (thread->scroll_row < 0)
                        thread->scroll_row = 0;
                    break;
                case pageDown:
                    thread->scroll_row += visible;
                    if (thread->scroll_row + visible > (short)thread->count)
                        thread->scroll_row = (short)thread->count - visible;
                    if (thread->scroll_row < 0)
                        thread->scroll_row = 0;
                    break;
                default:
                    break;
            }
            InvalRect(&thread->window->portRect);
            return PLATINUM_THREAD_NONE;

        default:
            break;
    }
    return PLATINUM_THREAD_NONE;
}

void platinum_thread_set_status(platinum_thread *thread, const char *status)
{
    long length;

    if (thread == NULL)
        return;

    thread->status[0] = '\0';
    if (status != NULL) {
        length = (long)strlen(status);
        if (length > PLATINUM_THREAD_STATUS_MAX)
            length = PLATINUM_THREAD_STATUS_MAX;
        memcpy(thread->status, status, (size_t)length);
        thread->status[length] = '\0';
    }
    if (thread->window != NULL)
        InvalRect(&thread->window->portRect);
}
