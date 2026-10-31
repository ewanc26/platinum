#ifndef PLATINUM_ATPROTO_URL_H
#define PLATINUM_ATPROTO_URL_H

/*
 * atproto_url.h -- URLs and identifiers for the native AT Protocol client.
 *
 * This module is the boundary the bridge used to be. It builds the addresses
 * the Mac dials itself: a PDS service base, an XRPC method URL with encoded
 * parameters, and the URLs identity resolution needs. It knows no lexicon and
 * holds no records; it only refuses malformed input before Wolfram's transport
 * is asked to send anything.
 *
 * Everything here is strict C89 with no Toolbox, no allocation and no network.
 * The Mac target and the host tests compile the same source.
 */

#include <stddef.h>

#include "wolfram/xrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Longest URL this module will produce. XRPC cursors are bounded upstream at
 * 255 bytes, so a query URL is comfortably inside this once escaped. */
#define PLATINUM_ATPROTO_URL_MAX 512
#define PLATINUM_ATPROTO_DID_MAX 255
#define PLATINUM_ATPROTO_HANDLE_MAX 253
#define PLATINUM_ATPROTO_NSID_MAX 127
#define PLATINUM_ATPROTO_AT_URI_MAX 512
#define PLATINUM_ATPROTO_PARAMS_MAX 8

/*
 * Normalise an `https://host[:port]` service base.
 *
 * The scheme must be https: there is deliberately no plain-http option. The
 * host is lowercased, a trailing slash is accepted and removed, and a path,
 * query, fragment or userinfo is refused. `out` is empty on failure.
 *
 * A PDS is always reached over TLS and the certificate is verified by macTLS;
 * accepting an http base here would let a caller opt out of that by accident.
 */
wf_status platinum_atproto_service_base(const char *input, char *out,
                                        size_t cap);

/*
 * Build `https://host/xrpc/<nsid>?name=value&...`.
 *
 * Names are literal query parameter names (letters, digits, dot, underscore,
 * hyphen); values are percent-encoded with the RFC 3986 unreserved set. A
 * NULL or empty `values` entry is refused rather than sent as `name=`, because
 * every parameter this client sends is required.
 *
 * Returns WF_ERR_INVALID_ARG for a bad service base, NSID, parameter name or
 * count, and WF_ERR_ALLOC when the result does not fit in `cap`.
 */
wf_status platinum_atproto_xrpc_url(const char *service_base, const char *nsid,
                                    const char *const *names,
                                    const char *const *values, size_t count,
                                    char *out, size_t cap);

/*
 * Percent-encode `value` for a query string: A-Z a-z 0-9 - . _ ~ are kept and
 * everything else becomes %XX. Always NUL-terminates. Returns WF_ERR_ALLOC if
 * the result does not fit, in which case `out` is empty.
 */
wf_status platinum_atproto_escape(const char *value, char *out, size_t cap);

/* 1 for a syntactically valid DID, else 0. The method is lowercase; the
 * method-specific identifier may use letters, digits, dot, underscore, colon,
 * percent and hyphen. The total is bounded to 255 bytes. */
int platinum_atproto_valid_did(const char *did);

/* 1 for a DNS-shaped handle: at least two labels, each 1 to 63 characters of
 * letters, digits or hyphen, not starting or ending with a hyphen. Handles are
 * case-insensitive and are accepted in either case; this does not lowercase. */
int platinum_atproto_valid_handle(const char *handle);

/* 1 for a lowercase NSID with at least three dot-separated segments, beginning
 * and ending with a letter. */
int platinum_atproto_valid_nsid(const char *nsid);

/* 1 for `at://<did-or-handle>/<collection>/<rkey>` with a valid collection
 * NSID and a non-empty record key. No query, fragment or extra path. */
int platinum_atproto_valid_at_uri(const char *uri);

#ifdef __cplusplus
}
#endif

#endif
