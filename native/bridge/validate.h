/*
 * Input checks for the write and search routes (Part 8, slice 5).
 *
 * Each function matches a validator in bridge/src/domain/api.ts, with the same
 * accept and reject boundaries. They take the already-parsed string, so JSON
 * parsing and the body-size limit stay in the server layer (Part 10).
 */
#ifndef PLATINUM_NATIVE_VALIDATE_H
#define PLATINUM_NATIVE_VALIDATE_H

#include <stddef.h>

/* Counts Unicode code points in a UTF-8 string (what Array.from(text).length counts).
 * Returns -1 for NULL. */
long platinum_utf8_length(const char *s);

/* A reply the chat service takes: not blank, at most 1000 code points. 0 ok, -1 not. */
int platinum_valid_reply_text(const char *text);

/* A post reference: an at:// URI of an app.bsky.feed.post record (at most 512 bytes)
 * and a non-empty alphanumeric cid of at most 255 bytes. 0 ok, -1 not. */
int platinum_valid_post_ref(const char *uri, const char *cid);

/* A DID to follow, mute or block: must start "did:" and be a valid actor DID. 0 ok, -1 not. */
int platinum_valid_subject_did(const char *did);

/* A search query: trimmed, 1 to 100 code points, no control characters. Writes the
 * trimmed query into out (cap bytes). 0 ok, -1 not. */
int platinum_valid_query(const char *in, char *out, size_t cap);

#endif
