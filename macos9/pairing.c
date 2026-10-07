#include "pairing.h"
#include "drawutil.h"
#include "text_codec.h"
#include "textfield.h"

#include <Quickdraw.h>
#include <Memory.h>
#include <string.h>

static const char kBridgeURLLabel[] = "Bridge URL:";
static const char kInstructions1[] =
    "Open the bridge login URL in a modern browser.";
static const char kInstructions2[] =
    "Complete sign-in, then enter the six-character code shown by the bridge.";
static const char kCodeLabel[] = "Pairing Code:";
static unsigned char kPairingTitle[] = {
    23, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', 'B', 'r', 'i', 'd', 'g',
    'e', ' ', 'P', 'a', 'i', 'r', 'i', 'n', 'g'
};
static unsigned char kCancel[] = { 6, 'C', 'a', 'n', 'c', 'e', 'l' };
static unsigned char kPair[] = { 4, 'P', 'a', 'i', 'r' };

OSErr platinum_pairing_open(platinum_pairing *pairing,
                            const char *bridge_url)
{
    Rect bounds;
    Rect url_rect;
    Rect code_rect;

    if (pairing == NULL)
        return paramErr;

    if (pairing->window != NULL)
        return noErr;

    memset(pairing, 0, sizeof(*pairing));

    SetRect(&bounds, 96, 66, 640, 376);
    pairing->window = NewCWindow(NULL, &bounds, kPairingTitle, 1,
                                 documentProc, (WindowPtr)-1L, 1, 0L);
    if (pairing->window == NULL)
        return memFullErr;

    SetRect(&url_rect, 18, 50, bounds.right - bounds.left - 18, 72);
    pairing->bridge_url = platinum_textfield_new(pairing->window, &url_rect);
    if (pairing->bridge_url == NULL) {
        platinum_pairing_close(pairing);
        return memFullErr;
    }

    SetRect(&code_rect, 18, 164, 150, 188);
    pairing->code = platinum_textfield_new(pairing->window, &code_rect);
    if (pairing->code == NULL) {
        platinum_pairing_close(pairing);
        return memFullErr;
    }

    if (bridge_url != NULL)
        platinum_textfield_set(pairing->bridge_url, bridge_url);

    pairing->active_field = 0;
    platinum_pairing_set_status(pairing,
                                "Enter the bridge URL and pairing code.");
    SetPort((GrafPtr)pairing->window);
    platinum_pairing_draw(pairing);
    TEActivate(pairing->bridge_url);
    return noErr;
}

void platinum_pairing_close(platinum_pairing *pairing)
{
    if (pairing == NULL)
        return;

    if (pairing->bridge_url != NULL) {
        TEDeactivate(pairing->bridge_url);
        TEDispose(pairing->bridge_url);
        pairing->bridge_url = NULL;
    }

    if (pairing->code != NULL) {
        TEDeactivate(pairing->code);
        TEDispose(pairing->code);
        pairing->code = NULL;
    }

    if (pairing->window != NULL) {
        DisposeWindow(pairing->window);
        pairing->window = NULL;
    }

    pairing->active_field = 0;
    pairing->status[0] = '\0';
}

void platinum_pairing_draw(platinum_pairing *pairing)
{
    GrafPtr old_port;
    Rect url_frame;
    Rect code_frame;
    Rect cancel_rect;
    Rect pair_rect;

    if (pairing == NULL || pairing->window == NULL ||
        pairing->bridge_url == NULL || pairing->code == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)pairing->window);

    EraseRect(&pairing->window->portRect);

    platinum_draw_text(kBridgeURLLabel, 18, 28);
    platinum_draw_text(kInstructions1, 18, 92);
    platinum_draw_text(kInstructions2, 18, 112);
    platinum_draw_text(kCodeLabel, 18, 140);

    url_frame = (*pairing->bridge_url)->viewRect;
    FrameRect(&url_frame);
    TEUpdate(&url_frame, pairing->bridge_url);

    code_frame = (*pairing->code)->viewRect;
    FrameRect(&code_frame);
    TEUpdate(&code_frame, pairing->code);

    if (pairing->status[0] != '\0')
        platinum_draw_text(pairing->status, 18, 214);

    cancel_rect = pairing->window->portRect;
    cancel_rect.left = cancel_rect.right - 150;
    cancel_rect.right = cancel_rect.left + 64;
    cancel_rect.top = cancel_rect.bottom - 34;
    cancel_rect.bottom -= 10;
    platinum_draw_button(&cancel_rect, kCancel);

    pair_rect = cancel_rect;
    pair_rect.left = cancel_rect.right + 8;
    pair_rect.right = pair_rect.left + 52;
    platinum_draw_button(&pair_rect, kPair);

    SetPort(old_port);
}

static int pairing_point_in_field(Point where, TEHandle text)
{
    Rect rect;

    if (text == NULL)
        return 0;

    rect = (*text)->viewRect;
    return PtInRect(where, &rect) != 0;
}

