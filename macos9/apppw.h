#ifndef PLATINUM_APPPW_H
#define PLATINUM_APPPW_H

#include "secret.h"

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_APPPW_HANDLE_MAX 128
#define PLATINUM_APPPW_STATUS_MAX 159
/* Mirrors platinum_atproto_session.h, so a resolved account always fits
 * without apppw.h depending on the session headers. */
#define PLATINUM_APPPW_RESOLVED_HANDLE_MAX 253
#define PLATINUM_APPPW_RESOLVED_PDS_MAX 511
#define PLATINUM_APPPW_ENTRYWAY_MAX 47

/*
 * Sign in to the account's own PDS with a handle and an app password. The
 * account is found from the handle (its well-known document, then resolveHandle
 * on the public AppView), so no server address is typed and no bridge is in the
 * path. The password is typed into a masked platinum_secret, never into
 * TextEdit.
 */
typedef struct platinum_apppw {
    WindowPtr window;
    TEHandle handle;
    platinum_secret password;
    /* The focus stops in drawing order: handle, password, Cancel, Sign In.
     * The signed-in view has one stop: Close, at the Cancel position. */
    short key_focus;
    int resolving; /* the account is being resolved; the panel is showing */
    int signed_in; /* success: the resolved view is showing */
    char resolved_handle[PLATINUM_APPPW_RESOLVED_HANDLE_MAX + 1];
    char resolved_pds[PLATINUM_APPPW_RESOLVED_PDS_MAX + 1];
    char resolved_entryway[PLATINUM_APPPW_ENTRYWAY_MAX + 1];
    char status[PLATINUM_APPPW_STATUS_MAX + 1];
} platinum_apppw;

enum {
    PLATINUM_APPPW_NONE = 0,
    PLATINUM_APPPW_SIGN_IN = 1,
    PLATINUM_APPPW_CANCEL = 2
};

enum {
    PLATINUM_APPPW_FOCUS_HANDLE = 0,
    PLATINUM_APPPW_FOCUS_PASSWORD = 1,
    PLATINUM_APPPW_FOCUS_CANCEL = 2,
    PLATINUM_APPPW_FOCUS_SIGN_IN = 3,
    PLATINUM_APPPW_FOCUS_COUNT = 4
};

void platinum_apppw_init(platinum_apppw *apppw);
/* `handle` is put in the field when it is not NULL (the signed-in account's
 * handle, so signing in again does not start from an empty box). */
OSErr platinum_apppw_open(platinum_apppw *apppw, const char *handle);
/* Closes the window and wipes the password. Safe to call twice. */
void platinum_apppw_close(platinum_apppw *apppw);
int platinum_apppw_handle_event(platinum_apppw *apppw, EventRecord *event);
void platinum_apppw_draw(platinum_apppw *apppw);
void platinum_apppw_set_status(platinum_apppw *apppw, const char *status);
/* Show or clear the loading panel. Turning it on draws immediately: the
 * sign-in request blocks the event loop, so an invalidation alone would not
 * repaint until the request finished. Turning it off only invalidates. */
void platinum_apppw_set_resolving(platinum_apppw *apppw, int resolving);
/* The signed-in view: the handle, the PDS it signed in to, and which handle
 * method found the account. Wipes the password and leaves the window open
 * with Close as the only stop; Return and Escape close it. Any argument may
 * be NULL for an empty line. */
void platinum_apppw_set_resolved(platinum_apppw *apppw, const char *handle,
                                 const char *pds, const char *entryway);
/* The handle as typed, prepared for the request (UTF-8, no leading @). */
OSErr platinum_apppw_get_handle(const platinum_apppw *apppw, char *utf8,
                                long capacity);

/* The next focus stop for Tab (backward for Shift-Tab). The signed-in view
 * has one stop, so it stays on Close. Pure. */
short platinum_apppw_next_focus(short focus, int backward, int signed_in);
/* What Return does at `focus`: the focused button, or the default action
 * (Sign In) from a field. The signed-in view closes. Pure. */
int platinum_apppw_return_action(short focus, int signed_in);

/*
 * Turn what was typed as the account (MacRoman) into what the client sends:
 * surrounding spaces and one leading @ dropped, converted to UTF-8. Returns the
 * length, or -1 for an empty handle, one with control characters, more than
 * PLATINUM_APPPW_HANDLE_MAX characters, or one that does not fit. Pure.
 */
long platinum_apppw_prepare_handle(const char *macroman, char *utf8,
                                   long capacity);

#ifdef __cplusplus
}
#endif

#endif
