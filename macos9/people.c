#include "people.h"
#include "drawutil.h"
#include "scrollbar.h"

#include <Quickdraw.h>
#include <string.h>

#define kPeopleRowHeight 20

static unsigned char kPeopleTitle[] = {
    6, 'T', 'h', 'r', 'e', 'a', 'd'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};

static short people_visible_rows(const platinum_people *people)
{
    short visible;

    visible = (people->window->portRect.bottom - 70) / kPeopleRowHeight;
    return visible < 1 ? 1 : visible;
}

OSErr platinum_people_open(platinum_people *people)
{
    Rect bounds;
    Rect scrollbar_bounds;

    if (people == NULL)
        return paramErr;
    if (people->window != NULL) {
        SelectWindow(people->window);
        return noErr;
    }

    SetRect(&bounds, 96, 74, 656, 434);
    people->window = NewCWindow(NULL, &bounds, kPeopleTitle, 1,
                                documentProc, (WindowPtr)-1L, 1, 0L);
    if (people->window == NULL)
        return memFullErr;

    scrollbar_bounds = people->window->portRect;
    scrollbar_bounds.left = scrollbar_bounds.right - 15;
    scrollbar_bounds.top = 28;
    scrollbar_bounds.bottom -= 40;
    if (platinum_scrollbar_open(&people->scrollbar, people->window,
                                &scrollbar_bounds) != noErr) {
        DisposeWindow(people->window);
        people->window = NULL;
        return memFullErr;
    }

    SetPort((GrafPtr)people->window);
    platinum_people_draw(people);
    SelectWindow(people->window);
    return noErr;
}

void platinum_people_close(platinum_people *people)
{
    if (people == NULL)
        return;
    platinum_scrollbar_close(&people->scrollbar);
    if (people->window != NULL) {
        DisposeWindow(people->window);
        people->window = NULL;
    }
}

static void people_draw_row(const platinum_people *people, short index,
                            short row)
{
    const platinum_person *item;
    short top;

    item = &people->items[index];
    top = 52 + (row * kPeopleRowHeight);
    platinum_draw_text(item->name, 12, top);
    platinum_draw_text(item->handle, 240, top);
    /* The selected row is framed and marked in words, not only by colour. */
    if (index == people->selected) {
        Rect box;

        SetRect(&box, 4, top - 14, people->window->portRect.right - 20, top + 5);
        FrameRect(&box);
        platinum_draw_text("(selected)", 420, top);
    }
}

void platinum_people_draw(platinum_people *people)
{
    GrafPtr old_port;
    Rect close_rect;
    short index;
    short row;
    short visible;

    if (people == NULL || people->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)people->window);
    EraseRect(&people->window->portRect);

    platinum_draw_text(people->heading, 12, 20);
    if (people->status[0] != '\0')
        platinum_draw_text(people->status, 12, 38);

    visible = people_visible_rows(people);
    for (row = 0; row < visible; ++row) {
        index = people->scroll_row + row;
        if (index >= (short)people->count)
            break;
        people_draw_row(people, index, row);
    }

    close_rect = people->window->portRect;
    close_rect.left = close_rect.right - 78;
    close_rect.right -= 10;
    close_rect.top = close_rect.bottom - 34;
    close_rect.bottom -= 10;
    platinum_draw_button(&close_rect, kClose);

    platinum_scrollbar_set_range(&people->scrollbar, (short)people->count,
                                 visible, people->scroll_row);
    platinum_scrollbar_draw(&people->scrollbar);
    SetPort(old_port);
}

int platinum_people_handle_event(platinum_people *people, EventRecord *event)
{
    Point where;
    Rect close_rect;
    short visible;

    if (people == NULL || event == NULL || people->window == NULL)
        return PLATINUM_PEOPLE_NONE;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == people->window) {
                BeginUpdate(people->window);
                platinum_people_draw(people);
                EndUpdate(people->window);
            }
            return PLATINUM_PEOPLE_NONE;

        case activateEvt:
            if ((WindowPtr)(long)event->message == people->window)
                HiliteWindow(people->window,
                             (event->modifiers & activeFlag) != 0);
            return PLATINUM_PEOPLE_NONE;

        case mouseDown:
            if (platinum_scrollbar_handle_mouse(&people->scrollbar, event,
                                                &people->scroll_row)) {
                InvalRect(&people->window->portRect);
                return PLATINUM_PEOPLE_NONE;
            }
            where = event->where;
            GlobalToLocal(&where);

            /* A click on a row selects it and asks the application to open it. */
            if (where.v >= 38 && where.h < people->window->portRect.right - 16) {
                short row;

                row = (short)(people->scroll_row + (where.v - 38) / kPeopleRowHeight);
                if ((where.v - 38) / kPeopleRowHeight < people_visible_rows(people) &&
                    row >= 0 && row < (short)people->count) {
                    people->selected = row;
                    InvalRect(&people->window->portRect);
                    return PLATINUM_PEOPLE_OPEN;
                }
            }

            close_rect = people->window->portRect;
            close_rect.left = close_rect.right - 78;
            close_rect.right -= 10;
            close_rect.top = close_rect.bottom - 34;
            close_rect.bottom -= 10;
            if (PtInRect(where, &close_rect))
                return PLATINUM_PEOPLE_CLOSE;
            return PLATINUM_PEOPLE_NONE;

        case keyDown:
        case autoKey:
            visible = people_visible_rows(people);
            if ((event->modifiers & cmdKey) != 0 &&
                ((event->message & charCodeMask) == 'w' ||
                 (event->message & charCodeMask) == 'W'))
                return PLATINUM_PEOPLE_CLOSE;

            switch (event->message & charCodeMask) {
                case upArrow:
                    if (people->scroll_row > 0)
                        --people->scroll_row;
                    break;
                case downArrow:
                    /* Down at the bottom asks for the next page, by keystroke
                     * rather than as a side effect of scrolling. */
                    if (people->scroll_row + visible >= (short)people->count &&
                        platinum_people_has_more(people))
                        return PLATINUM_PEOPLE_LOAD_MORE;
                    if (people->scroll_row + visible < (short)people->count)
                        ++people->scroll_row;
                    break;
                case pageUp:
                    people->scroll_row -= visible;
                    if (people->scroll_row < 0)
                        people->scroll_row = 0;
                    break;
                case pageDown:
                    people->scroll_row += visible;
                    if (people->scroll_row + visible > (short)people->count)
                        people->scroll_row = (short)people->count - visible;
                    if (people->scroll_row < 0)
                        people->scroll_row = 0;
                    break;
                default:
                    break;
            }
            InvalRect(&people->window->portRect);
            return PLATINUM_PEOPLE_NONE;

        default:
            break;
    }
    return PLATINUM_PEOPLE_NONE;
}
