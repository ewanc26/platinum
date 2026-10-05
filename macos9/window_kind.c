#include "window_kind.h"
#include "pairing.h"
#include "preferences.h"
#include "profile.h"
#include "notifications.h"
#include "compose.h"
#include "application.h"

/*
 * The real window procedure.
 *
 * Extracts the owner from refCon and calls the module's draw function on
 * update. The module draw functions already exist and already handle updateEvt;
 * this routes through the window manager rather than through manual pointer
 * comparison in platinum_application_handle_event.
 */
ProcPtr platinum_window_proc(WindowPtr window, int message,
                             ParamStructRec *param, Ptr lParam)
{
    platinum_window_owner *owner;

    (void)param;
    (void)lParam;

    if (window == NULL)
        return NULL;

    owner = (platinum_window_owner *)GetWindowRefCon(window);
    if (owner == NULL)
        return NULL;

    if (message == 0) {  /* updateEvt */
        BeginUpdate(window);
        switch (owner->kind) {
            case PLATINUM_WINDOW_PAIRING:
                platinum_pairing_draw((platinum_pairing *)owner->owner);
                break;
            case PLATINUM_WINDOW_PREFERENCES:
                platinum_preferences_draw((platinum_preferences *)owner->owner);
                break;
            case PLATINUM_WINDOW_PROFILE:
                platinum_profile_draw((platinum_profile *)owner->owner);
                break;
            case PLATINUM_WINDOW_NOTIFICATIONS:
                platinum_notifications_draw((platinum_notifications *)owner->owner);
                break;
            case PLATINUM_WINDOW_COMPOSE:
                platinum_compose_draw((platinum_compose *)owner->owner);
                break;
            case PLATINUM_WINDOW_MAIN:
            default:
                break;
        }
        EndUpdate(window);
    }

    return NULL;
}
