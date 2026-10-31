#ifndef PLATINUM_SESSION_H
#define PLATINUM_SESSION_H

#include "atproto_client.h"
#include "config.h"
#include "bridge_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_session {
    platinum_config config;
    /* The bridge, for the screens that have not migrated yet. May be NULL. */
    platinum_bridge_client *bridge;
    /* The native AT Protocol session. NULL when nobody has signed in natively. */
    platinum_atproto_client *client;
    /* Which handle method found the account, for Preferences. */
    platinum_atproto_entryway entryway;
} platinum_session;

void platinum_session_init(platinum_session *session);
void platinum_session_close(platinum_session *session);

OSErr platinum_session_load(platinum_session *session);
OSErr platinum_session_set_bridge_url(platinum_session *session,
                                      const char *bridge_url);

wf_status platinum_session_pair(platinum_session *session,
                                const char *code);
/* Sign in with a handle and an app password instead of a pairing code. The
 * caller wipes its copy of the password afterwards. `reason` is optional, see
 * platinum_bridge_login_app_password. */
wf_status platinum_session_sign_in_app_password(platinum_session *session,
                                                const char *identifier,
                                                const char *password,
                                                int *reason);
/*
 * Sign in to the account's own PDS without the bridge: resolve the handle to
 * its DID and PDS, then createSession there. The password is used for that one
 * request; the caller still wipes its own copy. The session is memory-only
 * until Keychain persistence lands (ADR 0007), so a restart asks again.
 * On success the bridge session, if any, is left alone: it still serves the
 * screens that have not migrated.
 */
wf_status platinum_session_sign_in_native(platinum_session *session,
                                          const char *handle,
                                          const char *password);
wf_status platinum_session_sign_out(platinum_session *session);

int platinum_session_is_paired(const platinum_session *session);
const platinum_config *platinum_session_config(const platinum_session *session);
platinum_bridge_client *platinum_session_bridge(platinum_session *session);
/* The native client, or NULL when nobody has signed in natively. */
platinum_atproto_client *platinum_session_client(platinum_session *session);
const platinum_atproto_session *platinum_session_native_account(
    const platinum_session *session);
/* Which handle method found the account. NONE before a native sign-in. */
platinum_atproto_entryway platinum_session_entryway(
    const platinum_session *session);
/* "the handle's own well-known document", "resolveHandle" or "not known".
 * Pure, for the windows that say how the account was found. */
const char *platinum_session_entryway_name(platinum_atproto_entryway entryway);

#ifdef __cplusplus
}
#endif

#endif
