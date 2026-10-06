#include "thread.h"
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

static void thread_text(const char *text, short x, short y)
{
    if (text == NULL || text[0] == '\0')
        return;
    MoveTo(x, y);
    DrawText((Ptr)text, 0, (short)strlen(text));
}

static void thread_button(const Rect *bounds, StringPtr title)
{
    long width;

    FrameRect(bounds);
    width = StringWidth(title);
    MoveTo(bounds->left + (short)((bounds->right - bounds->left - width) / 2),
           bounds->top + 14);
    DrawString(title);
}

static short thread_visible_rows(const platinum_thread *thread)
{
    short visible;

    visible = (thread->window->portRect.bottom - 78) / kThreadRowHeight;
    return visible < 1 ? 1 : visible;
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
    thread->window = NewCWindow(NULL, &bounds, kThreadTitle, 1,
                                documentProc, (WindowPtr)-1L, 1, 0L);
    if (thread->window == NULL)
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

    thread_text(item->post.author, indent, top);
    thread_text(item->post.handle, indent + 130, top);
    thread_text(item->post.time, indent + 260, top);
    if (item->depth == 0)
        thread_text("This post", indent + 320, top);
    else if (item->depth < 0)
        thread_text("Earlier", indent + 320, top);
    thread_text(item->post.line1, indent, top + 14);
    thread_text(item->post.line2, indent, top + 28);
}

void platinum_thread_draw(platinum_thread *thread)
{
    GrafPtr old_port;
    Rect close_rect;
    short index;
    short row;
    short visible;

    if (thread == NULL || thread->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)thread->window);
    EraseRect(&thread->window->portRect);

    thread_text("Thread", 12, 20);
    if (thread->status[0] != '\0')
        thread_text(thread->status, 80, 20);

    visible = thread_visible_rows(thread);
    for (row = 0; row < visible; ++row) {
        index = thread->scroll_row + row;
        if (index >= (short)thread->count)
            break;
        thread_draw_row(thread, index, row);
    }

    close_rect = thread->window->portRect;
    close_rect.left = close_rect.right - 78;
    close_rect.right -= 10;
    close_rect.top = close_rect.bottom - 34;
    close_rect.bottom -= 10;
    thread_button(&close_rect, kClose);

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
            if (platinum_scrollbar_handle_mouse(&thread->scrollbar, event,
                                                &thread->scroll_row)) {
                InvalRect(&thread->window->portRect);
                return PLATINUM_THREAD_NONE;
            }
            where = event->where;
            GlobalToLocal(&where);
            close_rect = thread->window->portRect;
            close_rect.left = close_rect.right - 78;
            close_rect.right -= 10;
            close_rect.top = close_rect.bottom - 34;
            close_rect.bottom -= 10;
            if (PtInRect(where, &close_rect))
                return PLATINUM_THREAD_CLOSE;
            return PLATINUM_THREAD_NONE;

        case keyDown:
        case autoKey:
            visible = thread_visible_rows(thread);
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
