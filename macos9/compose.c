#include "compose.h"

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
static unsigned char kCountPrefix[] = {
    0
};

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

OSErr platinum_compose_open(platinum_compose *compose)
{
    Rect bounds;
    Rect text_rect;
    OSErr err;

    if (compose == NULL)
        return paramErr;

    memset(compose, 0, sizeof(*compose));

    SetRect(&bounds, 126, 84, 594, 396);
    compose->window = NewCWindow(NULL, bounds, kComposeTitle, true,
                                 documentProc, (WindowPtr)-1L, true, 0L);
    if (compose->window == NULL)
        return memFullErr;

    text_rect.left = 14;
    text_rect.top = 14;
    text_rect.right = bounds.right - bounds.left - 14;
    text_rect.bottom = bounds.bottom - bounds.top - 58;

    compose->text = TENew(&text_rect, &text_rect);
    if (compose->text == NULL) {
        DisposeWindow(compose->window);
        compose->window = NULL;
        return memFullErr;
    }

    TEAutoView(true, compose->text);
    TESetSelect(0, 0, compose->text);

    SetPort((GrafPtr)compose->window);
    platinum_compose_draw(compose);
    TEActivate(compose->text);

    err = noErr;
    return err;
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

    MoveTo(14, compose->window->portRect.bottom - 12);
    DrawString(kCountPrefix);

    SetPort(old_port);
}

int platinum_compose_handle_event(platinum_compose *compose,
                                  EventRecord *event)
{
    Point where;
    Rect cancel_rect;
    Rect post_rect;

    if (compose == NULL || event == NULL || compose->window == NULL)
        return PLATINUM_COMPOSE_NONE;

    switch (event->what) {
        case activateEvt:
            if ((WindowPtr)event->message == compose->window)
                TEActivate(compose->text);
            return PLATINUM_COMPOSE_NONE;

        case updateEvt:
            if ((WindowPtr)event->message == compose->window) {
                BeginUpdate(compose->window);
                platinum_compose_draw(compose);
                EndUpdate(compose->window);
            }
            return PLATINUM_COMPOSE_NONE;

        case mouseDown:
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

            TEClick(where, (event->modifiers & shiftKey) != 0,
                    compose->text);
            return PLATINUM_COMPOSE_NONE;

        case keyDown:
        case autoKey:
            if ((event->modifiers & cmdKey) != 0 &&
                ((event->message & charCodeMask) == 'w' ||
                 (event->message & charCodeMask) == 'W')) {
                return PLATINUM_COMPOSE_CANCEL;
            }

            TEKey((short)(event->message & charCodeMask),
                  compose->text);
            return PLATINUM_COMPOSE_NONE;

        default:
            break;
    }

    return PLATINUM_COMPOSE_NONE;
}
