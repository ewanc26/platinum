#ifndef PLATINUM_ATPROTO_CLIENT_H
#define PLATINUM_ATPROTO_CLIENT_H

/*
 * atproto_client.h -- the Mac client's XRPC session layer.
 *
 * Wraps Wolfram's XRPC/HTTP client with the one thing every screen needs:
 * an authenticated call bound to the current session, a single
 * refreshSession-and-retry when the service says the access token is expired,
 * and a save hook so the platform can persist the refresh material without the
 * protocol code knowing where it lives.
 *
 * The password is used for one createSession request and then wiped. The
 * access token is memory-only. Nothing here logs a token or an error message.
 *
 * The unit is host-testable through wolfram_stub; it needs no Toolbox header.
 */

#include <stddef.h>

#include "atproto_session.h"
#include "wolfram/xrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_atproto_client platinum_atproto_client;

/*
 * Called after a successful sign-in or refresh, so the platform can save the
 * refresh token. Return 0 for success. A failure does not discard the live
 * session: the session works, but the user will have to sign in again after a
 * restart. Never called for a failed sign-in.
 */
typedef int (*platinum_atproto_save_fn)(const platinum_atproto_session *session,
                                        void *userdata);

/* Client bound to a PDS service base. NULL when the URL is not https or the
 * transport cannot allocate. */
platinum_atproto_client *platinum_atproto_client_new(const char *pds_url);
void platinum_atproto_client_free(platinum_atproto_client *client);

/* Replace the live session (for example after loading a stored refresh token
 * from the Keychain) and point the transport at its access token. */
wf_status platinum_atproto_client_set_session(platinum_atproto_client *client,
                                              const platinum_atproto_session *session);
const platinum_atproto_session *platinum_atproto_client_session(
    const platinum_atproto_client *client);

void platinum_atproto_client_set_save(platinum_atproto_client *client,
                                      platinum_atproto_save_fn save,
                                      void *userdata);

/*
 * POST com.atproto.server.createSession with the identifier and password.
 * On success the client holds the session and the save hook is called.
 * The caller owns its password buffer and must wipe it afterwards; this
 * function also wipes its own request scratch.
 */
wf_status platinum_atproto_client_sign_in(platinum_atproto_client *client,
                                          const char *identifier,
                                          const char *password);

/*
 * Resolve `handle` to its PDS, create a client for it and sign in there.
 *
 * The sequence is the one in ADR 0008: the handle's HTTPS well-known document
 * first, resolveHandle on `fallback_service` (may be NULL) when that is
 * unavailable, then the DID document and its #atproto_pds service. The
 * resulting client is bound to the discovered PDS, holds the session and has
 * the save hook installed.
 *
 * On success `*out` owns the client; the caller frees it. On failure `*out` is
 * NULL and no client is returned, so a failed sign-in leaves nothing to use.
 */
wf_status platinum_atproto_client_connect(
    const char *handle, const char *password, const char *fallback_service,
    platinum_atproto_save_fn save, void *userdata,
    platinum_atproto_client **out);

/* POST com.atproto.server.refreshSession with the refresh token, replace the
 * session, and save it. A failure clears the session. */
wf_status platinum_atproto_client_refresh(platinum_atproto_client *client);

/*
 * Authenticated GET. `names`/`values` are query parameters; count may be zero.
 * On WF_OK `out` owns the response and must be freed with wf_response_free.
 * On auth failure the client refreshes once and retries exactly once.
 */
wf_status platinum_atproto_client_get(platinum_atproto_client *client,
                                      const char *nsid,
                                      const char *const *names,
                                      const char *const *values,
                                      size_t count, wf_response *out);

/* Authenticated POST with a JSON body. Same single refresh-and-retry rule.
 * The body must be well-formed JSON; it is checked before anything is sent. */
wf_status platinum_atproto_client_post(platinum_atproto_client *client,
                                       const char *nsid, const char *json_body,
                                       wf_response *out);

/* 1 when the sign-in scratch area holds only zeros. For tests and diagnostics. */
int platinum_atproto_client_scratch_clear(void);

#ifdef __cplusplus
}
#endif

#endif
