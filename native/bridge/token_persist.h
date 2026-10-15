/*
 * Persistence for the token store (Part 4 under Part 3).
 *
 * The file holds the hashes and the bookkeeping of each slot, never a plaintext token,
 * so a copy of the file cannot be used to sign in. Format: a version line "platinum-tokens 1",
 * then one line per occupied slot: id, did, hash, created_at, revoked, last_used, tab
 * separated. A load that finds anything malformed changes nothing.
 */
#ifndef PLATINUM_NATIVE_TOKEN_PERSIST_H
#define PLATINUM_NATIVE_TOKEN_PERSIST_H

#include <stddef.h>
#include "token.h"

/* Writes the store into buf. Returns the length, or -1 when it does not fit or a field
 * holds a tab or newline. */
long platinum_token_store_serialize(const platinum_token_store *store, char *buf, size_t cap);

/* Replaces the store with the contents of buf (len bytes). Returns 0 on success, -1 when
 * the text is malformed; the store is unchanged on failure. */
int platinum_token_store_deserialize(platinum_token_store *store, const char *buf, size_t len);

/* Writes the store to dir/tokens.txt atomically. Returns 0 or a negative error. */
int platinum_token_store_save(const platinum_token_store *store, const char *dir);

/* Loads dir/tokens.txt into the store. Returns 0 when loaded, 1 when there is no file
 * (the store is left empty), or a negative error (the store is unchanged). */
int platinum_token_store_load(platinum_token_store *store, const char *dir);

#endif
