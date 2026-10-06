#include "compose.h"
#include "text_codec.h"

#include <Memory.h>
#include <Quickdraw.h>
#include <string.h>

static unsigned char kComposeTitle[] = {
    11, 'N', 'e', 'w', ' ', 'P', 'o', 's', 't', '.', '.', '.'
};
static unsigned char kCancel[] = {
    6, 'C', 'a', 'n', 'c', 'e', 'l'
};
static unsigned char kPost[] = {
    4, 'P', 'o', 's', 't'
};
static const char kPosting[] = "Posting...";
static char *compose_copy_handle(TEHandle text, char *buffer, long capacity)
{
    Handle handle;
    Size length;
    long copy_length;
    SignedByte state;

    if (text == NULL || buffer == NULL || capacity <= 0)
        return NULL;

    handle = TEGetText(text);
    if (handle == NULL)
        return NULL;

    length = GetHandleSize(handle);
    if (length < 0)
        return NULL;

    copy_length = (long)length;
    if (copy_length >= capacity)
        copy_length = capacity - 1;

    state = HGetState(handle);
    HLock(handle);
    if (copy_length > 0)
        memcpy(buffer, *handle, (size_t)copy_length);
    HSetState(handle, state);

    buffer[copy_length] = '\0';
    return buffer;
}

static void platinum_compose_button(const Rect *bounds, StringPtr title)
{
    long width;
    short baseline;

    FrameRect(bounds);
    width = StringWidth(title);
    baseline = bounds->top + 14;
    MoveTo(bounds->left + (short)((bounds->right - bounds->left - width) / 2),
           baseline);
    DrawString(title);
}

static void platinum_compose_text(const char *text, short x, short y)
{
    if (text == NULL)
        return;

    MoveTo(x, y);
    DrawText((Ptr)text, 0, (short)strlen(text));
}

OSErr platinum_compose_open(platinum_compose *compose)
{
    Rect bounds;
    Rect text_rect;

    if (compose == NULL)
        return paramErr;

    memset(compose, 0, sizeof(*compose));

    SetRect(&bounds, 126, 84, 594, 396);
    compose->window = NewCWindow(NULL, &bounds, kComposeTitle, 1,
                                 documentProc, (WindowPtr)-1L, 1, 0L);
    if (compose->window == NULL)
        return memFullErr;

    text_rect.left = 14;
    text_rect.top = 14;
    text_rect.right = bounds.right - bounds.left - 14;
    text_rect.bottom = bounds.bottom - bounds.top - 58;

    compose->text = TENew(&text_rect,
                         &text_rect,
                         0,
                         PLATINUM_COMPOSE_MAX_TEXT,
                         0,
                         0,
                         compose->window,
                         NULL,
                         NULL);
    if (compose->text == NULL) {
        DisposeWindow(compose->window);
        compose->window = NULL;
        return memFullErr;
    }

    TEAutoView(compose->text, 0);
    TESetSelection(compose->text, 0, 0);
    compose->status[0] = '\0';

    SetPort((GrafPtr)compose->window);
    platinum_compose_draw(compose);
    TEActivate(compose->text);

    return noErr;
}

int platinum_compose_set_reply(platinum_compose *compose,
                               const char *uri,
                               const char *cid,
                               const char *handle)
{
    size_t length;

    if (compose == NULL)
        return 0;
    compose->reply_uri[0] = '\0';
    compose->reply_cid[0] = '\0';
    compose->reply_to[0] = '\0';
    if (uri == NULL || cid == NULL || uri[0] == '\0' || cid[0] == '\0' ||
        strlen(uri) >= sizeof(compose->reply_uri) ||
        strlen(cid) >= sizeof(compose->reply_cid))
        return 0;

    strcpy(compose->reply_uri, uri);
    strcpy(compose->reply_cid, cid);
    strcpy(compose->reply_to, "Replying to ");
    if (handle != NULL && handle[0] != '\0') {
        length = strlen(compose->reply_to);
        if (handle[0] != '@')
            compose->reply_to[length++] = '@';
        strncpy(compose->reply_to + length, handle,
                sizeof(compose->reply_to) - length - 1);
        compose->reply_to[sizeof(compose->reply_to) - 1] = '\0';
    } else {
        strcpy(compose->reply_to, "Replying to a post");
    }

    if (compose->window != NULL)
        InvalRect(&compose->window->portRect);
    return 1;
}

void platinum_compose_close(platinum_compose *compose)
{
    if (compose == NULL)
        return;

    if (compose->text != NULL) {
        TEDeactivate(compose->text);
        TEDispose(compose->text);
        compose->text = NULL;
    }

    if (compose->window != NULL) {
        DisposeWindow(compose->window);
        compose->window = NULL;
    }

    compose->posting = 0;
    compose->status[0] = '\0';
}

