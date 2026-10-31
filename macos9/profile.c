#include "profile.h"
#include "drawutil.h"

#include <Quickdraw.h>
#include <stdio.h>
#include <string.h>

static unsigned char kProfileTitle[] = {
    7, 'P', 'r', 'o', 'f', 'i', 'l', 'e'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};

/* The model sets status text only; the window repaints here. A screen-state
 * sentence also becomes the shared panel, so the notice looks like Platinum
 * rather than like an error string. */
void platinum_profile_set_status(platinum_profile *profile,
                                 const char *status)
{
    if (profile == NULL)
        return;
    platinum_profile_status_text(profile, status);
    memset(&profile->panel, 0, sizeof profile->panel);
    if (status != NULL)
        (void)platinum_panel_from_message(status, &profile->panel);
    if (profile->window != NULL)
        InvalRect(&profile->window->portRect);
}

static void profile_close_rect(const platinum_profile *profile, Rect *rect)
{
    *rect = profile->window->portRect;
    rect->left = rect->right - 78;
    rect->right -= 10;
    rect->top = rect->bottom - 34;
    rect->bottom -= 10;
}

OSErr platinum_profile_open(platinum_profile *profile)
{
    Rect bounds;

    if (profile == NULL)
        return paramErr;

    if (profile->window != NULL) {
        SelectWindow(profile->window);
        return noErr;
    }

    SetRect(&bounds, 116, 70, 616, 390);
    if (platinum_window_open(&profile->window, &bounds, kProfileTitle) != noErr)
        return memFullErr;

    SetPort((GrafPtr)profile->window);
    platinum_profile_draw(profile);
    SelectWindow(profile->window);
    return noErr;
}

void platinum_profile_close(platinum_profile *profile)
{
    if (profile == NULL)
        return;

    if (profile->window != NULL) {
        DisposeWindow(profile->window);
        profile->window = NULL;
    }
}

void platinum_profile_draw(platinum_profile *profile)
{
    GrafPtr old_port;
    Rect close_rect;
    char counts[96];

    if (profile == NULL || profile->window == NULL)
        return;

    GetPort(&old_port);
    SetPort((GrafPtr)profile->window);
    EraseRect(&profile->window->portRect);

    if (profile->panel.kind != PLATINUM_PANEL_NONE) {
        Rect content;
        content = profile->window->portRect;
        content.left += 8;
        content.right -= 8;
        content.top += 8;
        content.bottom -= 44;
        platinum_panel_draw(&content, &profile->panel);
        profile_close_rect(profile, &close_rect);
        platinum_draw_button(&close_rect, kClose);
        SetPort(old_port);
        return;
    }

    if (profile->display_name[0] != '\0')
        platinum_draw_text(profile->display_name, 16, 28);
    else
        platinum_draw_text(profile->handle, 16, 28);

    if (profile->handle[0] != '\0')
        platinum_draw_text(profile->handle, 16, 48);

    if (profile->description[0] != '\0')
        platinum_draw_text(profile->description, 16, 76);

    if (profile->status[0] != '\0')
        platinum_draw_text(profile->status, 16, 104);

    sprintf(counts, "Followers: %ld   Following: %ld   Posts: %ld",
            profile->followers_count,
            profile->follows_count,
            profile->posts_count);
    platinum_draw_text(counts, 16, 136);

    /* Relationship in words, never by colour. */
    if (profile->other) {
        platinum_draw_text(profile->following ? "You follow them." : "You do not follow them.",
                     16, 160);
        if (profile->followed_by)
            platinum_draw_text("They follow you.", 16, 176);
        if (profile->muted)
            platinum_draw_text("You have muted them.", 16, 190);
        if (profile->blocking)
            platinum_draw_text("You have blocked them.", 16, 190);
    }

    if (profile->has_pinned) {
        platinum_draw_text("Pinned post:", 16, 204);
        platinum_draw_text(profile->pinned.line1, 16, 220);
        platinum_draw_text(profile->pinned.line2, 16, 234);
    }

    profile_close_rect(profile, &close_rect);
    platinum_draw_button(&close_rect, kClose);

    SetPort(old_port);
}

int platinum_profile_handle_event(platinum_profile *profile,
                                   EventRecord *event)
{
    Point where;
    Rect close_rect;

    if (profile == NULL || event == NULL || profile->window == NULL)
        return PLATINUM_PROFILE_NONE;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == profile->window) {
                BeginUpdate(profile->window);
                platinum_profile_draw(profile);
                EndUpdate(profile->window);
            }
            return PLATINUM_PROFILE_NONE;

        case activateEvt:
            if ((WindowPtr)(long)event->message == profile->window)
                HiliteWindow(profile->window,
                             (event->modifiers & activeFlag) != 0);
            return PLATINUM_PROFILE_NONE;

        case mouseDown:
            where = event->where;
            GlobalToLocal(&where);

            profile_close_rect(profile, &close_rect);
            if (PtInRect(where, &close_rect))
                return PLATINUM_PROFILE_CLOSE;

            if (profile->panel.kind != PLATINUM_PANEL_NONE) {
                platinum_panel_rects panel_rects;
                char lines[PLATINUM_PANEL_LINES][PLATINUM_PANEL_LINE_MAX];
                Rect content;
                short count;

                content = profile->window->portRect;
                content.left += 8;
                content.right -= 8;
                content.top += 8;
                content.bottom -= 44;
                count = platinum_panel_wrap(profile->panel.message, 48, lines,
                                            PLATINUM_PANEL_LINES);
                platinum_panel_rects_compute(
                    &content, count,
                    profile->panel.action != PLATINUM_PANEL_ACTION_NONE,
                    &panel_rects);
                if (panel_rects.button_visible &&
                    PtInRect(where, &panel_rects.button)) {
                    if (profile->panel.action == PLATINUM_PANEL_ACTION_SIGN_IN)
                        return PLATINUM_PROFILE_SIGN_IN;
                    if (profile->panel.action == PLATINUM_PANEL_ACTION_PAIR_BRIDGE)
                        return PLATINUM_PROFILE_PAIR;
                }
            }
            return PLATINUM_PROFILE_NONE;

        case keyDown:
        case autoKey:
            if ((event->modifiers & cmdKey) != 0 &&
                ((event->message & charCodeMask) == 'w' ||
                 (event->message & charCodeMask) == 'W'))
                return PLATINUM_PROFILE_CLOSE;
            return PLATINUM_PROFILE_NONE;

        default:
            break;
    }

    return PLATINUM_PROFILE_NONE;
}
