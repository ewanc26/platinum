/*
 * Pairing table for the native bridge.
 *
 * Mirrors bridge/src/auth/pairing.ts (PairingService): an OAuth callback
 * creates a short one-time code bound to a DID; the app exchanges the code
 * for that DID at POST /v1/pair.
 *
 * Scope: this module stores only the code and the DID. The TypeScript
 * PairingRecord also carries a 32-char token. That token is not kept here;
 * it is issued by the token store (token.c) when the exchange succeeds.
 *
 * No heap allocation. The table is a fixed array of PLATINUM_PAIRING_SLOTS.
 * Not thread-safe; the caller serializes access.
 */
#ifndef PLATINUM_PAIRING_H
#define PLATINUM_PAIRING_H

#include <stddef.h>

#define PLATINUM_PAIRING_SLOTS 16

/* Fills buf with len random bytes. Returns 0 on success, nonzero on failure. */
typedef int (*platinum_pair_random_fn)(unsigned char *buf, size_t len);

typedef struct platinum_pairing_slot {
    int used;
    long expires_at;      /* unix seconds; expired when expires_at < now */
    char code[7];         /* 6 alphabet chars plus NUL */
    char did[128];        /* DID plus NUL, at most 127 chars */
} platinum_pairing_slot;

typedef struct platinum_pairing {
    platinum_pairing_slot slots[PLATINUM_PAIRING_SLOTS];
} platinum_pairing;

void platinum_pairing_init(platinum_pairing *p);

/*
 * Creates a code for did, valid for ttl_seconds from now, and writes it to
 * out_code (NUL-terminated). Expired slots are pruned first.
 * Returns 0 on success. Returns -1 if the table is still full after pruning,
 * if rnd fails, if did is empty or 128 or more chars, if ttl_seconds is not
 * positive, or if no unused code is found after a few attempts.
 */
int platinum_pairing_create(platinum_pairing *p, platinum_pair_random_fn rnd, const char *did, long now, long ttl_seconds, char out_code[7]);

/*
 * Exchanges a code for its DID. Matching is case-insensitive. A successful
 * exchange removes the slot, so a code works once. An expired match is
 * removed and returns -1. Input of the wrong length or with characters outside
 * the alphabet returns -1 without scanning the table.
 * Returns 0 on success, with the DID written to out_did (NUL-terminated).
 */
int platinum_pairing_exchange(platinum_pairing *p, const char *code, long now, char out_did[128]);

#endif /* PLATINUM_PAIRING_H */
