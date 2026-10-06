#ifndef PLATINUM_SESSION_H
#define PLATINUM_SESSION_H

#include "config.h"
#include "bridge_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_session {
    platinum_config config;
    platinum_bridge_client *bridge;
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
wf_status platinum_session_sign_out(platinum_session *session);

int platinum_session_is_paired(const platinum_session *session);
const platinum_config *platinum_session_config(const platinum_session *session);
platinum_bridge_client *platinum_session_bridge(platinum_session *session);

#ifdef __cplusplus
}
#endif

#endif
