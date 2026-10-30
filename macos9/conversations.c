/*
 * conversations.c -- the Conversations window: the conversation list, the open
 * conversation's messages and a one-line composer. The protocol half is
 * conversations_feed.c, which is host-tested; this file is Toolbox only.
 */
#include "conversations.h"
#include "drawutil.h"
#include "scrollbar.h"

#include <Quickdraw.h>
#include <string.h>

#define ROW_HEIGHT 30
#define MESSAGE_ROW_HEIGHT 34

static unsigned char kTitle[] = {
    13, 'C', 'o', 'n', 'v', 'e', 'r', 's', 'a', 't', 'i', 'o', 'n', 's'
};
static unsigned char kSend[] = {
    4, 'S', 'e', 'n', 'd'
};
static unsigned char kOlder[] = {
    5, 'O', 'l', 'd', 'e', 'r'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};

static void list_rows(const platinum_conversations *conversations,
                      short *visible)
{
    short height;

    height = conversations->window != NULL
                 ? (short)(conversations->window->portRect.bottom - 88)
                 : 0;
    *visible = (short)(height / ROW_HEIGHT);
    if (*visible < 1)
        *visible = 1;
}

static void message_rows(const platinum_conversations *conversations,
                         short *visible)
{
    short height;

    height = conversations->window != NULL
                 ? (short)(conversations->window->portRect.bottom - 110)
                 : 0;
    *visible = (short)(height / MESSAGE_ROW_HEIGHT);
    if (*visible < 1)
        *visible = 1;
}

static Rect close_rect(const platinum_conversations *conversations)
{
    Rect rect;

    rect = conversations->window->portRect;
    rect.left = rect.right - 76;
    rect.right -= 10;
    rect.top = rect.bottom - 34;
    rect.bottom -= 10;
    return rect;
}

static Rect send_rect(const platinum_conversations *conversations)
{
    Rect rect;

    rect = close_rect(conversations);
    rect.left -= 70;
    rect.right -= 70;
    return rect;
}

static Rect older_rect(const platinum_conversations *conversations)
{
    Rect rect;

    rect = send_rect(conversations);
    rect.left -= 70;
    rect.right -= 70;
    return rect;
}

static Rect composer_rect(const platinum_conversations *conversations)
{
    Rect rect;

    rect = conversations->window->portRect;
    rect.left = 10;
    rect.right -= 236;
    rect.top = rect.bottom - 52;
    rect.bottom -= 18;
    return rect;
}

OSErr platinum_conversations_open_window(
    platinum_conversations *conversations)
{
    Rect bounds;
    Rect scrollbar_bounds;
    Rect field_bounds;

    if (conversations == NULL)
        return paramErr;

    if (conversations->window != NULL) {
        SelectWindow(conversations->window);
        return noErr;
    }

    SetRect(&bounds, 70, 50, 690, 470);
    if (platinum_window_open(&conversations->window, &bounds, kTitle) != noErr)
        return memFullErr;

    scrollbar_bounds = conversations->window->portRect;
    scrollbar_bounds.left = 250;
    scrollbar_bounds.right = 265;
    scrollbar_bounds.top = 30;
    scrollbar_bounds.bottom -= 58;
    if (platinum_scrollbar_open(&conversations->list_scrollbar,
                                conversations->window,
                                &scrollbar_bounds) != noErr) {
        DisposeWindow(conversations->window);
        conversations->window = NULL;
        return memFullErr;
    }

    scrollbar_bounds = conversations->window->portRect;
    scrollbar_bounds.left = scrollbar_bounds.right - 16;
    scrollbar_bounds.top = 30;
    scrollbar_bounds.bottom -= 58;
    if (platinum_scrollbar_open(&conversations->message_scrollbar,
                                conversations->window,
                                &scrollbar_bounds) != noErr) {
        platinum_scrollbar_close(&conversations->list_scrollbar);
        DisposeWindow(conversations->window);
        conversations->window = NULL;
        return memFullErr;
    }

    field_bounds = composer_rect(conversations);
    conversations->composer =
        platinum_textfield_new(conversations->window, &field_bounds);
    if (conversations->composer == NULL) {
        platinum_scrollbar_close(&conversations->message_scrollbar);
        platinum_scrollbar_close(&conversations->list_scrollbar);
        DisposeWindow(conversations->window);
        conversations->window = NULL;
        return memFullErr;
    }

    SetPort((GrafPtr)conversations->window);
    platinum_conversations_draw(conversations);
    SelectWindow(conversations->window);
    return noErr;
}

void platinum_conversations_close_window(
    platinum_conversations *conversations)
{
    if (conversations == NULL)
        return;

    if (conversations->composer != NULL) {
        TEDispose(conversations->composer);
        conversations->composer = NULL;
    }

    platinum_scrollbar_close(&conversations->message_scrollbar);
    platinum_scrollbar_close(&conversations->list_scrollbar);

    if (conversations->window != NULL) {
        DisposeWindow(conversations->window);
        conversations->window = NULL;
    }

    conversations->composer_focused = 0;
}

