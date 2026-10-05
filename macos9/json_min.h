#ifndef PLATINUM_JSON_MIN_H
#define PLATINUM_JSON_MIN_H

/*
 * json_min.h -- the smallest JSON support the Mac OS 9 side needs.
 *
 * Why this exists instead of cJSON: the native client is built for
 * CodeWarrior-era C89 and links against Wolfram's wolfram-macos9-transport
 * target, which deliberately contains only the transport and its macTLS
 * dependency. cJSON is a C99 library that is not part of that target and that
 * a Classic Mac build cannot rely on, so depending on it here would make the
 * Mac client impossible to build for its actual platform.
 *
 * The bridge protocol is deliberately small, so this is not a general JSON
 * library. It does exactly three things, all bounded by an explicit output
 * capacity:
 *
 *   - escape a caller-supplied string into JSON string contents;
 *   - read a string or integer member out of a flat response object;
 *   - validate that a document is well-formed JSON before sending it.
 *
 * It deliberately does not build a tree, does not allocate, and does not
 * interpret nested structures: the client reads small flat objects and hands
 * larger response bodies back to the UI layer untouched.
 *
 * Every entry point is C89: declarations precede statements, no `//`
 * comments, no variadic macros, no compound literals, no VLAs.
 */

#include <stddef.h>

#include "wolfram/xrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Deepest nesting platinum_json_valid accepts. The limit counts array and
 * object levels, so a document may nest 15 deep and a 16th level is rejected
 * rather than recursed into. Bridge responses are small and flat; a limit
 * keeps a hostile or corrupt body from driving unbounded recursion on a 1990s
 * stack.
 */
#define PLATINUM_JSON_MAX_DEPTH 16

/*
 * Escape `src` as the *contents* of a JSON string (no surrounding quotes) into
 * `dst`, always NUL-terminating when `cap` is non-zero. Writes at most
 * `cap - 1` bytes plus the terminator.
 *
 * Control characters are escaped as the JSON two-character forms \b \f \n \r
 * \t, and anything else below 0x20 as \u00XX, so the result is always valid
 * JSON regardless of what the caller typed. `"` and `\` are escaped. Bytes
 * >= 0x20 are passed through unchanged, which is correct for the ASCII text
 * the bridge protocol uses.
 *
 * Returns WF_OK on success, WF_ERR_INVALID_ARG for a NULL destination or a
 * zero capacity, and WF_ERR_ALLOC when the escaped form does not fit. An
 * output that does not fit is reported as WF_ERR_ALLOC because that is what
 * Wolfram's own Mac OS 9 transport reports when one of its internal buffers
 * overflows; Platinum reuses the transport's status codes rather than
 * inventing a second error system for the Mac side.
 */
wf_status platinum_json_escape(char *dst, size_t cap, const char *src);

/*
 * Read the string member `name` from the JSON object in `body`.
 *
 * Handles the full JSON string grammar, including \" \\ \/ \b \f \n \r \t and
 * \uXXXX escapes with surrogate pairs, so a token or DID is decoded correctly
 * even though the bridge currently emits plain ASCII. A malformed escape, a raw
 * control character inside the string and an unpaired surrogate are rejected
 * rather than half-decoded. Bytes >= 0x20 are passed through as written: this
 * does not validate that the response is UTF-8, so a caller that intends to
 * render a value must treat a non-ASCII byte as untrusted text rather than as
 * a verified character.
 *
 * Only top-level members of `body` are considered; this is not a path lookup.
 * Returns WF_ERR_NOT_FOUND when the member is absent, WF_ERR_PARSE when the
 * document is malformed or the member is not a string, and WF_ERR_ALLOC when
 * the decoded value does not fit in `cap`.
 */
wf_status platinum_json_get_string(const char *body,
                                   const char *name,
                                   char *dst,
                                   size_t cap);

/*
 * Read the integer member `name` from the JSON object in `body` into `*out`.
 *
 * JSON has one number type; a fractional or exponential value is rejected
 * rather than silently truncated, because every integer the bridge protocol
 * uses is a count or a protocol version.
 *
 * Returns WF_ERR_NOT_FOUND, WF_ERR_PARSE, WF_ERR_INVALID_ARG or WF_ERR_ALLOC
 * as above.
 */
wf_status platinum_json_get_int(const char *body, const char *name, long *out);

/*
 * Check that `body` is a well-formed JSON document.
 *
 * This guards the Mac side against sending a request body it built wrongly,
 * and is used on responses before they are handed to code that assumes a
 * particular shape. Returns WF_OK or WF_ERR_PARSE.
 */
wf_status platinum_json_valid(const char *body);

#ifdef __cplusplus
}
#endif

#endif
