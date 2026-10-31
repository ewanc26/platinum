#include "screen_state.h"

platinum_screen_state platinum_screen_native_state(int has_native,
                                                   int has_bridge)
{
    if (has_native || has_bridge)
        return PLATINUM_SCREEN_READY;
    return PLATINUM_SCREEN_NEEDS_SIGN_IN;
}

platinum_screen_state platinum_screen_bridge_state(int has_native,
                                                   int has_bridge)
{
    if (has_bridge)
        return PLATINUM_SCREEN_READY;
    if (has_native)
        return PLATINUM_SCREEN_NEEDS_BRIDGE;
    return PLATINUM_SCREEN_NEEDS_SIGN_IN;
}

const char *platinum_screen_state_text(platinum_screen_state state)
{
    if (state == PLATINUM_SCREEN_READY)
        return "";
    if (state == PLATINUM_SCREEN_NEEDS_SIGN_IN)
        return "Sign in first: choose File > Sign In with App Password.";
    if (state == PLATINUM_SCREEN_NEEDS_BRIDGE)
        return "This screen still uses the bridge. Pair one under File > Pair Account.";
    return "This screen is not available.";
}
