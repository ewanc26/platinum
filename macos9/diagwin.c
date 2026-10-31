#include "diagwin.h"
#include "drawutil.h"

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

static Rect diagwin_content(const platinum_diagwin *diagwin)
{
    Rect content;

    content = diagwin->window->portRect;
    InsetRect(&content, 8, 8);
    content.bottom -= 32;
    return content;
}

static void diagwin_panel(const platinum_diagwin *diagwin,
                          platinum_panel *panel)
{
    (void)platinum_panel_from_message(diagwin->status, panel);
}

static void diagwin_draw_focus(const Rect *rect)
{
    Rect inner;

    inner = *rect;
    InsetRect(&inner, 2, 2);
    FrameRect(&inner);
}

void platinum_diagwin_set_status(platinum_diagwin *diagwin,
                                 const char *status)
{
    if (diagwin == NULL)
        return;
    diagwin->status[0] = '\0';
    if (status != NULL) {
        strncpy(diagwin->status, status, PLATINUM_DIAGWIN_STATUS_MAX);
        diagwin->status[PLATINUM_DIAGWIN_STATUS_MAX] = '\0';
    }
    if (diagwin->window != NULL)
        InvalRect(&diagwin->window->portRect);
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
    if (platinum_window_open(&diagwin->window, &bounds, kTitle) != noErr)
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
    platinum_panel panel;
    int i;

    if (diagwin == NULL || diagwin->window == NULL)
        return;
    GetPort(&old_port);
    SetPort((GrafPtr)diagwin->window);
    EraseRect(&diagwin->window->portRect);

    diagwin_panel(diagwin, &panel);
    if (platinum_panel_is_active(&panel)) {
        Rect content;

        content = diagwin_content(diagwin);
        platinum_panel_draw(&content, &panel);
        if (diagwin->focus == 3) {
            platinum_panel_rects rects;
            char wrapped[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX];
            short count;

            count = platinum_panel_wrap(panel.message,
                                        platinum_panel_columns(&content),
                                        wrapped, PLATINUM_PANEL_LINES);
            platinum_panel_rects_compute(
                &content, count,
                panel.action != PLATINUM_PANEL_ACTION_NONE, &rects);
            if (rects.button_visible)
                diagwin_draw_focus(&rects.button);
        }
    } else {
        platinum_diag_lines(&diagwin->diag, lines);
        for (i = 0; i < PLATINUM_DIAG_LINES; ++i) {
            MoveTo(16, (short)(26 + i * 20));
            DrawText((Ptr)lines[i], 0, (short)strlen(lines[i]));
        }
    }

    diagwin_buttons(diagwin, &close, &check);
    platinum_draw_button(&close, kClose);
    if (diagwin->focus == 1)
        diagwin_draw_focus(&close);
    platinum_draw_button(&check, kCheck);
    if (diagwin->focus == 2)
        diagwin_draw_focus(&check);
    SetPort(old_port);
}

int platinum_diagwin_handle_event(platinum_diagwin *diagwin,
                                  EventRecord *event)
{
    Point where;
    Rect close;
    Rect check;
    unsigned char key;
    platinum_panel panel;
    Rect content;
    int panel_action;

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
            diagwin_panel(diagwin, &panel);
            if (platinum_panel_is_active(&panel)) {
                content = diagwin_content(diagwin);
                panel_action = platinum_panel_action_at(&content, &panel,
                                                        where);
                if (panel_action == PLATINUM_PANEL_ACTION_SIGN_IN)
                    return PLATINUM_DIAGWIN_SIGN_IN;
                if (panel_action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                    return PLATINUM_DIAGWIN_PAIR;
            }
            diagwin_buttons(diagwin, &close, &check);
            if (PtInRect(where, &close))
                return PLATINUM_DIAGWIN_CLOSE;
            if (PtInRect(where, &check))
                return PLATINUM_DIAGWIN_CHECK;
            return PLATINUM_DIAGWIN_NONE;

        case keyDown:
        case autoKey:
            key = (unsigned char)(event->message & charCodeMask);
            diagwin_panel(diagwin, &panel);
            if (platinum_panel_key_is_cancel(event))
                return PLATINUM_DIAGWIN_CLOSE;
            if (key == '\t') {
                short last = platinum_panel_is_active(&panel) ? 3 : 2;

                if ((event->modifiers & shiftKey) != 0)
                    diagwin->focus = diagwin->focus <= 0
                                         ? last
                                         : (short)(diagwin->focus - 1);
                else
                    diagwin->focus = diagwin->focus >= last
                                         ? 0
                                         : (short)(diagwin->focus + 1);
                InvalRect(&diagwin->window->portRect);
                return PLATINUM_DIAGWIN_NONE;
            }
            if (platinum_panel_key_is_return(event)) {
                if (panel.action == PLATINUM_PANEL_ACTION_SIGN_IN)
                    return PLATINUM_DIAGWIN_SIGN_IN;
                if (panel.action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                    return PLATINUM_DIAGWIN_PAIR;
                if (diagwin->focus == 1)
                    return PLATINUM_DIAGWIN_CLOSE;
                return PLATINUM_DIAGWIN_CHECK;
            }
            if ((event->modifiers & cmdKey) != 0 && (key == 'w' || key == 'W'))
                return PLATINUM_DIAGWIN_CLOSE;
            return PLATINUM_DIAGWIN_NONE;

        default:
            break;
    }
    return PLATINUM_DIAGWIN_NONE;
}