int platinum_pairing_handle_event(platinum_pairing *pairing,
                                  EventRecord *event)
{
    Point where;
    Rect cancel_rect;
    Rect pair_rect;
    TEHandle target;
    unsigned char key;

    if (pairing == NULL || event == NULL || pairing->window == NULL)
        return PLATINUM_PAIRING_NONE;

    switch (event->what) {
        case activateEvt:
            if ((WindowPtr)(long)event->message == pairing->window) {
                TEHandle field;

                field = (pairing->active_field == 0) ? pairing->bridge_url
                                                     : pairing->code;
                if (event->modifiers & activeFlag)
                    TEActivate(field);
                else
                    TEDeactivate(field);
            }
            return PLATINUM_PAIRING_NONE;

        case updateEvt:
            if ((WindowPtr)(long)event->message == pairing->window) {
                BeginUpdate(pairing->window);
                platinum_pairing_draw(pairing);
                EndUpdate(pairing->window);
            }
            return PLATINUM_PAIRING_NONE;

        case mouseDown:
            where = event->where;
            GlobalToLocal(&where);

            cancel_rect = pairing->window->portRect;
            cancel_rect.left = cancel_rect.right - 150;
            cancel_rect.right = cancel_rect.left + 64;
            cancel_rect.top = cancel_rect.bottom - 34;
            cancel_rect.bottom -= 10;

            pair_rect = cancel_rect;
            pair_rect.left = cancel_rect.right + 8;
            pair_rect.right = pair_rect.left + 52;

            if (PtInRect(where, &cancel_rect))
                return PLATINUM_PAIRING_CANCEL;

            if (PtInRect(where, &pair_rect))
                return PLATINUM_PAIRING_PAIR;

            if (pairing_point_in_field(where, pairing->bridge_url)) {
                pairing->active_field = 0;
                TEClick(where, (event->modifiers & shiftKey) != 0,
                        pairing->bridge_url);
            } else if (pairing_point_in_field(where, pairing->code)) {
                pairing->active_field = 1;
                TEClick(where, (event->modifiers & shiftKey) != 0,
                        pairing->code);
            }
            return PLATINUM_PAIRING_NONE;

        case keyDown:
        case autoKey:
            key = (unsigned char)(event->message & charCodeMask);
            if ((event->modifiers & cmdKey) != 0 &&
                (key == 'w' || key == 'W')) {
                return PLATINUM_PAIRING_CANCEL;
            }

            target = pairing->active_field == 0 ?
                pairing->bridge_url : pairing->code;
            if (target != NULL)
                (void)platinum_textfield_key(
                    target, event,
                    pairing->active_field == 0 ? PLATINUM_PAIRING_URL_MAX
                                               : PLATINUM_PAIRING_CODE_MAX,
                    1);
            return PLATINUM_PAIRING_NONE;

        default:
            break;
    }

    return PLATINUM_PAIRING_NONE;
}

OSErr platinum_pairing_get_bridge_url(const platinum_pairing *pairing,
                                      char *buffer,
                                      long capacity)
{
    if (pairing == NULL || pairing->bridge_url == NULL ||
        buffer == NULL || capacity <= 0)
        return paramErr;

    if (platinum_textfield_copy(pairing->bridge_url, buffer, capacity) < 0)
        return memFullErr;

    if (buffer[0] == '\0')
        return paramErr;

    return noErr;
}

OSErr platinum_pairing_get_code(const platinum_pairing *pairing,
                                char *buffer,
                                long capacity)
{
    char raw[32];
    long input;
    long output;
    long i;
    char ch;

    if (pairing == NULL || pairing->code == NULL ||
        buffer == NULL || capacity <= 0)
        return paramErr;

    if (platinum_textfield_copy(pairing->code, raw, sizeof(raw)) < 0)
        return memFullErr;

    input = (long)strlen(raw);
    output = 0;

    for (i = 0; i < input; ++i) {
        ch = raw[i];
        if (ch == ' ' || ch == '-' || ch == '\r' || ch == '\n' ||
            ch == '	')
            continue;

        if (ch >= 'a' && ch <= 'z')
            ch = (char)(ch - 'a' + 'A');

        if (!((ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9')))
            return paramErr;

        if (output >= capacity - 1)
            return paramErr;

        buffer[output++] = ch;
    }

    buffer[output] = '\0';
    if (output != PLATINUM_PAIRING_CODE_MAX)
        return paramErr;

    return noErr;
}

void platinum_pairing_set_status(platinum_pairing *pairing,
                                 const char *status)
{
    long length;

    if (pairing == NULL)
        return;

    pairing->status[0] = '\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_PAIRING_STATUS_MAX)
        length = PLATINUM_PAIRING_STATUS_MAX;

    memcpy(pairing->status, status, (size_t)length);
    pairing->status[length] = '\0';

    if (pairing->window != NULL)
        InvalRect(&pairing->window->portRect);
}
