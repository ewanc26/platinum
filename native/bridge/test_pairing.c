#include <stdio.h>
#include <string.h>

#include "pairing.h"

/* Copied from the spec so the test checks the real alphabet, not the header's. */
#define TEST_ALPHABET "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

/* Deterministic LCG. Gives different bytes on each call, so codes are distinct. */
static unsigned long fake_state = 1UL;

static int fake_rng(unsigned char *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        fake_state = fake_state * 1103515245UL + 12345UL;
        buf[i] = (unsigned char)((fake_state >> 16) & 0xFF);
    }
    return 0;
}

/* Every byte is 0x41, so every code is the same. Used to force collisions. */
static int const_rng(unsigned char *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] = 0x41;
    }
    return 0;
}

/* Every byte is 0xFF. Masking gives index 31, which is '9'. */
static int ff_rng(unsigned char *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] = 0xFF;
    }
    return 0;
}

static int failing_rng(unsigned char *buf, size_t len)
{
    (void)buf;
    (void)len;
    return -1;
}

static int urandom_rng(unsigned char *buf, size_t len)
{
    static FILE *f = NULL;

    if (f == NULL) {
        f = fopen("/dev/urandom", "rb");
    }
    if (f == NULL) {
        return -1;
    }
    return (fread(buf, 1, len, f) == len) ? 0 : -1;
}

static int in_alphabet(char c)
{
    return c != '\0' && strchr(TEST_ALPHABET, c) != NULL;
}

static int count_used(const platinum_pairing *p)
{
    int i;
    int n = 0;

    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        if (p->slots[i].used) {
            n++;
        }
    }
    return n;
}

static void test_alphabet_size(void)
{
    const char *a = TEST_ALPHABET;
    size_t i, j;

    CHECK(strlen(a) == 32);
    for (i = 0; i < strlen(a); i++) {
        for (j = i + 1; j < strlen(a); j++) {
            CHECK(a[i] != a[j]);
        }
    }
    /* 32 divides 256, so byte & 31 is uniform over the alphabet. */
    CHECK(256 % 32 == 0);
}

static void test_code_length_alphabet_and_uniqueness(void)
{
    platinum_pairing p;
    char codes[PLATINUM_PAIRING_SLOTS][7];
    int i, j, k;

    platinum_pairing_init(&p);
    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:x", 0, 600, codes[i]) == 0);
        CHECK(strlen(codes[i]) == 6);
        for (k = 0; k < 6; k++) {
            CHECK(in_alphabet(codes[i][k]));
        }
    }
    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        for (j = i + 1; j < PLATINUM_PAIRING_SLOTS; j++) {
            CHECK(strcmp(codes[i], codes[j]) != 0);
        }
    }
}

static void test_exchange_once(void)
{
    platinum_pairing p;
    char code[7];
    char did[128];

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:alice", 1000, 600, code) == 0);
    CHECK(platinum_pairing_exchange(&p, code, 1001, did) == 0);
    CHECK(strcmp(did, "did:plc:alice") == 0);
    CHECK(platinum_pairing_exchange(&p, code, 1002, did) == -1);
    CHECK(count_used(&p) == 0);
}

static void test_lowercase_exchange(void)
{
    platinum_pairing p;
    char code[7];
    char lower[7];
    char did[128];
    size_t i;

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:bob", 0, 600, code) == 0);
    for (i = 0; i < 7; i++) {
        lower[i] = (char)((code[i] >= 'A' && code[i] <= 'Z') ? code[i] - 'A' + 'a' : code[i]);
    }
    CHECK(platinum_pairing_exchange(&p, lower, 1, did) == 0);
    CHECK(strcmp(did, "did:plc:bob") == 0);
}

static void test_expired_code_fails_and_is_pruned(void)
{
    platinum_pairing p;
    char code[7];
    char did[128];

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:carol", 0, 10, code) == 0);
    CHECK(count_used(&p) == 1);
    CHECK(platinum_pairing_exchange(&p, code, 11, did) == -1);
    CHECK(count_used(&p) == 0);
    CHECK(platinum_pairing_exchange(&p, code, 11, did) == -1);
}

static void test_expiry_boundary_matches_ts(void)
{
    /* pairing.ts expires only when expiresAt < now, so now == expiresAt is still valid. */
    platinum_pairing p;
    char code[7];
    char did[128];

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:dan", 0, 10, code) == 0);
    CHECK(platinum_pairing_exchange(&p, code, 10, did) == 0);
}

static void test_table_full_until_expiry(void)
{
    platinum_pairing p;
    char codes[PLATINUM_PAIRING_SLOTS][7];
    char code[7];
    char did[128];
    int i;

    platinum_pairing_init(&p);
    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:f", 0, 600, codes[i]) == 0);
    }
    CHECK(count_used(&p) == PLATINUM_PAIRING_SLOTS);

    /* Full and nothing has expired yet. */
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:g", 100, 600, code) == -1);
    CHECK(count_used(&p) == PLATINUM_PAIRING_SLOTS);

    /* One exchange frees one slot. */
    CHECK(platinum_pairing_exchange(&p, codes[0], 100, did) == 0);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:g", 100, 600, code) == 0);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:h", 100, 600, code) == -1);

    /* At t=601 the 15 codes from t=0 (expires 600) are pruned. The t=100 code (expires 700) stays. */
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:i", 601, 600, code) == 0);
    CHECK(count_used(&p) == 2);
}

