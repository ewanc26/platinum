#include "diagwin.h"

#include <Quickdraw.h>
#include <string.h>

static unsigned char kTitle[] = {
    17, 'C', 'o', 'n', 'n', 'e', 'c', 't', 'i', 'o', 'n', ' ', 'S', 't', 'a',
    't', 'u', 's'
};
static unsigned char kClose[] = { 5, 'C', 'l', 'o', 's', 'e' };
static unsigned char kCheck[] = {
    11, 'C', 'h', 'e', 'c', 'k', ' ', 'A', 'g', 'a', 'i', 'n'
};

static void diagwin_buttons(const platinum_diagwin *diagwin, Rect *close,
                            Rect *check)
{
    *close = diagwin->window->portRect;
    close->left = close->right - 230;
    close->right = close->left + 60;
    close->top = close->bottom - 34;
    close->bottom -= 10;

    *check = *close;
    check->left = close->right + 8;
    check->right = check->left + 92;
}

static void diagwin_button(const Rect *bounds, StringPtr title)
{
    long width;

    FrameRect(bounds);
    width = StringWidth(title);
    MoveTo(bounds->left + (short)((bounds->right - bounds->left - width) / 2),
           bounds->top + 14);
    DrawString(title);
}

void platinum_diagwin_init(platinum_diagwin *diagwin)
{
    if (diagwin == NULL)
        return;
    memset(diagwin, 0, sizeof(*diagwin));
    platinum_diag_init(&diagwin->diag);
}

OSErr platinum_diagwin_open(platinum_diagwin *diagwin)
{
    Rect bounds;

    if (diagwin == NULL)
        return paramErr;
    if (diagwin->window != NULL) {
        SelectWindow(diagwin->window);
        return noErr;
    }
    SetRect(&bounds, 90, 80, 600, 290);
    diagwin->window = NewCWindow(NULL, &bounds, kTitle, 1, documentProc,
                                 (WindowPtr)-1L, 1, 0L);
    if (diagwin->window == NULL)
        return memFullErr;
    SetPort((GrafPtr)diagwin->window);
    platinum_diagwin_draw(diagwin);
    return noErr;
}

void platinum_diagwin_close(platinum_diagwin *diagwin)
{
    if (diagwin != NULL && diagwin->window != NULL) {
        DisposeWindow(diagwin->window);
        diagwin->window = NULL;
    }
}

void platinum_diagwin_draw(platinum_diagwin *diagwin)
{
    GrafPtr old_port;
    char lines[PLATINUM_DIAG_LINES][PLATINUM_DIAG_LINE_MAX];
    Rect close;
    Rect check;
    int i;

    if (diagwin == NULL || diagwin->window == NULL)
        return;
    GetPort(&old_port);
    SetPort((GrafPtr)diagwin->window);
    EraseRect(&diagwin->window->portRect);

    platinum_diag_lines(&diagwin->diag, lines);
    for (i = 0; i < PLATINUM_DIAG_LINES; ++i) {
        MoveTo(16, (short)(26 + i * 20));
        DrawText((Ptr)lines[i], 0, (short)strlen(lines[i]));
    }

    diagwin_buttons(diagwin, &close, &check);
    diagwin_button(&close, kClose);
    diagwin_button(&check, kCheck);
    SetPort(old_port);
}

int platinum_diagwin_handle_event(platinum_diagwin *diagwin,
                                  EventRecord *event)
{
    Point where;
    Rect close;
    Rect check;
    unsigned char key;

    if (diagwin == NULL || event == NULL || diagwin->window == NULL)
        return PLATINUM_DIAGWIN_NONE;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == diagwin->window) {
                BeginUpdate(diagwin->window);
                platinum_diagwin_draw(diagwin);
                EndUpdate(diagwin->window);
            }
            return PLATINUM_DIAGWIN_NONE;

        case mouseDown:
            where = event->where;
            GlobalToLocal(&where);
            diagwin_buttons(diagwin, &close, &check);
            if (PtInRect(where, &close))
                return PLATINUM_DIAGWIN_CLOSE;
            if (PtInRect(where, &check))
                return PLATINUM_DIAGWIN_CHECK;
            return PLATINUM_DIAGWIN_NONE;

        case keyDown:
        case autoKey:
            key = (unsigned char)(event->message & charCodeMask);
            if ((event->modifiers & cmdKey) != 0 && (key == 'w' || key == 'W'))
                return PLATINUM_DIAGWIN_CLOSE;
            return PLATINUM_DIAGWIN_NONE;

        default:
            break;
    }
    return PLATINUM_DIAGWIN_NONE;
}
