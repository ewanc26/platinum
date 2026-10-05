#include "preferences.h"

#include <Quickdraw.h>
#include <string.h>

static unsigned char kPreferencesTitle[] = {
    13, 'P', 'r', 'e', 'f', 'e', 'r', 'e', 'n', 'c', 'e', 's', '.', '.'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};
static unsigned char kPair[] = {
    4, 'P', 'a', 'i', 'r'
};
static unsigned char kSignOut[] = {
    8, 'S', 'i', 'g', 'n', ' ', 'O', 'u', 't'
};

static void preferences_text(const char *text, short x, short y)
{
    if (text == NULL || text[0] == '\0')
        return;

    MoveTo(x, y);
    DrawText((Ptr)text, 0, (short)strlen(text));
}

static void preferences_button(const Rect *bounds,
                               StringPtr title)
{
    long width;
    short baseline;

    FrameRect(bounds);
    width = StringWidth(title);
    baseline = bounds->top + 14;
    MoveTo(bounds->left +
               (short)((bounds->right - bounds->left - width) / 2),
           baseline);
    DrawString(title);
}

void platinum_preferences_init(platinum_preferences *preferences)
{
    if (preferences == NULL)
        return;

    memset(preferences, 0, sizeof(*preferences));
}

OSErr platinum_preferences_open(platinum_preferences *preferences,
                                const platinum_session *session)
{
    Rect bounds;

    if (preferences == NULL || session == NULL)
        return paramErr;

    if (preferences->window != NULL) {
        SelectWindow(preferences->window);
        return noErr;
    }

    preferences->session = session;
    preferences->status[0] = '\0';

    SetRect(&bounds, 128, 92, 608, 386);
    preferences->window = NewCWindow(NULL, &bounds, kPreferencesTitle, 1,
                                     documentProc, (WindowPtr)-1L, 1, 0L);
    if (preferences->window == NULL) {
        preferences->session = NULL;
        return memFullErr;
    }

    SetPort((GrafPtr)preferences->window);
    platinum_preferences_draw(preferences);
    SelectWindow(preferences->window);
    return noErr;
}

void platinum_preferences_close(platinum_preferences *preferences)
{
    if (preferences == NULL)
        return;

    if (preferences->window != NULL) {
        DisposeWindow(preferences->window);
        preferences->window = NULL;
    }

    preferences->session = NULL;
}

void platinum_preferences_set_status(platinum_preferences *preferences,
                                      const char *status)
{
    long length;

    if (preferences == NULL)
        return;

    preferences->status[0] = '\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_PREFERENCES_STATUS_MAX)
        length = PLATINUM_PREFERENCES_STATUS_MAX;

    memcpy(preferences->status, status, (size_t)length);
    preferences->status[length] = '\0';

    if (preferences->window != NULL)
        InvalRect(&preferences->window->portRect);
}

void platinum_preferences_draw(platinum_preferences *preferences)
{
    GrafPtr old_port;
    Rect close_rect;
    Rect sign_out_rect;
    Rect pair_rect;
    const platinum_config *config;

    if (preferences == NULL || preferences->window == NULL ||
        preferences->session == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)preferences->window);
    EraseRect(&preferences->window->portRect);

    config = platinum_session_config(preferences->session);

    preferences_text("Account", 16, 28);
    if (config != NULL && platinum_config_is_paired(config)) {
        preferences_text("Paired", 120, 28);
        preferences_text("Bridge URL:", 16, 56);
        preferences_text(config->bridge_url, 120, 56);
        preferences_text("DID:", 16, 84);
        preferences_text(config->did, 120, 84);
        preferences_text("Installation:", 16, 112);
        preferences_text(config->installation_id, 120, 112);
    } else {
        preferences_text("Not paired", 120, 28);
        preferences_text("No account is currently paired.",
                         16, 60);
    }

    if (preferences->status[0] != '\0')
        preferences_text(preferences->status, 16, 148);

    pair_rect = preferences->window->portRect;
    pair_rect.left = pair_rect.right - 258;
    pair_rect.right = pair_rect.left + 54;
    pair_rect.top = pair_rect.bottom - 34;
    pair_rect.bottom -= 10;
    preferences_button(&pair_rect, kPair);

    sign_out_rect = preferences->window->portRect;
    sign_out_rect.left = pair_rect.right + 8;
    sign_out_rect.right = sign_out_rect.left + 76;
    sign_out_rect.top = sign_out_rect.bottom - 34;
    sign_out_rect.bottom -= 10;
    preferences_button(&sign_out_rect, kSignOut);

    close_rect = sign_out_rect;
    close_rect.left = sign_out_rect.right + 8;
    close_rect.right = close_rect.left + 62;
    preferences_button(&close_rect, kClose);

    SetPort(old_port);
}

int platinum_preferences_handle_event(platinum_preferences *preferences,
                                       EventRecord *event)
{
    Point where;
    Rect sign_out_rect;
    Rect close_rect;
    Rect pair_rect;

    if (preferences == NULL || event == NULL ||
        preferences->window == NULL)
        return PLATINUM_PREFERENCES_NONE;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == preferences->window) {
                BeginUpdate(preferences->window);
                platinum_preferences_draw(preferences);
                EndUpdate(preferences->window);
            }
            return PLATINUM_PREFERENCES_NONE;

        case activateEvt:
            if ((WindowPtr)(long)event->message == preferences->window)
                HiliteWindow(preferences->window);
            return PLATINUM_PREFERENCES_NONE;

        case mouseDown:
            where = event->where;
            GlobalToLocal(&where);

            pair_rect = preferences->window->portRect;
            pair_rect.left = pair_rect.right - 258;
            pair_rect.right = pair_rect.left + 54;
            pair_rect.top = pair_rect.bottom - 34;
            pair_rect.bottom -= 10;

            sign_out_rect = preferences->window->portRect;
            sign_out_rect.left = pair_rect.right + 8;
            sign_out_rect.right = sign_out_rect.left + 76;
            sign_out_rect.top = sign_out_rect.bottom - 34;
            sign_out_rect.bottom -= 10;

            close_rect = sign_out_rect;
            close_rect.left = sign_out_rect.right + 8;
            close_rect.right = close_rect.left + 62;

            if (PtInRect(where, &pair_rect))
                return PLATINUM_PREFERENCES_PAIR;

            if (PtInRect(where, &sign_out_rect)) {
                if (platinum_session_is_paired(preferences->session))
                    return PLATINUM_PREFERENCES_SIGN_OUT;
                platinum_preferences_set_status(
                    preferences,
                    "No account is paired.");
                return PLATINUM_PREFERENCES_NONE;
            }

            if (PtInRect(where, &close_rect))
                return PLATINUM_PREFERENCES_CLOSE;
            return PLATINUM_PREFERENCES_NONE;

        case keyDown:
        case autoKey:
            if ((event->modifiers & cmdKey) != 0 &&
                ((event->message & charCodeMask) == 'w' ||
                 (event->message & charCodeMask) == 'W'))
                return PLATINUM_PREFERENCES_CLOSE;
            return PLATINUM_PREFERENCES_NONE;

        default:
            break;
    }

    return PLATINUM_PREFERENCES_NONE;
}