static void test_wrong_length_fails(void)
{
    platinum_pairing p;
    char code[7];
    char did[128];

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:j", 0, 600, code) == 0);
    CHECK(platinum_pairing_exchange(&p, "ABCDE", 1, did) == -1);
    CHECK(platinum_pairing_exchange(&p, "", 1, did) == -1);
    CHECK(platinum_pairing_exchange(&p, NULL, 1, did) == -1);
    {
        char longer[16];
        strcpy(longer, code);
        strcat(longer, "A");
        CHECK(platinum_pairing_exchange(&p, longer, 1, did) == -1);
    }
    CHECK(platinum_pairing_exchange(&p, " ", 1, did) == -1);
    /* The live code is still there. */
    CHECK(count_used(&p) == 1);
}

static void test_invalid_character_fails(void)
{
    platinum_pairing p;
    char code[7];
    char bad[7];
    char did[128];
    const char *bad_inputs[] = { "ABC0EF", "ABC1EF", "ABCIEF", "ABCOEF", "ABC-EF", "ABC EF", ("ABC\xC3" "EF") };
    size_t i;

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:k", 0, 600, code) == 0);

    for (i = 0; i < sizeof bad_inputs / sizeof bad_inputs[0]; i++) {
        CHECK(platinum_pairing_exchange(&p, bad_inputs[i], 1, did) == -1);
    }

    /* A live code with one character changed to 0 must not match. */
    memcpy(bad, code, 7);
    bad[0] = (bad[0] == '0') ? '2' : '0';
    CHECK(platinum_pairing_exchange(&p, bad, 1, did) == -1);
    CHECK(platinum_pairing_exchange(&p, code, 1, did) == 0);
}

static void test_mask_no_rejection_needed(void)
{
    platinum_pairing p;
    char code[7];
    char did[128];

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, ff_rng, "did:plc:l", 0, 600, code) == 0);
    CHECK(strcmp(code, "999999") == 0);
    CHECK(platinum_pairing_exchange(&p, "999999", 1, did) == 0);
}

static void test_collision_refused(void)
{
    platinum_pairing p;
    char code[7];
    char did[128];

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, const_rng, "did:plc:m", 0, 600, code) == 0);
    /* Every attempt collides with the live code, so create gives up rather than shadow it. */
    CHECK(platinum_pairing_create(&p, const_rng, "did:plc:n", 0, 600, code) == -1);
    CHECK(count_used(&p) == 1);
    CHECK(platinum_pairing_exchange(&p, "BBBBBB", 1, did) == 0);
    CHECK(strcmp(did, "did:plc:m") == 0);
}

static void test_rng_failure_leaves_table_unchanged(void)
{
    platinum_pairing p;
    char code[7];

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, failing_rng, "did:plc:o", 0, 600, code) == -1);
    CHECK(count_used(&p) == 0);
}

static void test_argument_validation(void)
{
    platinum_pairing p;
    char code[7];
    char long_did[129];
    char max_did[128];
    size_t i;

    platinum_pairing_init(&p);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:q", 0, 0, code) == -1);
    CHECK(platinum_pairing_create(&p, fake_rng, "did:plc:q", 0, -5, code) == -1);
    CHECK(platinum_pairing_create(&p, fake_rng, "", 0, 600, code) == -1);
    CHECK(platinum_pairing_create(&p, NULL, "did:plc:q", 0, 600, code) == -1);

    for (i = 0; i < sizeof long_did - 1; i++) {
        long_did[i] = 'd';
    }
    long_did[sizeof long_did - 1] = '\0';
    CHECK(platinum_pairing_create(&p, fake_rng, long_did, 0, 600, code) == -1);

    for (i = 0; i < sizeof max_did - 1; i++) {
        max_did[i] = 'd';
    }
    max_did[sizeof max_did - 1] = '\0';
    CHECK(platinum_pairing_create(&p, fake_rng, max_did, 0, 600, code) == 0);
    CHECK(count_used(&p) == 1);
}

static void test_urandom(void)
{
    platinum_pairing p;
    char codes[PLATINUM_PAIRING_SLOTS][7];
    char did[128];
    int i, k;

    platinum_pairing_init(&p);
    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        CHECK(platinum_pairing_create(&p, urandom_rng, "did:plc:urandom", 0, 600, codes[i]) == 0);
        CHECK(strlen(codes[i]) == 6);
        for (k = 0; k < 6; k++) {
            CHECK(in_alphabet(codes[i][k]));
        }
    }
    for (i = 0; i < PLATINUM_PAIRING_SLOTS; i++) {
        CHECK(platinum_pairing_exchange(&p, codes[i], 1, did) == 0);
    }
    CHECK(count_used(&p) == 0);
}

int main(void)
{
    test_alphabet_size();
    test_code_length_alphabet_and_uniqueness();
    test_exchange_once();
    test_lowercase_exchange();
    test_expired_code_fails_and_is_pruned();
    test_expiry_boundary_matches_ts();
    test_table_full_until_expiry();
    test_wrong_length_fails();
    test_invalid_character_fails();
    test_mask_no_rejection_needed();
    test_collision_refused();
    test_rng_failure_leaves_table_unchanged();
    test_argument_validation();
    test_urandom();

    if (failures) {
        fprintf(stderr, "test_pairing: %d check(s) failed\n", failures);
        return 1;
    }
    printf("test_pairing: all checks passed\n");
    return 0;
}
