#include "profile.h"

#include <Quickdraw.h>
#include <stdio.h>
#include <string.h>

static unsigned char kProfileTitle[] = {
    7, 'P', 'r', 'o', 'f', 'i', 'l', 'e'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};

/* The model sets status text only; the window repaints here. */
void platinum_profile_set_status(platinum_profile *profile,
                                 const char *status)
{
    platinum_profile_status_text(profile, status);
    if (profile != NULL && profile->window != NULL)
        InvalRect(&profile->window->portRect);
}

static void profile_text(const char *text, short x, short y)
{
    if (text == NULL || text[0] == '\0')
        return;

    MoveTo(x, y);
    DrawText((Ptr)text, 0, (short)strlen(text));
}

static void profile_button(const Rect *bounds, StringPtr title)
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
    profile->window = NewCWindow(NULL, &bounds, kProfileTitle, 1,
                                 documentProc, (WindowPtr)-1L, 1, 0L);
    if (profile->window == NULL)
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

    if (profile->display_name[0] != '\0')
        profile_text(profile->display_name, 16, 28);
    else
        profile_text(profile->handle, 16, 28);

    if (profile->handle[0] != '\0')
        profile_text(profile->handle, 16, 48);

    if (profile->description[0] != '\0')
        profile_text(profile->description, 16, 76);

    if (profile->status[0] != '\0')
        profile_text(profile->status, 16, 104);

    sprintf(counts, "Followers: %ld   Following: %ld   Posts: %ld",
            profile->followers_count,
            profile->follows_count,
            profile->posts_count);
    profile_text(counts, 16, 136);

    /* Relationship in words, never by colour. */
    if (profile->other) {
        profile_text(profile->following ? "You follow them." : "You do not follow them.",
                     16, 160);
        if (profile->followed_by)
            profile_text("They follow you.", 16, 176);
    }

    if (profile->has_pinned) {
        profile_text("Pinned post:", 16, 204);
        profile_text(profile->pinned.line1, 16, 220);
        profile_text(profile->pinned.line2, 16, 234);
    }

    close_rect = profile->window->portRect;
    close_rect.left = close_rect.right - 78;
    close_rect.right -= 10;
    close_rect.top = close_rect.bottom - 34;
    close_rect.bottom -= 10;
    profile_button(&close_rect, kClose);

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

            close_rect = profile->window->portRect;
            close_rect.left = close_rect.right - 78;
            close_rect.right -= 10;
            close_rect.top = close_rect.bottom - 34;
            close_rect.bottom -= 10;

            if (PtInRect(where, &close_rect))
                return PLATINUM_PROFILE_CLOSE;
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