void platinum_compose_draw(platinum_compose *compose)
{
    GrafPtr old_port;
    Rect text_frame;
    Rect cancel_rect;
    Rect post_rect;

    if (compose == NULL || compose->window == NULL || compose->text == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)compose->window);

    EraseRect(&compose->window->portRect);

    text_frame = compose->text->viewRect;
    FrameRect(&text_frame);
    TEUpdate(&compose->window->portRect, compose->text);

    if (compose->status[0] != '\0')
        platinum_compose_text(compose->status, 14,
                              compose->window->portRect.bottom - 48);

    cancel_rect = compose->window->portRect;
    cancel_rect.left = cancel_rect.right - 160;
    cancel_rect.right = cancel_rect.left + 68;
    cancel_rect.top = cancel_rect.bottom - 30;
    cancel_rect.bottom -= 8;
    platinum_compose_button(&cancel_rect, kCancel);

    post_rect = cancel_rect;
    post_rect.left = cancel_rect.right + 8;
    post_rect.right = post_rect.left + 58;
    platinum_compose_button(&post_rect, kPost);

    if (compose->reply_to[0] != '\0')
        platinum_compose_text(compose->reply_to, 14,
                              compose->window->portRect.bottom - 14);

    if (compose->posting) {
        platinum_compose_text(kPosting, 14, 28);
    }

    SetPort(old_port);
}

static int compose_point_in_text(Point where, TEHandle text)
{
    Rect rect;

    if (text == NULL)
        return 0;

    rect = text->viewRect;
    return PtInRect(where, &rect) != 0;
}

int platinum_compose_handle_event(platinum_compose *compose,
                                  EventRecord *event)
{
    Point where;
    Rect cancel_rect;
    Rect post_rect;
    KeyMap key_map;

    if (compose == NULL || event == NULL || compose->window == NULL)
        return PLATINUM_COMPOSE_NONE;

    switch (event->what) {
        case activateEvt:
            if ((WindowPtr)(long)event->message == compose->window &&
                !compose->posting) {
                if (event->modifiers & activeFlag)
                    TEActivate(compose->text);
                else
                    TEDeactivate(compose->text);
            }
            return PLATINUM_COMPOSE_NONE;

        case updateEvt:
            if ((WindowPtr)(long)event->message == compose->window) {
                BeginUpdate(compose->window);
                platinum_compose_draw(compose);
                EndUpdate(compose->window);
            }
            return PLATINUM_COMPOSE_NONE;

        case mouseDown:
            if (compose->posting)
                return PLATINUM_COMPOSE_NONE;

            where = event->where;
            GlobalToLocal(&where);

            cancel_rect = compose->window->portRect;
            cancel_rect.left = cancel_rect.right - 160;
            cancel_rect.right = cancel_rect.left + 68;
            cancel_rect.top = cancel_rect.bottom - 30;
            cancel_rect.bottom -= 8;

            post_rect = cancel_rect;
            post_rect.left = cancel_rect.right + 8;
            post_rect.right = post_rect.left + 58;

            if (PtInRect(where, &cancel_rect))
                return PLATINUM_COMPOSE_CANCEL;

            if (PtInRect(where, &post_rect))
                return PLATINUM_COMPOSE_POST;

            if (compose_point_in_text(where, compose->text))
                TEClick(where, (event->modifiers & shiftKey) != 0,
                            0,
                            0,
                        compose->text);
            return PLATINUM_COMPOSE_NONE;

        case keyDown:
        case autoKey:
            if (compose->posting)
                return PLATINUM_COMPOSE_NONE;

            if ((event->modifiers & cmdKey) != 0 &&
                ((event->message & charCodeMask) == 'w' ||
                 (event->message & charCodeMask) == 'W'))
                return PLATINUM_COMPOSE_CANCEL;

            platinum_text_key_map(key_map,
                                  (short)(event->message & charCodeMask),
                                  event->modifiers);
            TEKey(key_map, compose->text);
            return PLATINUM_COMPOSE_NONE;

        default:
            break;
    }

    return PLATINUM_COMPOSE_NONE;
}

OSErr platinum_compose_get_text(const platinum_compose *compose,
                                char *buffer,
                                long capacity)
{
    long length;

    if (compose == NULL || compose->text == NULL ||
        buffer == NULL || capacity <= 0)
        return paramErr;

    if (compose_copy_handle(compose->text, buffer, capacity) == NULL)
        return memFullErr;

    length = (long)strlen(buffer);
    if (length == 0)
        return paramErr;

    if (length > PLATINUM_COMPOSE_MAX_TEXT)
        return overrunErr;

    return noErr;
}

void platinum_compose_set_posting(platinum_compose *compose,
                                  int posting)
{
    if (compose == NULL)
        return;

    compose->posting = posting != 0;
    if (compose->posting)
        platinum_compose_set_status(compose, kPosting);
}

void platinum_compose_set_status(platinum_compose *compose,
                                 const char *status)
{
    long length;

    if (compose == NULL)
        return;

    compose->status[0] = '\0';
    if (status != NULL) {
        length = (long)strlen(status);
        if (length > PLATINUM_COMPOSE_STATUS_MAX)
            length = PLATINUM_COMPOSE_STATUS_MAX;

        memcpy(compose->status, status, (size_t)length);
        compose->status[length] = '\0';
    }

    if (compose->window != NULL)
        InvalRect(&compose->window->portRect);
}
