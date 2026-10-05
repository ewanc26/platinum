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
 * It deliberately does not build a tree and does not allocate. A cursor is
 * two pointers into the caller's own buffer, so walking into a nested object
 * or along an array costs nothing but a few words of stack. The timeline and
 * notification screens need exactly that much: one array of objects, each with
 * a nested author object, read straight out of a response body that is already
 * in memory and is about to be freed.
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
 * A cursor into a JSON document.
 *
 * Two pointers, no allocation, no ownership: `text` is the whole
 * NUL-terminated document and `at` is the first byte of the value this cursor
 * points at. A cursor stays valid exactly as long as its document does, which
 * for every caller here means as long as the `wf_response` holding the body.
 *
 * Never memset this and never build one by hand; open it with
 * platinum_json_open.
 */
typedef struct {
    const char *text;
    const char *at;
} platinum_json;

/*
 * Validate `body` in full and position `*out` at its root value.
 *
 * The whole document is validated here, once, rather than at each accessor.
 * That is the same guarantee the flat accessors give -- a response that parses
 * as far as the member being read but is truncated after it is a broken
 * response -- and doing it once means navigation inside a validated document
 * cannot hand back half a token.
 *
 * Returns WF_ERR_INVALID_ARG, WF_ERR_PARSE or WF_OK.
 */
wf_status platinum_json_open(platinum_json *out, const char *body);

/*
 * Descend into the member `name` of the object `object`, producing a cursor
 * over that member's value.
 *
 * The member need not be an object or an array; this only positions a cursor.
 * Use platinum_json_string, platinum_json_int or platinum_json_bool to read it
 * with a type check, or platinum_json_member and platinum_json_element to go
 * deeper.
 *
 * Returns WF_ERR_INVALID_ARG, WF_ERR_PARSE if `object` is not an object or the
 * document does not match, or WF_ERR_NOT_FOUND if the member is absent.
 */
wf_status platinum_json_member(platinum_json object,
                               const char *name,
                               platinum_json *out);

/*
 * Descend into element `index` of the array `array`, counting elements from
 * zero. Returns WF_ERR_NOT_FOUND when the array is shorter than that.
 */
wf_status platinum_json_element(platinum_json array,
                                long index,
                                platinum_json *out);

/*
 * Write the number of elements in the array `array` to `*out`. Returns
 * WF_ERR_PARSE if `array` is not an array.
 */
wf_status platinum_json_count(platinum_json array, long *out);

/*
 * Read a string member of `object`.
 *
 * platinum_json_string fails with WF_ERR_ALLOC when the value does not fit,
 * which is what a caller reading a token or a DID wants: a half-read token is
 * worse than no token.
 *
 * platinum_json_string_truncating copies as much as fits, always
 * NUL-terminates, and returns WF_OK for a value of any length. Use it only for
 * text that is displayed, never for an identifier. A value that does not fit is
 * still bounded by `cap`, so a hostile response cannot widen a buffer.
 */
wf_status platinum_json_string(platinum_json object,
                              const char *name,
                              char *dst,
                              size_t cap);

wf_status platinum_json_string_truncating(platinum_json object,
                                          const char *name,
                                          char *dst,
                                          size_t cap);

/*
 * Read an integer member of `object`. A fractional or exponential value is
 * rejected rather than truncated.
 */
wf_status platinum_json_int(platinum_json object, const char *name, long *out);

/*
 * Read a boolean member of `object`, writing 0 or 1 to `*out`. Only `true` and
 * `false` are booleans; a string or a number is rejected rather than coerced.
 */
wf_status platinum_json_bool(platinum_json object, const char *name, int *out);

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
