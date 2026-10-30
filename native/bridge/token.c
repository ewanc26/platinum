#include <string.h>

#include "sha256.h"
#include "token.h"

#define PLATINUM_TOKEN_RAW 32
#define PLATINUM_TOKEN_HEX 64

static const char B64URL[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* Unpadded base64url. Writes 4 chars per full 3-byte group, then 2 or 3 for the tail. */
static void b64url_encode(const unsigned char *in, size_t len, char *out)
{
    size_t i = 0;
    size_t o = 0;
    unsigned long v;

    while (len - i >= 3) {
        v = ((unsigned long)in[i] << 16) | ((unsigned long)in[i + 1] << 8) | (unsigned long)in[i + 2];
        out[o++] = B64URL[(v >> 18) & 63];
        out[o++] = B64URL[(v >> 12) & 63];
        out[o++] = B64URL[(v >> 6) & 63];
        out[o++] = B64URL[v & 63];
        i += 3;
    }
    if (len - i == 1) {
        v = (unsigned long)in[i] << 16;
        out[o++] = B64URL[(v >> 18) & 63];
        out[o++] = B64URL[(v >> 12) & 63];
    } else if (len - i == 2) {
        v = ((unsigned long)in[i] << 16) | ((unsigned long)in[i + 1] << 8);
        out[o++] = B64URL[(v >> 18) & 63];
        out[o++] = B64URL[(v >> 12) & 63];
        out[o++] = B64URL[(v >> 6) & 63];
    }
    out[o] = '\0';
}

/* Compares every byte, with no early exit on a mismatch. */
static int ct_equal(const char *a, const char *b, size_t n)
{
    unsigned int diff = 0;
    size_t i;

    for (i = 0; i < n; i++) {
        diff |= (unsigned int)((unsigned char)a[i] ^ (unsigned char)b[i]);
    }
    return diff == 0;
}

/*
 * Index of the occupied slot whose hash equals token_hash_hex, or -1.
 * Scans every slot without stopping early, so timing does not reveal the match position.
 */
static int find_by_hash(platinum_token_store *store, const char *hex)
{
    int found = -1;
    int i;
    int eq;
    int occupied;

    for (i = 0; i < PLATINUM_TOKEN_SLOTS; i++) {
        eq = ct_equal(store->slots[i].token_hash_hex, hex, PLATINUM_TOKEN_HEX);
        occupied = store->slots[i].id[0] != '\0';
        if (eq && occupied && found < 0) {
            found = i;
        }
    }
    return found;
}

static void hash_token(const char *token, char out_hex[PLATINUM_TOKEN_HASH_HEX])
{
    platinum_sha256_hex((const unsigned char *)token, strlen(token), out_hex);
}

int platinum_token_generate(platinum_random_fn rnd, char out[PLATINUM_TOKEN_BUF])
{
    unsigned char raw[PLATINUM_TOKEN_RAW];

    if (rnd == NULL || out == NULL) {
        return -1;
    }
    if (rnd(raw, sizeof(raw)) != 0) {
        memset(raw, 0, sizeof(raw));
        return -1;
    }
    b64url_encode(raw, sizeof(raw), out);
    memset(raw, 0, sizeof(raw));
    return 0;
}

void platinum_token_store_init(platinum_token_store *store)
{
    memset(store, 0, sizeof(*store));
}

int platinum_token_store_add(platinum_token_store *store, platinum_random_fn rnd,
                             const char *did, long now, const char *id,
                             char out_token[PLATINUM_TOKEN_BUF])
{
    char token[PLATINUM_TOKEN_BUF];
    platinum_token_slot *slot;
    int free_index = -1;
    int i;

    if (store == NULL || rnd == NULL || did == NULL || id == NULL || out_token == NULL) {
        return -1;
    }
    if (did[0] == '\0' || strlen(did) >= PLATINUM_TOKEN_DID_MAX) {
        return -1;
    }
    if (id[0] == '\0' || strlen(id) >= PLATINUM_TOKEN_ID_MAX) {
        return -1;
    }

    /* Refuse a duplicate id, and pick the first free slot. Nothing is evicted. */
    for (i = 0; i < PLATINUM_TOKEN_SLOTS; i++) {
        if (store->slots[i].id[0] == '\0') {
            if (free_index < 0) {
                free_index = i;
            }
        } else if (strcmp(store->slots[i].id, id) == 0) {
            return -1;
        }
    }
    if (free_index < 0) {
        return -1;
    }

    if (platinum_token_generate(rnd, token) != 0) {
        return -1;
    }

    slot = &store->slots[free_index];
    strcpy(slot->id, id);
    strcpy(slot->did, did);
    hash_token(token, slot->token_hash_hex);
    slot->created_at = now;
    slot->revoked = 0;
    slot->last_used = 0;

    strcpy(out_token, token);
    memset(token, 0, sizeof(token));
    return free_index;
}

int platinum_token_store_authenticate(platinum_token_store *store, const char *token, long now)
{
    char hex[PLATINUM_TOKEN_HASH_HEX];
    int index;

    if (store == NULL || token == NULL || token[0] == '\0') {
        return -1;
    }

    hash_token(token, hex);
    index = find_by_hash(store, hex);
    if (index < 0 || store->slots[index].revoked) {
        return -1;
    }
    store->slots[index].last_used = now;
    return index;
}

int platinum_token_store_revoke(platinum_token_store *store, const char *token)
{
    char hex[PLATINUM_TOKEN_HASH_HEX];
    int index;

    if (store == NULL || token == NULL || token[0] == '\0') {
        return -1;
    }

    hash_token(token, hex);
    index = find_by_hash(store, hex);
    if (index < 0 || store->slots[index].revoked) {
        return -1;
    }
    store->slots[index].revoked = 1;
    return 0;
}
