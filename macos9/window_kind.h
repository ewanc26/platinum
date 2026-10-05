#ifndef PLATINUM_WINDOW_KIND_H
#define PLATINUM_WINDOW_KIND_H

#include <MacTypes.h>

/*
 * Window identity carried by refCon.
 *
 * The Classic Mac window manager does not know what platinum structure owns a
 * window. The routing in platinum_application_handle_event reconstructs this
 * from six stored pointers, which works but means the application has to know
 * about every window type. The window procedure below extracts the owner from
 * refCon instead, so the application does not hold every pointer and so a new
 * window type does not require a new branch in the event loop.
 */

typedef enum {
    PLATINUM_WINDOW_MAIN,
    PLATINUM_WINDOW_PAIRING,
    PLATINUM_WINDOW_PREFERENCES,
    PLATINUM_WINDOW_PROFILE,
    PLATINUM_WINDOW_NOTIFICATIONS,
    PLATINUM_WINDOW_COMPOSE
} platinum_window_kind;

/*
 * The structure stored in refCon. Every module that opens a window embeds this
 * as its first member, so the refCon can be cast to platinum_window_owner * and
 * then to the correct structure once the kind is known.
 */
typedef struct {
    platinum_window_kind kind;
    void *owner;        /* The platinum_pairing *, platinum_profile *, etc. */
} platinum_window_owner;

/*
 * The real window procedure.
 *
 * NewCWindow takes a procedure pointer that the window manager calls on
 * update and activate. The stub SDK declared documentProc with this signature
 * but defined it nowhere, so every window was created with a dangling procedure
 * pointer. This is the procedure: it extracts the owner from refCon and calls
 * the module's draw function on update, or does nothing on other messages.
 */
ProcPtr platinum_window_proc(WindowPtr window, int message,
                             ParamStructRec *param, Ptr lParam);

#endif
