#ifndef PLATINUM_BRIDGE_CLIENT_H
#define PLATINUM_BRIDGE_CLIENT_H

#include <stddef.h>

#include "wolfram/xrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_bridge_client platinum_bridge_client;

typedef struct platinum_bridge_pairing {
    int protocol;
    char *token;
    char *did;
    char *installation_id;
} platinum_bridge_pairing;

/* Overwrite `size` bytes at `data` in a way the compiler may not drop. */
void platinum_bridge_wipe(void *data, size_t size);

/* Why an app-password sign-in was refused, from the bridge's error code. */
enum {
    PLATINUM_LOGIN_OTHER = 0,
    PLATINUM_LOGIN_DISABLED = 1,    /* app_password_disabled */
    PLATINUM_LOGIN_INVALID = 2,     /* invalid_credentials */
    PLATINUM_LOGIN_TOO_MANY = 3,    /* too_many_attempts */
    PLATINUM_LOGIN_BAD_SERVICE = 4, /* invalid_service */
    PLATINUM_LOGIN_BAD_REQUEST = 5  /* invalid_request */
};

/*
 * POST /v1/login/app-password with a handle and an app password (both UTF-8,
 * at most 256 bytes, no control characters), answered exactly like pairing. The
 * request is built in one static scratch area that is overwritten before this
 * returns, on every path, so the password is not left in a heap block or on the
 * stack by this code. `reason` (optional) says why a refusal happened. On
 * success `out` holds the token as for platinum_bridge_pair. The caller wipes its
 * own copy of the password. The transport keeps its own copy of the request
 * while it sends it; that is Wolfram's to clear, not this function's.
 */
wf_status platinum_bridge_login_app_password(platinum_bridge_client *client,
                                             const char *identifier,
                                             const char *password,
                                             platinum_bridge_pairing *out,
                                             int *reason);
/* 1 if the sign-in scratch area holds only zeros. For tests and diagnostics. */
int platinum_bridge_login_scratch_clear(void);

platinum_bridge_client *platinum_bridge_client_new(const char *base_url);
void platinum_bridge_client_free(platinum_bridge_client *client);

wf_status platinum_bridge_client_set_token(platinum_bridge_client *client,
                                           const char *token);

wf_status platinum_bridge_pair(platinum_bridge_client *client,
                               const char *code,
                               platinum_bridge_pairing *out);
void platinum_bridge_pairing_free(platinum_bridge_pairing *pairing);

wf_status platinum_bridge_get(platinum_bridge_client *client,
                              const char *path,
                              wf_response *out);

wf_status platinum_bridge_post(platinum_bridge_client *client,
                               const char *path,
                               const char *json_body,
                               wf_response *out);

/*
 * Percent-encode `value` for use as a query-string value: everything except
 * RFC 3986 unreserved characters (A-Z a-z 0-9 - . _ ~) becomes %XX. Writes a
 * NUL-terminated result into `out` and returns its length, or -1 if it does
 * not fit (in which case `out` holds an empty string).
 */
/*
 * Tell the bridge the account has read notifications up to `seen_at`, an
 * indexedAt exactly as the bridge sent it (POST /v1/notifications/seen). Refuses
 * an empty or over-long value without making a request.
 */
wf_status platinum_bridge_mark_seen(platinum_bridge_client *client,
                                    const char *seen_at);

#define PLATINUM_MUTED_WORD_BYTES 400

/*
 * Add (on) or remove (off) a muted word: POST /v1/muted-words
 * {"value":"...","on":true}. `utf8_word` is escaped; empty or more than
 * PLATINUM_MUTED_WORD_BYTES is refused with no request. The bridge applies the filter.
 */
wf_status platinum_bridge_set_muted_word(platinum_bridge_client *client,
                                         const char *utf8_word, int on);

/* Who may reply to a new post. Index 0 is the default and sends nothing. */
#define PLATINUM_REPLY_GATE_COUNT 5
/* The bridge's name for gate `index` ("everyone", "nobody", "mentioned",
 * "following", "followers"); NULL out of range. */
const char *platinum_bridge_reply_gate_name(int index);
/* What the writer reads: "Everyone", "Nobody", "People you mention", ... */
const char *platinum_bridge_reply_gate_label(int index);

/* 1 when a POST /v1/post reply says the post landed but its reply gate did
 * not ("replyGateApplied":false); 0 for anything else, including a body that
 * does not parse. */
int platinum_bridge_reply_gate_failed(const char *response_body);

/*
 * As platinum_bridge_post_body, plus an optional quote ("quote":{uri,cid}, both
 * or neither, else NULL is returned) and a reply gate index (0 omits it; a
 * gate on a reply is refused with NULL, as the bridge refuses it too).
 */
char *platinum_bridge_post_body_ex(const char *utf8_text,
                                   const char *reply_uri,
                                   const char *reply_cid,
                                   const char *quote_uri,
                                   const char *quote_cid,
                                   int reply_gate);

/*
 * Build the JSON body for POST /v1/post: {"text":"..."}, plus
 * "replyTo":{"uri":"...","cid":"..."} when both reply arguments are non-empty.
 * `utf8_text` is escaped, as are the reply identifiers. The caller frees the
 * result with free(). Returns NULL for NULL text or on allocation failure; a
 * reply with only one of uri and cid is refused (NULL) rather than sent as a
 * plain post.
 */
char *platinum_bridge_post_body(const char *utf8_text,
                                const char *reply_uri,
                                const char *reply_cid);

long platinum_bridge_query_escape(const char *value, char *out, long capacity);

wf_status platinum_bridge_revoke(platinum_bridge_client *client,
                                 wf_response *out);

#ifdef __cplusplus
}
#endif

#endif
