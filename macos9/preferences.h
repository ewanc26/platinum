#ifndef PLATINUM_PREFERENCES_H
#define PLATINUM_PREFERENCES_H

#include "session.h"

#include <Events.h>
#include <Windows.h>
#include "window_kind.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_PREFERENCES_STATUS_MAX 127

typedef struct platinum_preferences {
    platinum_window_owner owner;
    WindowPtr window;
    const platinum_session *session;
    char status[PLATINUM_PREFERENCES_STATUS_MAX + 1];
} platinum_preferences;

enum {
    PLATINUM_PREFERENCES_NONE = 0,
    PLATINUM_PREFERENCES_CLOSE = 1,
    PLATINUM_PREFERENCES_SIGN_OUT = 2,
    PLATINUM_PREFERENCES_PAIR = 3
};

void platinum_preferences_init(platinum_preferences *preferences);
OSErr platinum_preferences_open(platinum_preferences *preferences,
                                const platinum_session *session);
void platinum_preferences_close(platinum_preferences *preferences);
int platinum_preferences_handle_event(platinum_preferences *preferences,
                                      EventRecord *event);
void platinum_preferences_draw(platinum_preferences *preferences);
void platinum_preferences_set_status(platinum_preferences *preferences,
                                      const char *status);

#ifdef __cplusplus
}
#endif

#endif
