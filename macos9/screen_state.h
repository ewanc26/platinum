#ifndef PLATINUM_SCREEN_STATE_H
#define PLATINUM_SCREEN_STATE_H

/*
 * What a screen can do with the sessions the application holds.
 *
 * A screen is either native (the timeline, notifications) or bridge-backed
 * (profile, people, thread, search, conversations, diagnostics). A native
 * session does not make a bridge-backed screen work. Saying that plainly is the
 * point of this module: on a fresh install those screens used to fail silently,
 * because the sign-in window no longer configures a bridge.
 *
 * No Toolbox and no allocation, so every combination is host-tested.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLATINUM_SCREEN_READY = 0,
    PLATINUM_SCREEN_NEEDS_SIGN_IN = 1,
    PLATINUM_SCREEN_NEEDS_BRIDGE = 2
} platinum_screen_state;

/* A screen fed by the native session: ready with either session, because the
 * bridge session is still a fallback for the timeline. */
platinum_screen_state platinum_screen_native_state(int has_native,
                                                   int has_bridge);

/* A screen that still calls the bridge: ready only when one is configured. */
platinum_screen_state platinum_screen_bridge_state(int has_native,
                                                   int has_bridge);

/* The sentence the window shows. Never NULL; READY is the empty string. */
const char *platinum_screen_state_text(platinum_screen_state state);

#ifdef __cplusplus
}
#endif

#endif
