#ifndef PLATINUM_TOKEN_H
#define PLATINUM_TOKEN_H

#include <stddef.h>

/*
 * Bridge token store, C89, no allocation.
 *
 * A token is 32 random bytes, base64url without padding (43 characters).
 * Only the SHA-256 hex digest of the token is kept; the plaintext is returned
 * once from platinum_token_store_add and is never stored in the struct.
 * The store is bounded: when every slot is occupied, add refuses. Revoked
 * slots stay occupied, so nothing is evicted or reused.
 */

#define PLATINUM_TOKEN_SLOTS 64
#define PLATINUM_TOKEN_LEN 43
#define PLATINUM_TOKEN_BUF 64
#define PLATINUM_TOKEN_ID_MAX 40
#define PLATINUM_TOKEN_DID_MAX 128
#define PLATINUM_TOKEN_HASH_HEX 65

/* Returns 0 on success, nonzero on failure. */
typedef int (*platinum_random_fn)(unsigned char *buf, size_t len);

typedef struct platinum_token_slot {
    char id[PLATINUM_TOKEN_ID_MAX];           /* unique; empty id = free slot */
    char did[PLATINUM_TOKEN_DID_MAX];
    char token_hash_hex[PLATINUM_TOKEN_HASH_HEX];
    long created_at;                          /* seconds */
    int revoked;
    long last_used;                           /* seconds; 0 = never used */
} platinum_token_slot;

typedef struct platinum_token_store {
    platinum_token_slot slots[PLATINUM_TOKEN_SLOTS];
} platinum_token_store;

/* out receives 43 base64url characters plus NUL. Returns 0 or -1. */
int platinum_token_generate(platinum_random_fn rnd, char out[PLATINUM_TOKEN_BUF]);

void platinum_token_store_init(platinum_token_store *store);

/*
 * Issue a token for did under id. Returns the slot index, or -1 when the store
 * is full, the id is empty, too long or already in use, or the did is empty or
 * too long. out_token receives the plaintext token; it is written only on success.
 */
int platinum_token_store_add(platinum_token_store *store, platinum_random_fn rnd,
                             const char *did, long now, const char *id,
                             char out_token[PLATINUM_TOKEN_BUF]);

/* Returns the index of the live slot matching token, or -1. Updates last_used on success. */
int platinum_token_store_authenticate(platinum_token_store *store, const char *token, long now);

/* Returns 0 if a live token was revoked, -1 otherwise (including already revoked). */
int platinum_token_store_revoke(platinum_token_store *store, const char *token);

#endif
