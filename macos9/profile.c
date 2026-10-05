#include "profile.h"
#include "text_codec.h"

#include <Quickdraw.h>
#include <stdio.h>
#include "json_min.h"
#include <string.h>

static unsigned char kProfileTitle[] = {
    7, 'P', 'r', 'o', 'f', 'i', 'l', 'e'
};
static unsigned char kClose[] = {
    5, 'C', 'l', 'o', 's', 'e'
};
static const char kNoProfile[] = "No profile information is available.";
static const char kLoading[] = "Loading profile...";

/*
 * Copy a string member of `root` into `destination`, converting it to
 * MacRoman on the way in.
 *
 * A member that is absent, or that is present but not a string, leaves the
 * destination empty. That is deliberate rather than lax: the bridge is trusted
 * to send the documented shape, and a field of the wrong type is a response
 * this client does not understand, so showing nothing is better than showing
 * whatever the parser happened to find there.
 */
static void profile_copy(char *destination,
                         long capacity,
                         platinum_json root,
                         const char *name)
{
    char utf8[PLATINUM_TEXT_UTF8_CAPACITY];
    long length;

    if (destination == NULL || capacity <= 0)
        return;

    destination[0] = '\0';
    if (platinum_json_string_truncating(root, name, utf8, sizeof(utf8))
        != WF_OK)
        return;

    length = platinum_text_utf8_to_macroman(utf8, destination, capacity, NULL);
    if (length < 0)
        destination[0] = '\0';
}

static long profile_number(platinum_json root, const char *name)
{
    long value = 0;

    if (platinum_json_int(root, name, &value) != WF_OK)
        return 0;

    return value;
}

void platinum_profile_set_status(platinum_profile *profile,
                                 const char *status)
{
    long length;

    if (profile == NULL)
        return;

    profile->status[0] = '\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_PROFILE_STATUS_MAX)
        length = PLATINUM_PROFILE_STATUS_MAX;

    memcpy(profile->status, status, (size_t)length);
    profile->status[length] = '\0';

    if (profile->window != NULL)
        InvalRect(&profile->window->portRect);
}

void platinum_profile_init(platinum_profile *profile)
{
    if (profile == NULL)
        return;

    memset(profile, 0, sizeof(*profile));
    platinum_profile_set_status(profile, kNoProfile);
}

wf_status platinum_profile_refresh(platinum_profile *profile,
                                   platinum_bridge_client *bridge)
{
    wf_response response;
    platinum_json root;
    wf_status status;

    if (profile == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;

    memset(&response, 0, sizeof(response));
    profile->loading = 1;
    platinum_profile_set_status(profile, kLoading);

    status = platinum_bridge_get(bridge, "/v1/profile", &response);
    if (status != WF_OK) {
        profile->loading = 0;
        if (status == WF_ERR_AUTH)
            platinum_profile_set_status(
                profile, "Session expired. Pair the account again.");
        else
            platinum_profile_set_status(profile, "Profile refresh failed.");
        wf_response_free(&response);
        return status;
    }

    status = platinum_json_open(&root,
                               response.body != NULL ? response.body : "");
    if (status != WF_OK) {
        profile->loading = 0;
        platinum_profile_set_status(profile, "The bridge returned invalid profile data.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    profile_copy(profile->did, sizeof(profile->did), root, "did");
    profile_copy(profile->handle, sizeof(profile->handle), root, "handle");
    profile_copy(profile->display_name, sizeof(profile->display_name), root,
                 "displayName");
    profile_copy(profile->description, sizeof(profile->description), root,
                 "description");
    profile->followers_count = profile_number(root, "followersCount");
    profile->follows_count = profile_number(root, "followsCount");
    profile->posts_count = profile_number(root, "postsCount");

    /* Every member is read while the response is still alive, and the cursor
     * only ever pointed into response.body, so nothing here can be left
     * holding into a freed buffer. */
    wf_response_free(&response);

    profile->loading = 0;
    if (profile->handle[0] == '\0' && profile->did[0] == '\0')
        platinum_profile_set_status(profile, kNoProfile);
    else
        platinum_profile_set_status(profile, NULL);

    return WF_OK;
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

    profile->owner.kind = PLATINUM_WINDOW_PROFILE;
    profile->owner.owner = profile;

    SetRect(&bounds, 116, 70, 616, 390);
    profile->window = NewCWindow(&bounds, 1, 0,
                                 platinum_window_proc,
                                 (WindowPtr)-1L, 1, 0L);
    if (profile->window == NULL)
        return memFullErr;
    SetWindowRefCon(profile->window, (long)&profile->owner);
    SetWindowTitle(profile->window, kProfileTitle);

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
                HiliteWindow(profile->window);
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