long platinum_conversations_composer_text(
    const platinum_conversations *conversations,
    char *buffer,
    long capacity)
{
    if (conversations == NULL || conversations->composer == NULL)
        return 0;

    return platinum_textfield_copy(conversations->composer, buffer, capacity);
}

void platinum_conversations_clear_composer(
    platinum_conversations *conversations)
{
    if (conversations == NULL || conversations->composer == NULL)
        return;

    platinum_textfield_set(conversations->composer, "");
}

static void conversations_draw_list(platinum_conversations *conversations)
{
    short visible;
    short row;
    short index;
    const platinum_conversation *conversation;

    list_rows(conversations, &visible);

    for (row = 0; row < visible; ++row) {
        Rect row_rect;
        short top;

        index = conversations->list_row + row;
        if (index >= (short)conversations->count)
            break;

        conversation = &conversations->items[index];
        top = 34 + (row * ROW_HEIGHT);

        /* The open conversation is boxed so the message pane has a visible
         * subject. */
        if (strcmp(conversation->id, conversations->selected_id) == 0) {
            SetRect(&row_rect, 8, top - 12, 246, top + 16);
            FrameRect(&row_rect);
        }

        platinum_draw_text(conversation->name, 12, top);
        if (conversation->unread > 0) {
            char count[16];
            long value;
            short digit;
            char reversed[16];

            value = conversation->unread;
            digit = 0;
            do {
                reversed[digit++] = (char)('0' + (value % 10));
                value /= 10;
            } while (value > 0 && digit < 15);
            {
                short out = 0;
                while (digit > 0)
                    count[out++] = reversed[--digit];
                count[out] = '\0';
            }
            platinum_draw_text(count, 208, top);
        }

        if (conversation->handle[0] != '\0')
            platinum_draw_text(conversation->handle, 12, top + 13);

        if (conversation->preview[0] != '\0')
            platinum_draw_text(conversation->preview, 12, top + 24);
    }

    platinum_scrollbar_set_range(&conversations->list_scrollbar,
                                 (short)conversations->count, visible,
                                 conversations->list_row);
}

static void conversations_draw_messages(platinum_conversations *conversations)
{
    short visible;
    short row;
    short index;
    const platinum_message *message;

    message_rows(conversations, &visible);

    if (conversations->selected_id[0] == '\0') {
        platinum_draw_text("No conversation open.", 268, 60);
    } else {
        platinum_draw_text(conversations->selected_name, 268, 44);
    }

    for (row = 0; row < visible; ++row) {
        short top;

        index = conversations->message_row + row;
        if (index >= (short)conversations->message_count)
            break;

        message = &conversations->messages[index];
        top = 62 + (row * MESSAGE_ROW_HEIGHT);

        platinum_draw_text(message->mine ? "You" : "Them", 268, top);
        platinum_draw_text(message->text, 320, top);
        platinum_draw_text(message->time, 560, top);
    }

    platinum_scrollbar_set_range(&conversations->message_scrollbar,
                                 (short)conversations->message_count, visible,
                                 conversations->message_row);
}

void platinum_conversations_draw(platinum_conversations *conversations)
{
    GrafPtr old_port;
    Rect field_bounds;
    Rect button;

    if (conversations == NULL || conversations->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)conversations->window);
    EraseRect(&conversations->window->portRect);

    platinum_draw_text("Conversations", 12, 22);
    platinum_draw_text("Messages", 268, 22);

    conversations_draw_list(conversations);
    conversations_draw_messages(conversations);

    if (conversations->status[0] != '\0')
        platinum_draw_text(conversations->status, 12,
                           conversations->window->portRect.bottom - 60);

    field_bounds = composer_rect(conversations);
    FrameRect(&field_bounds);

    button = close_rect(conversations);
    platinum_draw_button(&button, kClose);
    button = send_rect(conversations);
    platinum_draw_button(&button, kSend);
    if (platinum_conversations_has_older_messages(conversations)) {
        button = older_rect(conversations);
        platinum_draw_button(&button, kOlder);
    }

    platinum_scrollbar_draw(&conversations->list_scrollbar);
    platinum_scrollbar_draw(&conversations->message_scrollbar);

    SetPort(old_port);
}

