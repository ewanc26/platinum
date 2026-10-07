#include "apppw.h"
#include "drawutil.h"
#include "textfield.h"

#include <Quickdraw.h>
#include <string.h>

static unsigned char kTitle[] = {
    27, 'S', 'i', 'g', 'n', ' ', 'I', 'n', ' ', 'w', 'i', 't', 'h', ' ', 'A',
    'p', 'p', ' ', 'P', 'a', 's', 's', 'w', 'o', 'r', 'd'
};
static unsigned char kCancel[] = { 6, 'C', 'a', 'n', 'c', 'e', 'l' };
static unsigned char kSignIn[] = { 7, 'S', 'i', 'g', 'n', ' ', 'I', 'n' };

static void apppw_buttons(const platinum_apppw *apppw, Rect *cancel,
                          Rect *go)
{
    *cancel = apppw->window->portRect;
    cancel->left = cancel->right - 160;
    cancel->right = cancel->left + 64;
    cancel->top = cancel->bottom - 34;
    cancel->bottom -= 10;

    *go = *cancel;
    go->left = cancel->right + 8;
    go->right = go->left + 70;
}

/* The masked password box. */
static void apppw_password_rect(const platinum_apppw *apppw, Rect *rect)
{
    SetRect(rect, 18, 170,
            apppw->window->portRect.right - apppw->window->portRect.left - 18,
            192);
}

void platinum_apppw_init(platinum_apppw *apppw)
{
    if (apppw == NULL)
        return;
    platinum_secret_wipe(&apppw->password);
    memset(apppw, 0, sizeof(*apppw));
}

void platinum_apppw_set_status(platinum_apppw *apppw, const char *status)
{
    if (apppw == NULL)
        return;
    apppw->status[0] = '\0';
    if (status != NULL) {
        strncpy(apppw->status, status, sizeof(apppw->status) - 1);
        apppw->status[sizeof(apppw->status) - 1] = '\0';
    }
    if (apppw->window != NULL)
        InvalRect(&apppw->window->portRect);
}

OSErr platinum_apppw_open(platinum_apppw *apppw, const char *bridge_url)
{
    Rect bounds;
    Rect url_rect;
    Rect handle_rect;

    if (apppw == NULL)
        return paramErr;
    if (apppw->window != NULL) {
        SelectWindow(apppw->window);
        return noErr;
    }

    platinum_apppw_init(apppw);
    SetRect(&bounds, 96, 66, 640, 340);
    apppw->window = NewCWindow(NULL, &bounds, kTitle, 1, documentProc,
                               (WindowPtr)-1L, 1, 0L);
    if (apppw->window == NULL)
        return memFullErr;

    SetRect(&url_rect, 18, 38, bounds.right - bounds.left - 18, 60);
    apppw->bridge_url = platinum_textfield_new(apppw->window, &url_rect);
    SetRect(&handle_rect, 18, 104, bounds.right - bounds.left - 18, 126);
    apppw->handle = platinum_textfield_new(apppw->window, &handle_rect);
    if (apppw->bridge_url == NULL || apppw->handle == NULL) {
        platinum_apppw_close(apppw);
        return memFullErr;
    }
    if (bridge_url != NULL)
        platinum_textfield_set(apppw->bridge_url, bridge_url);

    apppw->active_field = bridge_url != NULL && bridge_url[0] != '\0' ? 1 : 0;
    platinum_apppw_set_status(apppw, "Enter your handle and an app password.");
    SetPort((GrafPtr)apppw->window);
    platinum_apppw_draw(apppw);
    TEActivate(apppw->active_field == 0 ? apppw->bridge_url : apppw->handle);
    return noErr;
}

void platinum_apppw_close(platinum_apppw *apppw)
{
    if (apppw == NULL)
        return;
    /* First, before anything can fail: the password goes. */
    platinum_secret_wipe(&apppw->password);
    if (apppw->bridge_url != NULL) {
        TEDeactivate(apppw->bridge_url);
        TEDispose(apppw->bridge_url);
        apppw->bridge_url = NULL;
    }
    if (apppw->handle != NULL) {
        TEDeactivate(apppw->handle);
        TEDispose(apppw->handle);
        apppw->handle = NULL;
    }
    if (apppw->window != NULL) {
        DisposeWindow(apppw->window);
        apppw->window = NULL;
    }
    apppw->active_field = 0;
    apppw->http_armed = 0;
    apppw->status[0] = '\0';
}

void platinum_apppw_draw(platinum_apppw *apppw)
{
    GrafPtr old_port;
    Rect frame;
    Rect cancel;
    Rect go;
    char mask[PLATINUM_SECRET_MAX + 1];
    char url[PLATINUM_APPPW_URL_MAX + 1];

    if (apppw == NULL || apppw->window == NULL || apppw->bridge_url == NULL ||
        apppw->handle == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)apppw->window);
    EraseRect(&apppw->window->portRect);

    platinum_draw_text("Bridge URL:", 18, 28);
    platinum_draw_text("Handle (for example you.bsky.social):", 18, 94);
    platinum_draw_text("App password (not your account password):", 18, 160);

    frame = (*apppw->bridge_url)->viewRect;
    FrameRect(&frame);
    TEUpdate(&frame, apppw->bridge_url);
    frame = (*apppw->handle)->viewRect;
    FrameRect(&frame);
    TEUpdate(&frame, apppw->handle);

    apppw_password_rect(apppw, &frame);
    FrameRect(&frame);
    if (platinum_secret_mask(&apppw->password, mask, sizeof(mask)) >= 0)
        platinum_draw_text(mask, frame.left + 4, frame.top + 15);
    if (apppw->active_field == 2) {
        /* No insertion point to blink: a thick bottom edge says it is active. */
        MoveTo(frame.left, frame.bottom + 1);
        LineTo(frame.right, frame.bottom + 1);
    }

    if (platinum_textfield_copy(apppw->bridge_url, url, sizeof(url)) >= 0 &&
        platinum_apppw_is_plain_http(url))
        platinum_draw_text("Warning: http:// sends the password across the network unencrypted.",
                   18, 214);
    platinum_draw_text(apppw->status, 18, 232);

    apppw_buttons(apppw, &cancel, &go);
    platinum_draw_button(&cancel, kCancel);
    platinum_draw_button(&go, kSignIn);
    SetPort(old_port);
}

