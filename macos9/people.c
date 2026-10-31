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

static Rect people_close_rect(const platinum_people *people)
{
    Rect rect;

    rect = people->window->portRect;
    rect.left = rect.right - 78;
    rect.right -= 10;
    rect.top = rect.bottom - 34;
    rect.bottom -= 10;
    return rect;
}

static Rect people_content_rect(const platinum_people *people)
{
    Rect rect;

    rect = people->window->portRect;
    rect.left += 8;
    rect.right -= 8;
    rect.top += 8;
    rect.bottom -= 44;
    return rect;
}

static void people_panel(const platinum_people *people, platinum_panel *panel)
{
    (void)platinum_panel_for_screen(panel, people->status, people->loading,
                                    people->count != 0);
}

static void people_draw_focus(const Rect *rect)
{
    Rect inner;

    inner = *rect;
    InsetRect(&inner, 2, 2);
    FrameRect(&inner);
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
    if (platinum_window_open(&people->window, &bounds, kPeopleTitle) != noErr)
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
    platinum_panel panel;
    short index;
    short row;
    short visible;

    if (people == NULL || people->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)people->window);
    EraseRect(&people->window->portRect);

    people_panel(people, &panel);
    close_rect = people_close_rect(people);
    if (platinum_panel_is_active(&panel)) {
        Rect content;

        content = people_content_rect(people);
        platinum_panel_draw(&content, &panel);
        if (people->focus == 2) {
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
                people_draw_focus(&rects.button);
        }
        platinum_draw_button(&close_rect, kClose);
        if (people->focus == 1)
            people_draw_focus(&close_rect);
        SetPort(old_port);
        return;
    }

    platinum_draw_text(people->heading, 12, 20);

    visible = people_visible_rows(people);
    for (row = 0; row < visible; ++row) {
        index = people->scroll_row + row;
        if (index >= (short)people->count)
            break;
        people_draw_row(people, index, row);
    }

    if (people->status[0] != '\0')
        platinum_draw_text(people->status, 12,
                           people->window->portRect.bottom - 48);

    platinum_draw_button(&close_rect, kClose);
    if (people->focus == 1)
        people_draw_focus(&close_rect);

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
            where = event->where;
            GlobalToLocal(&where);

            {
                platinum_panel panel;
                Rect content;
                int panel_action;

                people_panel(people, &panel);
                content = people_content_rect(people);
                panel_action = platinum_panel_action_at(&content, &panel, where);
                if (panel_action == PLATINUM_PANEL_ACTION_SIGN_IN)
                    return PLATINUM_PEOPLE_SIGN_IN;
                if (panel_action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                    return PLATINUM_PEOPLE_PAIR;
                if (platinum_panel_is_active(&panel)) {
                    close_rect = people_close_rect(people);
                    if (PtInRect(where, &close_rect))
                        return PLATINUM_PEOPLE_CLOSE;
                    return PLATINUM_PEOPLE_NONE;
                }
            }

            if (platinum_scrollbar_handle_mouse(&people->scrollbar, event,
                                                &people->scroll_row)) {
                InvalRect(&people->window->portRect);
                return PLATINUM_PEOPLE_NONE;
            }

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

            close_rect = people_close_rect(people);
            if (PtInRect(where, &close_rect))
                return PLATINUM_PEOPLE_CLOSE;
            return PLATINUM_PEOPLE_NONE;

        case keyDown:
        case autoKey:
            visible = people_visible_rows(people);
            if (platinum_panel_key_is_cancel(event))
                return PLATINUM_PEOPLE_CLOSE;

            {
                platinum_panel panel;
                unsigned char key;

                people_panel(people, &panel);
                key = (unsigned char)(event->message & charCodeMask);
                if (key == '\t') {
                    short last = platinum_panel_is_active(&panel) ? 2 : 1;
                    if ((event->modifiers & shiftKey) != 0)
                        people->focus = people->focus <= 0
                                            ? last
                                            : (short)(people->focus - 1);
                    else
                        people->focus = people->focus >= last
                                            ? 0
                                            : (short)(people->focus + 1);
                    InvalRect(&people->window->portRect);
                    return PLATINUM_PEOPLE_NONE;
                }
                if (platinum_panel_key_is_return(event)) {
                    if (panel.action == PLATINUM_PANEL_ACTION_SIGN_IN)
                        return PLATINUM_PEOPLE_SIGN_IN;
                    if (panel.action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                        return PLATINUM_PEOPLE_PAIR;
                    if (people->focus == 1)
                        return PLATINUM_PEOPLE_CLOSE;
                    if (people->selected >= 0)
                        return PLATINUM_PEOPLE_OPEN;
                }
            }

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