static int conversations_click(platinum_conversations *conversations,
                               Point where)
{
    short visible;
    short row;
    Rect rect;

    rect = close_rect(conversations);
    if (PtInRect(where, &rect))
        return PLATINUM_CONVERSATIONS_CLOSE;

    rect = send_rect(conversations);
    if (PtInRect(where, &rect))
        return PLATINUM_CONVERSATIONS_SEND;

    if (platinum_conversations_has_older_messages(conversations)) {
        rect = older_rect(conversations);
        if (PtInRect(where, &rect))
            return PLATINUM_CONVERSATIONS_OLDER_MESSAGES;
    }

    rect = composer_rect(conversations);
    if (PtInRect(where, &rect)) {
        conversations->composer_focused = 1;
        if (conversations->composer != NULL)
            TEActivate(conversations->composer);
        return PLATINUM_CONVERSATIONS_NONE;
    }

    if (where.h < 250) {
        list_rows(conversations, &visible);
        if (where.v >= 22) {
            row = (short)((where.v - 22) / ROW_HEIGHT);
            if (row >= 0 && row < visible &&
                conversations->list_row + row < (short)conversations->count) {
                conversations->list_row += row;
                conversations->composer_focused = 0;
                if (conversations->composer != NULL)
                    TEDeactivate(conversations->composer);
                return PLATINUM_CONVERSATIONS_OPEN;
            }
        }
    }

    return PLATINUM_CONVERSATIONS_NONE;
}

static int conversations_key(platinum_conversations *conversations,
                             EventRecord *event)
{
    short visible;
    char code;

    code = (char)(event->message & charCodeMask);

    if ((event->modifiers & cmdKey) != 0) {
        if (code == 'w' || code == 'W')
            return PLATINUM_CONVERSATIONS_CLOSE;
        if (code == 'r' || code == 'R')
            return PLATINUM_CONVERSATIONS_REFRESH;
        if (code == 'o' || code == 'O')
            return PLATINUM_CONVERSATIONS_OLDER_MESSAGES;
        return PLATINUM_CONVERSATIONS_NONE;
    }

    if (conversations->composer_focused) {
        if (code == '\r' || code == '\n' || code == 3)
            return PLATINUM_CONVERSATIONS_SEND;
        if (conversations->composer != NULL)
            platinum_textfield_key(conversations->composer, event,
                                   PLATINUM_CONVERSATION_SEND_MAX, 1);
        return PLATINUM_CONVERSATIONS_NONE;
    }

    list_rows(conversations, &visible);

    switch ((unsigned char)code) {
        case upArrow:
            if (conversations->list_row > 0)
                --conversations->list_row;
            break;
        case downArrow:
            if (conversations->list_row + visible <
                (short)conversations->count)
                ++conversations->list_row;
            break;
        case pageUp:
            conversations->list_row -= visible;
            if (conversations->list_row < 0)
                conversations->list_row = 0;
            break;
        case pageDown:
            conversations->list_row += visible;
            if (conversations->list_row + visible >
                (short)conversations->count)
                conversations->list_row =
                    (short)conversations->count - visible;
            if (conversations->list_row < 0)
                conversations->list_row = 0;
            break;
        case '\r':
        case '\n':
        case 3:
            return PLATINUM_CONVERSATIONS_OPEN;
        default:
            break;
    }

    return PLATINUM_CONVERSATIONS_NONE;
}

int platinum_conversations_handle_event(
    platinum_conversations *conversations,
    EventRecord *event)
{
    Point where;

    if (conversations == NULL || event == NULL ||
        conversations->window == NULL)
        return PLATINUM_CONVERSATIONS_NONE;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == conversations->window) {
                BeginUpdate(conversations->window);
                platinum_conversations_draw(conversations);
                EndUpdate(conversations->window);
            }
            return PLATINUM_CONVERSATIONS_NONE;

        case activateEvt:
            if ((WindowPtr)(long)event->message == conversations->window) {
                HiliteWindow(conversations->window,
                             (event->modifiers & activeFlag) != 0);
                if (conversations->composer != NULL) {
                    if ((event->modifiers & activeFlag) != 0)
                        TEActivate(conversations->composer);
                    else
                        TEDeactivate(conversations->composer);
                }
            }
            return PLATINUM_CONVERSATIONS_NONE;

        case mouseDown:
            if (platinum_scrollbar_handle_mouse(
                    &conversations->list_scrollbar, event,
                    &conversations->list_row)) {
                InvalRect(&conversations->window->portRect);
                return PLATINUM_CONVERSATIONS_NONE;
            }
            if (platinum_scrollbar_handle_mouse(
                    &conversations->message_scrollbar, event,
                    &conversations->message_row)) {
                InvalRect(&conversations->window->portRect);
                return PLATINUM_CONVERSATIONS_NONE;
            }

            where = event->where;
            GlobalToLocal(&where);
            {
                int action = conversations_click(conversations, where);
                InvalRect(&conversations->window->portRect);
                return action;
            }

        case keyDown:
            {
                int action = conversations_key(conversations, event);
                InvalRect(&conversations->window->portRect);
                return action;
            }

        case autoKey:
            if (conversations->composer_focused &&
                conversations->composer != NULL) {
                platinum_textfield_key(conversations->composer, event,
                                       PLATINUM_CONVERSATION_SEND_MAX, 1);
                InvalRect(&conversations->window->portRect);
            }
            return PLATINUM_CONVERSATIONS_NONE;

        default:
            break;
    }

    return PLATINUM_CONVERSATIONS_NONE;
}
