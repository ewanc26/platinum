#ifndef PLATINUM_APPPW_H
#define PLATINUM_APPPW_H

#include "secret.h"

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_APPPW_URL_MAX 255
#define PLATINUM_APPPW_HANDLE_MAX 128
#define PLATINUM_APPPW_STATUS_MAX 159

/*
 * Sign in with a handle and an app password, for when the browser pairing is not
 * wanted and the bridge's operator has allowed it. The password is typed into a
 * masked platinum_secret, never into TextEdit.
 */
typedef struct platinum_apppw {
    WindowPtr window;
    TEHandle bridge_url;
    TEHandle handle;
    platinum_secret password;
    short active_field; /* 0 URL, 1 handle, 2 password */
    int http_armed;     /* the plain-http warning has been shown once */
    char status[PLATINUM_APPPW_STATUS_MAX + 1];
} platinum_apppw;

enum {
    PLATINUM_APPPW_NONE = 0,
    PLATINUM_APPPW_SIGN_IN = 1,
    PLATINUM_APPPW_CANCEL = 2
};

void platinum_apppw_init(platinum_apppw *apppw);
OSErr platinum_apppw_open(platinum_apppw *apppw, const char *bridge_url);
/* Closes the window and wipes the password. Safe to call twice. */
void platinum_apppw_close(platinum_apppw *apppw);
int platinum_apppw_handle_event(platinum_apppw *apppw, EventRecord *event);
void platinum_apppw_draw(platinum_apppw *apppw);
void platinum_apppw_set_status(platinum_apppw *apppw, const char *status);
OSErr platinum_apppw_get_bridge_url(const platinum_apppw *apppw, char *buffer,
                                    long capacity);
/* The handle as typed, prepared for the bridge (UTF-8, no leading @). */
OSErr platinum_apppw_get_handle(const platinum_apppw *apppw, char *utf8,
                                long capacity);

/* True for an address that starts http:// (any case): the password would cross
 * the network unencrypted. Pure. */
int platinum_apppw_is_plain_http(const char *url);

/*
 * Turn what was typed as the account (MacRoman) into what the bridge wants:
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
