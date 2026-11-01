#include "help.h"
#include "drawutil.h"
#include "state_panel.h"

#include <Quickdraw.h>
#include <string.h>

static unsigned char kAboutTitle[] = {
    15, 'A', 'b', 'o', 'u', 't', ' ', 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm'
};
static unsigned char kGuideTitle[] = {
    13, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', 'H', 'e', 'l', 'p'
};
static unsigned char kClose[] = { 5, 'C', 'l', 'o', 's', 'e' };

static const char *kAboutLines[PLATINUM_HELP_ABOUT_LINES] = {
    "Platinum",
    "A Bluesky client for Classic Mac OS 9.",
    "Native interface by Ewan Croft.",
    "Modern authentication and protocol work",
    "are provided by the Platinum Bridge.",
    "Early development."
};

void platinum_help_close(platinum_help *help);

static const char *kGuideLines[PLATINUM_HELP_GUIDE_LINES] = {
    "Keyboard",
    "Command-R Refresh, Command-N New post",
    "Command-K Pair account, Command-W Close",
    "Command-T Thread, Command-F Search",
    "Command-L Like, Command-E Repost",
    "Command-J Reply, Command-I Author",
    "Command-Y Follow, Command-Q Quit",
    "Up / Down move, Return activates",
    "Escape closes, Tab moves focus",
    "Account",
    "Use File > Pair Account to sign in through",
    "a modern browser. Preferences can revoke",
    "the bridge token and start pairing again.",
    "Windows",
    "Home, Notifications, Profile, People,",
    "Conversations and Search are separate",
    "native document windows. Drag the title bar",
    "to move a window and use the close box",
    "to dismiss it. Connection Status lives",
    "under Help when the bridge misbehaves.",
    "Shortcuts match the menus beside them."
};

short platinum_help_line_count(short kind)
{
    if (kind == PLATINUM_HELP_ABOUT)
        return PLATINUM_HELP_ABOUT_LINES;
    if (kind == PLATINUM_HELP_GUIDE)
        return PLATINUM_HELP_GUIDE_LINES;
    return 0;
}

const char *platinum_help_line(short kind, short index)
{
    if (kind == PLATINUM_HELP_ABOUT) {
        if (index < 0 || index >= PLATINUM_HELP_ABOUT_LINES)
            return NULL;
        return kAboutLines[index];
    }
    if (kind == PLATINUM_HELP_GUIDE) {
        if (index < 0 || index >= PLATINUM_HELP_GUIDE_LINES)
            return NULL;
        return kGuideLines[index];
    }
    return NULL;
}

void platinum_help_close_rect(const Rect *port, Rect *close)
{
    if (port == NULL || close == NULL)
        return;
    *close = *port;
    close->left = close->right - 68;
    close->right -= 10;
    close->top = close->bottom - 34;
    close->bottom -= 10;
}

int platinum_help_close_at(const Rect *port, Point where)
{
    Rect close;

    if (port == NULL)
        return 0;
    platinum_help_close_rect(port, &close);
    return PtInRect(where, &close) ? 1 : 0;
}

static void help_draw_focus(const Rect *rect)
{
    Rect inner;

    inner = *rect;
    InsetRect(&inner, 2, 2);
    FrameRect(&inner);
}

void platinum_help_init(platinum_help *help)
{
    if (help == NULL)
        return;
    memset(help, 0, sizeof(*help));
}

OSErr platinum_help_open(platinum_help *help, short kind)
{
    Rect bounds;

    if (help == NULL)
        return paramErr;
    if (kind != PLATINUM_HELP_ABOUT && kind != PLATINUM_HELP_GUIDE)
        return paramErr;
    if (help->window != NULL) {
        if (help->kind == kind) {
            InvalRect(&help->window->portRect);
            SelectWindow(help->window);
            return noErr;
        }
        platinum_help_close(help);
    }
    if (kind == PLATINUM_HELP_ABOUT)
        SetRect(&bounds, 150, 110, 566, 330);
    else
        SetRect(&bounds, 92, 62, 648, 430);
    if (platinum_window_open(&help->window, &bounds,
                             kind == PLATINUM_HELP_ABOUT ? kAboutTitle
                                                         : kGuideTitle) != noErr)
        return memFullErr;
    help->kind = kind;
    help->focus = 0;
    SetPort((GrafPtr)help->window);
    platinum_help_draw(help);
    SelectWindow(help->window);
    return noErr;
}

void platinum_help_close(platinum_help *help)
{
    if (help != NULL && help->window != NULL) {
        DisposeWindow(help->window);
        help->window = NULL;
    }
    if (help != NULL)
        help->kind = 0;
}

void platinum_help_draw(platinum_help *help)
{
    GrafPtr old_port;
    Rect close;
    short count;
    short i;
    short step;

    if (help == NULL || help->window == NULL)
        return;
    GetPort(&old_port);
    SetPort((GrafPtr)help->window);
    EraseRect(&help->window->portRect);

    count = platinum_help_line_count(help->kind);
    step = help->kind == PLATINUM_HELP_ABOUT ? 18 : 14;
    for (i = 0; i < count; ++i) {
        const char *line;

        line = platinum_help_line(help->kind, i);
        if (line != NULL)
            platinum_draw_text(line, 18, (short)(30 + i * step));
    }

    platinum_help_close_rect(&help->window->portRect, &close);
    platinum_draw_button(&close, kClose);
    if (help->focus == 1)
        help_draw_focus(&close);
    SetPort(old_port);
}

int platinum_help_handle_event(platinum_help *help, EventRecord *event)
{
    Point where;
    unsigned char key;

    if (help == NULL || event == NULL || help->window == NULL)
        return PLATINUM_HELP_NONE;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == help->window) {
                BeginUpdate(help->window);
                platinum_help_draw(help);
                EndUpdate(help->window);
            }
            return PLATINUM_HELP_NONE;

        case mouseDown:
            where = event->where;
            GlobalToLocal(&where);
            if (platinum_help_close_at(&help->window->portRect, where))
                return PLATINUM_HELP_CLOSE;
            return PLATINUM_HELP_NONE;

        case keyDown:
        case autoKey:
            if (platinum_panel_key_is_cancel(event))
                return PLATINUM_HELP_CLOSE;
            if (platinum_panel_key_is_return(event))
                return PLATINUM_HELP_CLOSE;
            key = (unsigned char)(event->message & charCodeMask);
            if (key == '\t') {
                help->focus = help->focus == 1 ? 0 : 1;
                InvalRect(&help->window->portRect);
                return PLATINUM_HELP_NONE;
            }
            if ((event->modifiers & cmdKey) != 0 && (key == 'w' || key == 'W'))
                return PLATINUM_HELP_CLOSE;
            return PLATINUM_HELP_NONE;

        default:
            break;
    }
    return PLATINUM_HELP_NONE;
}
