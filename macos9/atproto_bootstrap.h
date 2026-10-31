#ifndef PLATINUM_ATPROTO_BOOTSTRAP_H
#define PLATINUM_ATPROTO_BOOTSTRAP_H

/*
 * atproto_bootstrap.h -- handle to PDS, over verified transport.
 *
 * This is the sequence a client needs before it can sign in without a
 * preconfigured server (ADR 0008):
 *
 *   handle --https://<handle>/.well-known/atproto-did--> did
 *          --resolveHandle on a known service, if that is unavailable--> did
 *   did    --plc.directory or did:web--> DID document
 *   document --#atproto_pds--> PDS service base
 *
 * The module owns no transport. The caller supplies a fetch callback, so the
 * host tests drive the exact request order and bodies with recorded fixtures,
 * and the Mac build passes Wolfram's `wf_http_get_limited`. Nothing here
 * allocates: the only buffers are the caller's outputs and the response the
 * callback returns.
 *
 * A handle's well-known result is not trusted on its own. The DID document
 * must have the resolved `id` and must list `at://<handle>` in `alsoKnownAs`;
 * a document that merely names a PDS for some other account is refused.
 */

#include <stddef.h>

#include "atproto_url.h"
#include "wolfram/xrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A handle's identity document is a DID, not JSON; 4 KiB is far more than one
 * line needs and still bounds a hostile response. */
#define PLATINUM_ATPROTO_WELLKNOWN_MAX 4096

/* A real PLC document is a few kilobytes. This cap keeps a hostile or broken
 * document from reaching the parser at all. */
#define PLATINUM_ATPROTO_DOCUMENT_MAX 16384

/*
 * Fetch `url` over the caller's verified transport.
 *
 * Writes the response into `out` (which the caller must release with
 * wf_response_free) and returns WF_OK for a 2xx response, WF_ERR_HTTP for a
 * non-2xx one, or the transport's failure status. `max_bytes` is a body cap
 * for this request; the callback must enforce it rather than buffer first.
 */
typedef wf_status (*platinum_atproto_fetch_fn)(void *userdata, const char *url,
                                               size_t max_bytes,
                                               wf_response *out);

/* Which of the two handle methods produced the DID. Reported so the sign-in
 * window and Preferences can say how the account was found instead of implying
 * one route was used. */
typedef enum {
    PLATINUM_ATPROTO_ENTRY_NONE = 0,
    PLATINUM_ATPROTO_ENTRY_WELLKNOWN = 1,
    PLATINUM_ATPROTO_ENTRY_RESOLVE_HANDLE = 2
} platinum_atproto_entryway;

/*
 * Resolve `handle` to `did_out` and its PDS to `pds_out`.
 *
 * `fallback_service` is an https PDS or AppView used for
 * com.atproto.identity.resolveHandle when the handle serves no well-known
 * document. It may be NULL, in which case a handle without a well-known
 * document is unresolved rather than guessed.
 *
 * Both outputs are empty on failure. Returns:
 *   WF_ERR_INVALID_ARG            bad handle, missing fetch or output
 *   WF_ERR_HANDLE_DOCUMENT_NOT_FOUND  no well-known DID and no usable fallback
 *   WF_ERR_DID_DOCUMENT_NOT_FOUND a document 404, or one for another DID
 *   WF_ERR_HANDLE_RESOLVE         the document does not claim this handle
 *   WF_ERR_PARSE                  a 2xx body is not the expected shape
 *   WF_ERR_HTTP / transport       the request failed
 *   WF_ERR_ALLOC                  an output is too small
 */
wf_status platinum_atproto_bootstrap(platinum_atproto_fetch_fn fetch,
                                     void *userdata, const char *handle,
                                     const char *fallback_service,
                                     char *did_out, size_t did_cap,
                                     char *pds_out, size_t pds_cap);

/*
 * As platinum_atproto_bootstrap, also writing the handle method that produced
 * the DID to `*entryway` (NONE on failure). `entryway` may be NULL when the
 * caller does not care. The two functions are otherwise identical.
 */
wf_status platinum_atproto_bootstrap_ex(
    platinum_atproto_fetch_fn fetch, void *userdata, const char *handle,
    const char *fallback_service, char *did_out, size_t did_cap,
    char *pds_out, size_t pds_cap, platinum_atproto_entryway *entryway);

#ifdef __cplusplus
}
#endif

#endif
