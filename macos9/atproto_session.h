#ifndef PLATINUM_ATPROTO_SESSION_H
#define PLATINUM_ATPROTO_SESSION_H

/*
 * atproto_session.h -- the native AT Protocol session.
 *
 * Phase 1 signs in with an app password through
 * com.atproto.server.createSession and keeps only what a restart needs: the
 * refresh JWT, the account DID and handle, and the PDS base URL. The access
 * JWT lives in memory and is replaced by refreshSession before it expires.
 *
 * The password never enters this module's persistent representation: the
 * caller uses it for one createSession request and wipes its own copy. Nothing
 * here logs a token, and the clear function overwrites every byte.
 *
 * Expiry is read from the access JWT's `exp` claim without verifying the
 * signature. That is deliberate: the claim only schedules a refresh, and the
 * server remains the authority on whether a token is accepted. It must never
 * be used as an authorisation decision.
 */

#include <stddef.h>

#include "wolfram/xrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_ATPROTO_JWT_MAX 2047
#define PLATINUM_ATPROTO_SESSION_DID_MAX 255
#define PLATINUM_ATPROTO_SESSION_HANDLE_MAX 253
#define PLATINUM_ATPROTO_SESSION_PDS_MAX 511

typedef struct platinum_atproto_session {
    char access_jwt[PLATINUM_ATPROTO_JWT_MAX + 1];
    char refresh_jwt[PLATINUM_ATPROTO_JWT_MAX + 1];
    char did[PLATINUM_ATPROTO_SESSION_DID_MAX + 1];
    char handle[PLATINUM_ATPROTO_SESSION_HANDLE_MAX + 1];
    char pds_url[PLATINUM_ATPROTO_SESSION_PDS_MAX + 1];
} platinum_atproto_session;

/* Zero the session, overwriting the tokens in a way the compiler may not drop. */
void platinum_atproto_session_init(platinum_atproto_session *session);
void platinum_atproto_session_clear(platinum_atproto_session *session);

/*
 * Parse a com.atproto.server.createSession response.
 *
 * `pds_url` is the service the request was sent to; it is normalised with
 * platinum_atproto_service_base and becomes the session's PDS. The body must
 * carry accessJwt, refreshJwt, did and handle, all of a plausible shape.
 *
 * On failure `out` is cleared, never half-filled. Returns WF_ERR_PARSE for a
 * missing or malformed field, WF_ERR_INVALID_ARG for a bad PDS URL, and
 * WF_ERR_ALLOC if a value does not fit.
 */
wf_status platinum_atproto_session_parse_create(const char *body,
                                                const char *pds_url,
                                                platinum_atproto_session *out);

/*
 * Parse a com.atproto.server.refreshSession response into `out`, keeping the
 * DID, handle and PDS from `previous` when the response omits them.
 *
 * `previous` and `out` may be the same object. On failure `out` is cleared and
 * the caller must sign in again; a refresh that cannot be parsed is not a
 * session. Returns WF_ERR_INVALID_ARG when `previous` is NULL.
 */
wf_status platinum_atproto_session_parse_refresh(
    const char *body, const platinum_atproto_session *previous,
    platinum_atproto_session *out);

/*
 * Unix time from the access JWT's `exp` claim, or 0 when this build cannot read
 * it. The token is not verified and a signature failure does not matter here.
 */
long platinum_atproto_session_access_expiry(
    const platinum_atproto_session *session);

/*
 * 1 when the access token's expiry is at or before `now + skew_seconds`.
 *
 * A session whose expiry cannot be read returns 0: refreshing on an unknown
 * clock would be a guess, and the server's 401 is the reliable signal.
 */
int platinum_atproto_session_access_expired(
    const platinum_atproto_session *session, long now, long skew_seconds);

/*
 * Map an XRPC error envelope (`{"error":"...","message":"..."}`) to a status.
 *
 * AuthRequired, ExpiredToken and InvalidToken become WF_ERR_AUTH so a caller
 * refreshes once before giving up; NotFound becomes WF_ERR_NOT_FOUND,
 * InvalidRequest becomes WF_ERR_VALIDATION and RateLimitExceeded becomes
 * WF_ERR_RATE_LIMIT. Anything else, including a body that is not an XRPC
 * error, is WF_ERR_HTTP. The message is never copied or logged.
 */
wf_status platinum_atproto_error_status(const char *body);

#ifdef __cplusplus
}
#endif

#endif
