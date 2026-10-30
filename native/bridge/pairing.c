#include <ctype.h>
#include <limits.h>
#include <string.h>

#include "pairing.h"

/* Same 32-character alphabet as bridge/src/auth/pairing.ts. I, O, 0 and 1 are excluded. */
#define PLATINUM_PAIRING_ALPHABET "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
#define PLATINUM_PAIRING_CODE_LEN 6
#define PLATINUM_PAIRING_DID_MAX 128
#define PLATINUM_PAIRING_CREATE_ATTEMPTS 8

static void clear_slot(platinum_pairing_slot *s)
{
    s->used = 0;
    s->expires_at = 0;
    s->code[0] = '\0';
    s->did[0] = '\0';
}

static void prune_expired(platinum_pairing *p, long now)
{
    int i;

    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        if (p->slots[i].used && p->slots[i].expires_at < now) {
            clear_slot(&p->slots[i]);
        }
    }
}

static int find_free(const platinum_pairing *p)
{
    int i;

    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        if (!p->slots[i].used) {
            return i;
        }
    }
    return -1;
}

/* Only used slots are compared. Codes are uppercase and of fixed length. */
static int find_code(const platinum_pairing *p, const char *code)
{
    int i;

    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        if (p->slots[i].used && strcmp(p->slots[i].code, code) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Maps 6 random bytes to 6 code characters.
 *
 * Uniformity: the alphabet has 32 characters and 32 divides 256, so each byte
 * value 0..255 maps to exactly one index with byte & 31. Each of the 32
 * characters is hit by exactly 8 byte values, so the output is uniform. A
 * rejection loop would never reject anything here, so there is none. If the
 * alphabet length ever stops being a power of two, this function must switch
 * to rejection sampling.
 */
static void code_from_bytes(const unsigned char *bytes, char out[7])
{
    size_t i;

    for (i = 0; i < PLATINUM_PAIRING_CODE_LEN; i++) {
        out[i] = PLATINUM_PAIRING_ALPHABET[bytes[i] & 31];
    }
    out[PLATINUM_PAIRING_CODE_LEN] = '\0';
}

void platinum_pairing_init(platinum_pairing *p)
{
    int i;

    if (p == NULL) {
        return;
    }
    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        clear_slot(&p->slots[i]);
    }
}

int platinum_pairing_create(platinum_pairing *p, platinum_pair_random_fn rnd, const char *did, long now, long ttl_seconds, char out_code[7])
{
    unsigned char bytes[PLATINUM_PAIRING_CODE_LEN];
    char code[7];
    size_t did_len;
    int slot;
    int attempt;

    if (p == NULL || rnd == NULL || did == NULL || out_code == NULL) {
        return -1;
    }
    if (ttl_seconds <= 0 || now > LONG_MAX - ttl_seconds) {
        return -1;
    }
    did_len = strlen(did);
    if (did_len == 0 || did_len >= PLATINUM_PAIRING_DID_MAX) {
        return -1;
    }

    prune_expired(p, now);
    slot = find_free(p);
    if (slot < 0) {
        return -1;
    }

    /* A live duplicate would shadow an earlier code, so retry on collision. */
    for (attempt = 0; attempt < PLATINUM_PAIRING_CREATE_ATTEMPTS; attempt++) {
        if (rnd(bytes, sizeof bytes) != 0) {
            return -1;
        }
        code_from_bytes(bytes, code);
        if (find_code(p, code) < 0) {
            p->slots[slot].used = 1;
            p->slots[slot].expires_at = now + ttl_seconds;
            memcpy(p->slots[slot].code, code, sizeof code);
            memcpy(p->slots[slot].did, did, did_len + 1);
            memcpy(out_code, code, sizeof code);
            return 0;
        }
    }
    return -1;
}

int platinum_pairing_exchange(platinum_pairing *p, const char *code, long now, char out_did[128])
{
    char key[PLATINUM_PAIRING_CODE_LEN + 1];
    size_t len;
    size_t j;
    int slot;

    if (p == NULL || code == NULL || out_did == NULL) {
        return -1;
    }

    /* Validate before touching the table. */
    len = strlen(code);
    if (len != PLATINUM_PAIRING_CODE_LEN) {
        return -1;
    }
    for (j = 0; j < PLATINUM_PAIRING_CODE_LEN; j++) {
        int c = toupper((unsigned char)code[j]);
        if (strchr(PLATINUM_PAIRING_ALPHABET, c) == NULL) {
            return -1;
        }
        key[j] = (char)c;
    }
    key[PLATINUM_PAIRING_CODE_LEN] = '\0';

    slot = find_code(p, key);
    if (slot < 0) {
        return -1;
    }

    /* An expired match is dropped and not returned. Expired means expires_at < now, as in pairing.ts. */
    if (p->slots[slot].expires_at < now) {
        clear_slot(&p->slots[slot]);
        return -1;
    }

    memcpy(out_did, p->slots[slot].did, sizeof p->slots[slot].did);
    clear_slot(&p->slots[slot]);
    return 0;
}
