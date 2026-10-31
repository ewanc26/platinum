#include "search.h"
#include "drawutil.h"
#include "state_panel.h"
#include "textfield.h"

#include <Quickdraw.h>
#include <string.h>

static unsigned char kAccountsTitle[] = {
    15, 'S', 'e', 'a', 'r', 'c', 'h', ' ', 'A', 'c', 'c', 'o', 'u', 'n', 't', 's'
};
static unsigned char kPostsTitle[] = {
    12, 'S', 'e', 'a', 'r', 'c', 'h', ' ', 'P', 'o', 's', 't', 's'
};
static unsigned char kWordTitle[] = {
    15, 'A', 'd', 'd', ' ', 'M', 'u', 't', 'e', 'd', ' ', 'W', 'o', 'r', 'd'
};
static unsigned char kCancel[] = { 6, 'C', 'a', 'n', 'c', 'e', 'l' };
static unsigned char kSearch[] = { 6, 'S', 'e', 'a', 'r', 'c', 'h' };
static unsigned char kAdd[] = { 3, 'A', 'd', 'd' };

static void search_buttons(const platinum_search *search, Rect *cancel,
                           Rect *go)
{
    *cancel = search->window->portRect;
    cancel->left = cancel->right - 160;
    cancel->right = cancel->left + 68;
    cancel->top = cancel->bottom - 30;
    cancel->bottom -= 8;

    *go = *cancel;
    go->left = cancel->right + 8;
    go->right = go->left + 64;
}

static Rect search_panel_content(const platinum_search *search)
{
    Rect content;

    content = search->window->portRect;
    InsetRect(&content, 8, 8);
    return content;
}

static void search_panel(const platinum_search *search, platinum_panel *panel)
{
    (void)platinum_panel_from_message(search->status, panel);
}

static void search_draw_focus(const Rect *rect)
{
    Rect inner;

    inner = *rect;
    InsetRect(&inner, 2, 2);
    FrameRect(&inner);
}

void platinum_search_init(platinum_search *search)
{
    if (search != NULL)
        memset(search, 0, sizeof(*search));
}

void platinum_search_set_status(platinum_search *search, const char *status)
{
    if (search == NULL)
        return;
    search->status[0] = '\0';
    if (status != NULL) {
        strncpy(search->status, status, sizeof(search->status) - 1);
        search->status[sizeof(search->status) - 1] = '\0';
    }
    if (search->window != NULL)
        InvalRect(&search->window->portRect);
}

OSErr platinum_search_open(platinum_search *search, int mode)
{
    Rect bounds;
    Rect field_rect;

    if (search == NULL)
        return paramErr;
    if (search->window != NULL) {
        SelectWindow(search->window);
        return noErr;
    }

    memset(search, 0, sizeof(*search));
    search->mode = mode == PLATINUM_SEARCH_POSTS ? PLATINUM_SEARCH_POSTS
                   : mode == PLATINUM_SEARCH_WORD ? PLATINUM_SEARCH_WORD
                                                  : PLATINUM_SEARCH_ACCOUNTS;

    SetRect(&bounds, 136, 110, 576, 250);
    if (platinum_window_open(&search->window, &bounds,
                             search->mode == PLATINUM_SEARCH_POSTS ? kPostsTitle
                             : search->mode == PLATINUM_SEARCH_WORD ? kWordTitle
                                                                    : kAccountsTitle) != noErr)
        return memFullErr;

    SetRect(&field_rect, 14, 30, bounds.right - bounds.left - 14, 54);
    search->field = platinum_textfield_new(search->window, &field_rect);
    if (search->field == NULL) {
        DisposeWindow(search->window);
        search->window = NULL;
        return memFullErr;
    }

    SetPort((GrafPtr)search->window);
    platinum_search_draw(search);
    TEActivate(search->field);
    SelectWindow(search->window);
    return noErr;
}

void platinum_search_close(platinum_search *search)
{
    if (search == NULL)
        return;
    if (search->field != NULL) {
        TEDeactivate(search->field);
        TEDispose(search->field);
        search->field = NULL;
    }
    if (search->window != NULL) {
        DisposeWindow(search->window);
        search->window = NULL;
    }
}

