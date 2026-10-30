/*
 * Native sign-in and token lifecycle for the bridge.
 *
 * Owns the token store, the app-password failure limiter and the per-installation
 * session files. A token lookup returns a live Wolfram agent: for an app-password
 * installation the PDS session is restored from disk and refreshed, and for a
 * hosted-node installation the node bearer is installed on a client pointed at
 * the node.
 *
 * Not thread-safe. The bridge's libmicrohttpd daemon handles requests on one
 * internal polling thread, and tests call it serially.
 */
#ifndef PLATINUM_NATIVE_AUTH_H
#define PLATINUM_NATIVE_AUTH_H

#include <stddef.h>

#include "config.h"

typedef struct platinum_bridge_auth platinum_bridge_auth;

/* Returns NULL when cfg is NULL or an allocation fails. */
platinum_bridge_auth *platinum_auth_new(const platinum_bridge_config *cfg);
void platinum_auth_free(platinum_bridge_auth *auth);

/* Creates the data directory and loads tokens.txt. Returns 0 on success, 1 when
 * there is no token file yet, or -1 when the directory or file cannot be used. */
int platinum_auth_load(platinum_bridge_auth *auth);

/* platinum_token_lookup_fn: returns a borrowed agent for a live token, or NULL. */
void *platinum_auth_lookup(void *user, const char *token);

/* POST /v1/login/app-password. Returns the HTTP status. */
int platinum_auth_app_password_login(platinum_bridge_auth *auth, const char *body,
                                     size_t body_len, const char *peer, char *out,
                                     size_t cap);

/* POST /v1/revoke. Returns the HTTP status. */
int platinum_auth_revoke(platinum_bridge_auth *auth, const char *authorization,
                         char *out, size_t cap);

/* GET /login?handle=...: begins a pairing at the hosted OAuth node and returns
 * an HTML page with the node link and the six-character bridge code. Sets
 * *content_type when it is not NULL. */
int platinum_auth_login_page(platinum_bridge_auth *auth, const char *handle,
                             char *out, size_t cap, const char **content_type);

/* POST /v1/pair: polls the node once for the bridge code's pairing. Returns the
 * usual pairing JSON on success, 409 while the browser flow is still open, or
 * an error body. */
int platinum_auth_pair(platinum_bridge_auth *auth, const char *body,
                       size_t body_len, char *out, size_t cap);

/* Issues an installation token backed by a hosted-node session. Used by the
 * OAuth pairing route, and by tests to exercise lookup and persistence without
 * a live PDS. out_token has room for 64 bytes and out_id for 40. Returns 0 on
 * success, -1 on any failure (nothing is left behind). */
int platinum_auth_add_node_session(platinum_bridge_auth *auth, const char *service,
                                   const char *token, const char *handle,
                                   const char *did, char out_token[64],
                                   char out_id[40]);

/* Tests inspect these without opening the files. */
size_t platinum_auth_cached_agents(const platinum_bridge_auth *auth);
void platinum_auth_invalidate(platinum_bridge_auth *auth, const char *installation_id);

#endif /* PLATINUM_NATIVE_AUTH_H */
