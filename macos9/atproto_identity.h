#ifndef PLATINUM_ATPROTO_IDENTITY_H
#define PLATINUM_ATPROTO_IDENTITY_H

/*
 * atproto_identity.h -- identity resolution for the native AT Protocol client.
 *
 * The bridge used to resolve a handle and read the DID document. The Mac does
 * it itself now, over the same verified transport as every other request:
 *
 *   handle --resolveHandle--> did --plc.directory or /.well-known/did.json-->
 *   DID document --service#atproto_pds--> PDS endpoint
 *
 * `did:plc` documents come from plc.directory. `did:web` documents come from
 * the host named in the DID, with the method's path and port rules applied.
 * A handle is resolved through the PDS/service the caller already dials, which
 * avoids depending on Open Transport's DNS for a TXT lookup.
 *
 * All functions are C89, allocate nothing, and never touch the network. The
 * caller fetches the URL and passes the body back to the parser, so the host
 * tests exercise the exact code the Mac runs using recorded responses.
 */

#include <stddef.h>

#include "wolfram/xrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * URL of the DID document for `did`, into `out`.
 *
 * did:plc -> https://plc.directory/<did>
 * did:web -> https://<host>[/<path>]/did.json, with the method-specific port
 *            and path rules applied
 *
 * Other methods are WF_ERR_UNSUPPORTED: the AT Protocol allows them, but this
 * client has no resolver for them and must not pretend a URL it cannot check
 * is a document. `out` is empty on failure.
 */
wf_status platinum_atproto_did_document_url(const char *did, char *out,
                                            size_t cap);

/*
 * URL of com.atproto.identity.resolveHandle for `handle`, on `service_base`.
 *
 * `service_base` is any PDS or AppView the caller already talks to; the answer
 * is a DID the caller then resolves through platinum_atproto_did_document_url.
 */
wf_status platinum_atproto_resolve_handle_url(const char *service_base,
                                              const char *handle, char *out,
                                              size_t cap);

/*
 * The handle's own HTTPS identity document:
 * `https://<handle>/.well-known/atproto-did`. The body is a DID as plain text,
 * not JSON. This is the protocol's first-class handle method; resolveHandle is
 * the fallback (ADR 0008).
 */
wf_status platinum_atproto_handle_wellknown_url(const char *handle, char *out,
                                                size_t cap);

/* Read the DID from a `/.well-known/atproto-did` body. Surrounding ASCII
 * whitespace is allowed; anything else that is not one valid DID is refused. */
wf_status platinum_atproto_handle_wellknown_did(const char *body, char *out,
                                                size_t cap);

/*
 * Read a DID document and write its `#atproto_pds` service endpoint into `out`.
 *
 * The document's `id` must exist; when `expected_did` is non-NULL it must also
 * equal it, so a document served for another identity cannot be used. The
 * service must be typed AtprotoPersonalDataServer and its endpoint must be an
 * https service base; anything else is skipped rather than trusted.
 *
 * Returns WF_ERR_DID_DOCUMENT_NOT_FOUND for an id mismatch or a missing id,
 * WF_ERR_NOT_FOUND when no usable PDS service is present, and WF_ERR_PARSE for
 * a body that is not a DID document.
 */
wf_status platinum_atproto_did_pds(const char *document_json,
                                   const char *expected_did, char *out,
                                   size_t cap);

/*
 * 1 when the DID document lists `at://<handle>` in `alsoKnownAs`, else 0.
 *
 * This is the bidirectional check ADR 0008 requires: a document for the right
 * DID that does not claim the handle that was resolved is not proof that the
 * account owns it. The comparison is case-insensitive because handles are.
 * A malformed document, a missing array or a missing entry all return 0.
 */
int platinum_atproto_did_claims_handle(const char *document_json,
                                       const char *handle);

#ifdef __cplusplus
}
#endif

#endif