void platinum_search_draw(platinum_search *search)
{
    GrafPtr old_port;
    Rect cancel;
    Rect go;
    Rect frame;
    platinum_panel panel;

    if (search == NULL || search->window == NULL || search->field == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)search->window);
    EraseRect(&search->window->portRect);

    search_panel(search, &panel);
    if (platinum_panel_is_active(&panel)) {
        Rect content;

        content = search_panel_content(search);
        platinum_panel_draw(&content, &panel);
        if (search->focus == 3) {
            platinum_panel_rects rects;
            char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX];
            short count;

            count = platinum_panel_wrap(panel.message,
                                        platinum_panel_columns(&content),
                                        lines, PLATINUM_PANEL_LINES);
            platinum_panel_rects_compute(
                &content, count,
                panel.action != PLATINUM_PANEL_ACTION_NONE, &rects);
            if (rects.button_visible)
                search_draw_focus(&rects.button);
        }
        SetPort(old_port);
        return;
    }

    platinum_draw_text(search->mode == PLATINUM_SEARCH_POSTS ? "Search posts for:"
                : search->mode == PLATINUM_SEARCH_WORD
                    ? "Hide posts containing this word or phrase:"
                    : "Search accounts for:", 14, 20);
    frame = (*search->field)->viewRect;
    FrameRect(&frame);
    TEUpdate(&frame, search->field);
    platinum_draw_text(search->mode == PLATINUM_SEARCH_WORD ? "Press Return to add it."
                                                     : "Press Return to search.", 14, 80);
    if (search->status[0] != '\0')
        platinum_draw_text(search->status, 14, 100);

    search_buttons(search, &cancel, &go);
    platinum_draw_button(&cancel, kCancel);
    if (search->focus == 1)
        search_draw_focus(&cancel);
    platinum_draw_button(&go, search->mode == PLATINUM_SEARCH_WORD ? kAdd : kSearch);
    if (search->focus == 2)
        search_draw_focus(&go);
    SetPort(old_port);
}

int platinum_search_handle_event(platinum_search *search, EventRecord *event)
{
    Point where;
    Rect cancel;
    Rect go;
    unsigned char key;
    platinum_panel panel;
    Rect panel_content;
    int panel_action;

    if (search == NULL || event == NULL || search->window == NULL)
        return PLATINUM_SEARCH_NONE;

    switch (event->what) {
        case activateEvt:
            if ((WindowPtr)(long)event->message == search->window) {
                if (event->modifiers & activeFlag)
                    TEActivate(search->field);
                else
                    TEDeactivate(search->field);
            }
            return PLATINUM_SEARCH_NONE;

        case updateEvt:
            if ((WindowPtr)(long)event->message == search->window) {
                BeginUpdate(search->window);
                platinum_search_draw(search);
                EndUpdate(search->window);
            }
            return PLATINUM_SEARCH_NONE;

        case mouseDown:
            where = event->where;
            GlobalToLocal(&where);
            search_panel(search, &panel);
            if (platinum_panel_is_active(&panel)) {
                panel_content = search_panel_content(search);
                panel_action = platinum_panel_action_at(&panel_content, &panel,
                                                        where);
                if (panel_action == PLATINUM_PANEL_ACTION_SIGN_IN)
                    return PLATINUM_SEARCH_SIGN_IN;
                if (panel_action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                    return PLATINUM_SEARCH_PAIR;
                return PLATINUM_SEARCH_NONE;
            }
            search_buttons(search, &cancel, &go);
            if (PtInRect(where, &cancel))
                return PLATINUM_SEARCH_CANCEL;
            if (PtInRect(where, &go))
                return PLATINUM_SEARCH_RUN;
            if (PtInRect(where, &(*search->field)->viewRect))
                TEClick(where, (event->modifiers & shiftKey) != 0,
                        search->field);
            return PLATINUM_SEARCH_NONE;

        case keyDown:
        case autoKey:
            key = (unsigned char)(event->message & charCodeMask);
            search_panel(search, &panel);
            if (platinum_panel_key_is_cancel(event))
                return PLATINUM_SEARCH_CANCEL;
            if (key == '\t') {
                short last = platinum_panel_is_active(&panel) ? 3 : 2;

                if ((event->modifiers & shiftKey) != 0)
                    search->focus = search->focus <= 0
                                        ? last
                                        : (short)(search->focus - 1);
                else
                    search->focus = search->focus >= last
                                        ? 0
                                        : (short)(search->focus + 1);
                if (search->focus == 0)
                    TEActivate(search->field);
                else
                    TEDeactivate(search->field);
                InvalRect(&search->window->portRect);
                return PLATINUM_SEARCH_NONE;
            }
            if (platinum_panel_key_is_return(event) &&
                platinum_panel_is_active(&panel)) {
                if (panel.action == PLATINUM_PANEL_ACTION_SIGN_IN)
                    return PLATINUM_SEARCH_SIGN_IN;
                if (panel.action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                    return PLATINUM_SEARCH_PAIR;
            }
            if ((event->modifiers & cmdKey) != 0 && (key == 'w' || key == 'W'))
                return PLATINUM_SEARCH_CANCEL;
            if (key == '\r' || key == 3)
                return PLATINUM_SEARCH_RUN;
            (void)platinum_textfield_key(search->field, event,
                                         PLATINUM_SEARCH_MAX, 1);
            return PLATINUM_SEARCH_NONE;

        default:
            break;
    }
    return PLATINUM_SEARCH_NONE;
}

long platinum_search_query(const platinum_search *search, char *utf8,
                           long capacity)
{
    char raw[PLATINUM_SEARCH_MAX + 1];

    if (search == NULL || search->field == NULL)
        return -1;
    if (platinum_textfield_copy(search->field, raw, sizeof(raw)) < 0)
        return -1;
    return platinum_search_prepare(raw, utf8, capacity);
}