static TEHandle apppw_field(const platinum_apppw *apppw)
{
    if (apppw->active_field == 0)
        return apppw->bridge_url;
    if (apppw->active_field == 1)
        return apppw->handle;
    return NULL;
}

static void apppw_focus(platinum_apppw *apppw, short field)
{
    TEHandle old_field = apppw_field(apppw);

    if (old_field != NULL)
        TEDeactivate(old_field);
    apppw->active_field = field;
    if (apppw_field(apppw) != NULL)
        TEActivate(apppw_field(apppw));
    InvalRect(&apppw->window->portRect);
}

int platinum_apppw_handle_event(platinum_apppw *apppw, EventRecord *event)
{
    Point where;
    Rect cancel;
    Rect go;
    Rect box;
    unsigned char key;
    TEHandle field;

    if (apppw == NULL || event == NULL || apppw->window == NULL)
        return PLATINUM_APPPW_NONE;

    switch (event->what) {
        case activateEvt:
            if ((WindowPtr)(long)event->message == apppw->window) {
                field = apppw_field(apppw);
                if (field != NULL) {
                    if (event->modifiers & activeFlag)
                        TEActivate(field);
                    else
                        TEDeactivate(field);
                }
            }
            return PLATINUM_APPPW_NONE;

        case updateEvt:
            if ((WindowPtr)(long)event->message == apppw->window) {
                BeginUpdate(apppw->window);
                platinum_apppw_draw(apppw);
                EndUpdate(apppw->window);
            }
            return PLATINUM_APPPW_NONE;

        case mouseDown:
            where = event->where;
            GlobalToLocal(&where);
            apppw_buttons(apppw, &cancel, &go);
            if (PtInRect(where, &cancel))
                return PLATINUM_APPPW_CANCEL;
            if (PtInRect(where, &go))
                return PLATINUM_APPPW_SIGN_IN;
            if (PtInRect(where, &(*apppw->bridge_url)->viewRect)) {
                apppw_focus(apppw, 0);
                TEClick(where, (event->modifiers & shiftKey) != 0,
                        apppw->bridge_url);
            } else if (PtInRect(where, &(*apppw->handle)->viewRect)) {
                apppw_focus(apppw, 1);
                TEClick(where, (event->modifiers & shiftKey) != 0,
                        apppw->handle);
            } else {
                apppw_password_rect(apppw, &box);
                if (PtInRect(where, &box))
                    apppw_focus(apppw, 2);
            }
            return PLATINUM_APPPW_NONE;

        case keyDown:
        case autoKey:
            key = (unsigned char)(event->message & charCodeMask);
            if ((event->modifiers & cmdKey) != 0) {
                if (key == 'w' || key == 'W')
                    return PLATINUM_APPPW_CANCEL;
                /* No other command key reaches a field, the password least. */
                return PLATINUM_APPPW_NONE;
            }
            if (key == '\r' || key == 3)
                return PLATINUM_APPPW_SIGN_IN;
            if (key == '\t') {
                apppw_focus(apppw, (short)((apppw->active_field + 1) % 3));
                return PLATINUM_APPPW_NONE;
            }
            if (apppw->active_field == 2) {
                if (platinum_secret_key(&apppw->password, key))
                    InvalRect(&apppw->window->portRect);
                return PLATINUM_APPPW_NONE;
            }
            field = apppw_field(apppw);
            if (field != NULL)
                (void)platinum_textfield_key(
                    field, event,
                    apppw->active_field == 0 ? PLATINUM_APPPW_URL_MAX
                                             : PLATINUM_APPPW_HANDLE_MAX,
                    1);
            return PLATINUM_APPPW_NONE;

        default:
            break;
    }
    return PLATINUM_APPPW_NONE;
}

OSErr platinum_apppw_get_bridge_url(const platinum_apppw *apppw, char *buffer,
                                    long capacity)
{
    if (apppw == NULL || apppw->bridge_url == NULL || buffer == NULL ||
        capacity <= 0)
        return paramErr;
    if (platinum_textfield_copy(apppw->bridge_url, buffer, capacity) < 0)
        return memFullErr;
    return buffer[0] == '\0' ? paramErr : noErr;
}

OSErr platinum_apppw_get_handle(const platinum_apppw *apppw, char *utf8,
                                long capacity)
{
    char raw[PLATINUM_APPPW_HANDLE_MAX + 1];

    if (apppw == NULL || apppw->handle == NULL || utf8 == NULL || capacity <= 0)
        return paramErr;
    if (platinum_textfield_copy(apppw->handle, raw, sizeof(raw)) < 0)
        return memFullErr;
    return platinum_apppw_prepare_handle(raw, utf8, capacity) < 0 ? paramErr
                                                                  : noErr;
}
