#include "search.h"
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

static void search_text(const char *text, short x, short y)
{
    if (text == NULL || text[0] == '\0')
        return;
    MoveTo(x, y);
    DrawText((Ptr)text, 0, (short)strlen(text));
}

static void search_button(const Rect *bounds, StringPtr title)
{
    long width;

    FrameRect(bounds);
    width = StringWidth(title);
    MoveTo(bounds->left + (short)((bounds->right - bounds->left - width) / 2),
           bounds->top + 14);
    DrawString(title);
}

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
    search->window = NewCWindow(NULL, &bounds,
                                search->mode == PLATINUM_SEARCH_POSTS ? kPostsTitle
                                : search->mode == PLATINUM_SEARCH_WORD ? kWordTitle
                                                                       : kAccountsTitle,
                                1, documentProc, (WindowPtr)-1L, 1, 0L);
    if (search->window == NULL)
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

    if (search == NULL || search->window == NULL || search->field == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)search->window);
    EraseRect(&search->window->portRect);

    search_text(search->mode == PLATINUM_SEARCH_POSTS ? "Search posts for:"
                : search->mode == PLATINUM_SEARCH_WORD
                    ? "Hide posts containing this word or phrase:"
                    : "Search accounts for:", 14, 20);
    frame = (*search->field)->viewRect;
    FrameRect(&frame);
    TEUpdate(&frame, search->field);
    search_text(search->mode == PLATINUM_SEARCH_WORD ? "Press Return to add it."
                                                     : "Press Return to search.", 14, 80);
    if (search->status[0] != '\0')
        search_text(search->status, 14, 100);

    search_buttons(search, &cancel, &go);
    search_button(&cancel, kCancel);
    search_button(&go, search->mode == PLATINUM_SEARCH_WORD ? kAdd : kSearch);
    SetPort(old_port);
}

int platinum_search_handle_event(platinum_search *search, EventRecord *event)
{
    Point where;
    Rect cancel;
    Rect go;
    unsigned char key;

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
