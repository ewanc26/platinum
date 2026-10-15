#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "sha256.h"
#include "token.h"

static int failures = 0;

static void check(int ok, const char *what)
{
    if (ok) {
        printf("ok   - %s\n", what);
    } else {
        printf("FAIL - %s\n", what);
        failures++;
    }
}

/* Host random source: 32 bytes from /dev/urandom. */
static int urandom_fill(unsigned char *buf, size_t len)
{
    FILE *f;
    size_t got;

    f = fopen("/dev/urandom", "rb");
    if (f == NULL) {
        return -1;
    }
    got = fread(buf, 1, len, f);
    fclose(f);
    return got == len ? 0 : -1;
}

/* Deterministic source for known-answer encoding tests. */
static int fill_ff(unsigned char *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] = 0xFF;
    }
    return 0;
}

static int fail_random(unsigned char *buf, size_t len)
{
    (void)buf;
    (void)len;
    return -1;
}

static int is_b64url(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '-' || c == '_';
}

static int find_bytes(const unsigned char *hay, size_t hay_len, const char *needle, size_t n)
{
    size_t i;

    if (n == 0 || hay_len < n) {
        return 0;
    }
    for (i = 0; i + n <= hay_len; i++) {
        if (memcmp(hay + i, needle, n) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_sha256(void)
{
    char hex[65];
    const char *m56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    platinum_sha256_hex((const unsigned char *)"", 0, hex);
    check(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0,
          "sha256(\"\") matches FIPS vector");

    platinum_sha256_hex((const unsigned char *)"abc", 3, hex);
    check(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0,
          "sha256(\"abc\") matches FIPS vector");

    check(strlen(m56) == 56, "two-block input is 56 bytes");
    platinum_sha256_hex((const unsigned char *)m56, 56, hex);
    check(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0,
          "sha256 56-byte two-block vector matches");
}

static void test_generate(void)
{
    char tok[PLATINUM_TOKEN_BUF];
    char expect[PLATINUM_TOKEN_BUF];
    int i;
    size_t j;
    int len_ok = 1;
    int nul_ok = 1;
    int alpha_ok = 1;
    int gen_ok = 1;

    for (i = 0; i < 50; i++) {
        if (platinum_token_generate(urandom_fill, tok) != 0) {
            gen_ok = 0;
            break;
        }
        if (strlen(tok) != PLATINUM_TOKEN_LEN) {
            len_ok = 0;
        }
        if (tok[PLATINUM_TOKEN_LEN] != '\0') {
            nul_ok = 0;
        }
        for (j = 0; j < PLATINUM_TOKEN_LEN; j++) {
            if (!is_b64url(tok[j])) {
                alpha_ok = 0;
            }
        }
    }
    check(gen_ok, "generate succeeds with /dev/urandom");
    check(len_ok, "generated token is 43 characters");
    check(nul_ok, "generated token is NUL-terminated at 43");
    check(alpha_ok, "generated token uses only the base64url alphabet");

    /* 32 bytes of 0xFF encode to 42 underscores then '8' in unpadded base64url. */
    memset(expect, '_', 42);
    expect[42] = '8';
    expect[43] = '\0';
    check(platinum_token_generate(fill_ff, tok) == 0 && strcmp(tok, expect) == 0,
          "base64url known answer for 32 x 0xFF");

    check(platinum_token_generate(fail_random, tok) == -1, "generate returns -1 when random fails");
    check(platinum_token_generate(urandom_fill, NULL) == -1, "generate rejects NULL output");
}

static void test_store_basic(void)
{
    platinum_token_store store;
    char tok[PLATINUM_TOKEN_BUF];
    char tok2[PLATINUM_TOKEN_BUF];
    long before_use;

    platinum_token_store_init(&store);

    check(platinum_token_store_add(&store, urandom_fill, "did:plc:alice", 1000L, "inst-a", tok) == 0,
          "first add takes slot 0");
    check(strlen(tok) == PLATINUM_TOKEN_LEN, "add returns a 43-character token");

    check(platinum_token_store_add(&store, urandom_fill, "did:plc:alice", 1000L, "inst-a", tok2) == -1,
          "duplicate id is refused");
    check(platinum_token_store_add(&store, urandom_fill, "", 1000L, "inst-e", tok2) == -1,
          "empty did is refused");
    check(platinum_token_store_add(&store, urandom_fill, "did:plc:alice", 1000L, "", tok2) == -1,
          "empty id is refused");

    check(platinum_token_store_authenticate(&store, tok, 2000L) == 0,
          "token authenticates to its slot");
    before_use = store.slots[0].last_used;
    check(before_use == 2000L, "authenticate updates last_used");
    check(platinum_token_store_authenticate(&store, "", 2100L) == -1, "empty token never authenticates");
    check(platinum_token_store_authenticate(&store, NULL, 2100L) == -1, "NULL token never authenticates");
    check(platinum_token_store_revoke(&store, "") == -1, "empty token cannot be revoked");

    check(platinum_token_store_add(&store, urandom_fill, "did:plc:carol", 1000L, "inst-c", tok2) == 1,
          "second token takes slot 1");
    check(platinum_token_store_authenticate(&store, tok2, 2200L) == 1,
          "second token authenticates to its own slot");
    check(platinum_token_store_authenticate(&store, tok, 2200L) == 0,
          "first token still authenticates to slot 0");
    check(platinum_token_store_authenticate(&store, "not-a-real-token", 2200L) == -1,
          "unknown token does not authenticate");
}

static void test_store_revoke(void)
{
    platinum_token_store store;
    char tok[PLATINUM_TOKEN_BUF];

    platinum_token_store_init(&store);
    platinum_token_store_add(&store, urandom_fill, "did:plc:bob", 1L, "inst-b", tok);

    check(platinum_token_store_revoke(&store, tok) == 0, "revoke a live token returns 0");
    check(platinum_token_store_authenticate(&store, tok, 3L) == -1,
          "authenticate after revoke fails");
    check(platinum_token_store_revoke(&store, tok) == -1, "double revoke returns -1");
    check(platinum_token_store_revoke(&store, "not-a-real-token") == -1,
          "revoke of unknown token returns -1");
}

static void test_store_full(void)
{
    platinum_token_store store;
    char toks[PLATINUM_TOKEN_SLOTS][PLATINUM_TOKEN_BUF];
    char tok_extra[PLATINUM_TOKEN_BUF];
    char id[16];
    int i;
    int idx_ok = 1;

    platinum_token_store_init(&store);
    for (i = 0; i < PLATINUM_TOKEN_SLOTS; i++) {
        sprintf(id, "tok-%d", i);
        if (platinum_token_store_add(&store, urandom_fill, "did:plc:full", 10L, id, toks[i]) != i) {
            idx_ok = 0;
        }
    }
    check(idx_ok, "store fills 64 slots in order");

    check(platinum_token_store_add(&store, urandom_fill, "did:plc:full", 10L, "overflow", tok_extra) == -1,
          "full store refuses a 65th add");
    check(platinum_token_store_authenticate(&store, toks[0], 20L) == 0,
          "refused add does not evict the first token");
    check(platinum_token_store_authenticate(&store, toks[PLATINUM_TOKEN_SLOTS - 1], 20L)
          == PLATINUM_TOKEN_SLOTS - 1, "last slot still authenticates when full");

    check(platinum_token_store_revoke(&store, toks[5]) == 0, "revoke inside a full store");
    check(platinum_token_store_add(&store, urandom_fill, "did:plc:full", 10L, "overflow", tok_extra) == -1,
          "revoked slot is not reused: store stays full");
}

static void test_no_plaintext_at_rest(void)
{
    platinum_token_store store;
    char tok[PLATINUM_TOKEN_BUF];
    char hex[PLATINUM_TOKEN_HASH_HEX];
    int idx;

    platinum_token_store_init(&store);
    idx = platinum_token_store_add(&store, urandom_fill, "did:plc:scan", 5L, "scan-1", tok);
    check(idx == 0, "add for at-rest scan");

    check(!find_bytes((const unsigned char *)&store, sizeof(store), tok, PLATINUM_TOKEN_LEN),
          "plaintext token absent from every byte of the store");

    platinum_sha256_hex((const unsigned char *)tok, PLATINUM_TOKEN_LEN, hex);
    check(find_bytes((const unsigned char *)&store, sizeof(store), hex, 64),
          "SHA-256 hex of the token is present in the store (scan is meaningful)");
}

int main(void)
{
    test_sha256();
    test_generate();
    test_store_basic();
    test_store_revoke();
    test_store_full();
    test_no_plaintext_at_rest();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all token tests passed\n");
    return 0;
}
