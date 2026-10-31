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
    short active_field; /* 0 handle, 1 password */
    char status[PLATINUM_APPPW_STATUS_MAX + 1];
} platinum_apppw;

enum {
    PLATINUM_APPPW_NONE = 0,
    PLATINUM_APPPW_SIGN_IN = 1,
    PLATINUM_APPPW_CANCEL = 2
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
/* The handle as typed, prepared for the request (UTF-8, no leading @). */
OSErr platinum_apppw_get_handle(const platinum_apppw *apppw, char *utf8,
                                long capacity);

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
