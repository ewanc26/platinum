#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "fsstore.h"
#include "sha256.h"
#include "token.h"
#include "token_persist.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int
urandom(unsigned char *buf, size_t len)
{
    FILE *f = fopen("/dev/urandom", "rb");
    size_t got;
    if (f == NULL) return -1;
    got = fread(buf, 1, len, f);
    fclose(f);
    return got == len ? 0 : -1;
}

static int
slot_equal(const platinum_token_store *a, const platinum_token_store *b)
{
    return memcmp(a->slots, b->slots, sizeof a->slots) == 0;
}

static void
make_dir(char *dir, size_t cap)
{
    const char *base = getenv("TMPDIR");
    if (base == NULL || base[0] == '\0') base = "/tmp";
    sprintf(dir, "%s/platinum-tokpersist-%ld-%ld", base, (long) getpid(), (long) time(NULL));
    mkdir(dir, 0700);
    (void) cap;
}

static void
remove_dir(const char *dir)
{
    char path[1024];
    sprintf(path, "%s/tokens.txt", dir);
    remove(path);
    sprintf(path, "%s/tokens.txt.tmp", dir);
    remove(path);
    rmdir(dir);
}

static void
test_round_trip(const char *dir)
{
    platinum_token_store store;
    platinum_token_store loaded;
    char t1[PLATINUM_TOKEN_BUF];
    char t2[PLATINUM_TOKEN_BUF];
    char hash1[65];
    char path[1024];
    unsigned char *data = NULL;
    size_t len = 0;
    long now = 1000;
    int ix;

    platinum_token_store_init(&store);
    CHECK(platinum_token_store_add(&store, urandom, "did:plc:a", now, "t1", t1) >= 0);
    CHECK(platinum_token_store_add(&store, urandom, "did:plc:b", now, "t2", t2) >= 0);
    CHECK(platinum_token_store_revoke(&store, t2) == 0);
    ix = platinum_token_store_authenticate(&store, t1, 2000);
    CHECK(ix == 0);

    CHECK(platinum_token_store_save(&store, dir) == 0);

    /* The file holds the hash of each token and never the token itself. */
    sprintf(path, "%s/tokens.txt", dir);
    CHECK(platinum_fs_read_all(dir, "tokens.txt", &data, &len, 65536) == 0);
    CHECK(len > 0 && data != NULL);
    platinum_sha256_hex((const unsigned char *) t1, strlen(t1), hash1);
    CHECK(data != NULL && strstr((char *) data, hash1) != NULL);
    CHECK(data != NULL && strstr((char *) data, t1) == NULL);
    CHECK(data != NULL && strstr((char *) data, t2) == NULL);
    free(data);

    platinum_token_store_init(&loaded);
    CHECK(platinum_token_store_load(&loaded, dir) == 0);
    CHECK(slot_equal(&store, &loaded));
    /* A token issued before the restart still signs in; a revoked one still does not. */
    CHECK(platinum_token_store_authenticate(&loaded, t1, 3000) == 0);
    CHECK(platinum_token_store_authenticate(&loaded, t2, 3000) == -1);
    CHECK(loaded.slots[0].last_used == 3000);

    remove(path);
}

static void
test_no_file(const char *dir)
{
    platinum_token_store s;
    platinum_token_store_init(&s);
    s.slots[0].id[0] = 'x';   /* must be cleared by a load that finds no file */
    s.slots[0].id[1] = '\0';
    CHECK(platinum_token_store_load(&s, dir) == 1);
    CHECK(s.slots[0].id[0] == '\0');
}

static void
test_malformed_keeps_store(const char *dir)
{
    platinum_token_store s;
    platinum_token_store before;
    char t[PLATINUM_TOKEN_BUF];
    static const char *bad[] = {
        "platinum-tokens 1\nonly-one-field\n",
        "platinum-tokens 2\n",
        "not-the-header\n",
        "platinum-tokens 1\nid\tdid:plc:x\tshort\t1\t0\t0\n",
        "platinum-tokens 1\nid\tdid:plc:x\t0000000000000000000000000000000000000000000000000000000000000000\t1\t7\t0\n",
        NULL
    };
    int i;

    platinum_token_store_init(&s);
    CHECK(platinum_token_store_add(&s, urandom, "did:plc:keep", 1, "keep", t) >= 0);
    before = s;

    for (i = 0; bad[i] != NULL; i++) {
        CHECK(platinum_fs_write_atomic(dir, "tokens.txt", (const unsigned char *) bad[i], strlen(bad[i])) == 0);
        CHECK(platinum_token_store_load(&s, dir) == -1);
        CHECK(slot_equal(&s, &before));
    }

    /* Two lines with the same id are refused: a revocation would hit the wrong one. */
    {
        static const char dup[] =
            "platinum-tokens 1\n"
            "id\tdid:plc:x\t0000000000000000000000000000000000000000000000000000000000000000\t1\t0\t0\n"
            "id\tdid:plc:y\t1111111111111111111111111111111111111111111111111111111111111111\t1\t0\t0\n";
        CHECK(platinum_fs_write_atomic(dir, "tokens.txt", (const unsigned char *) dup, strlen(dup)) == 0);
        CHECK(platinum_token_store_load(&s, dir) == -1);
        CHECK(slot_equal(&s, &before));
    }

    {
        char path[1024];
        sprintf(path, "%s/tokens.txt", dir);
        remove(path);
    }
}

static void
test_serialize_rejects_tabs(void)
{
    platinum_token_store s;
    char t[PLATINUM_TOKEN_BUF];
    char buf[8192];
    platinum_token_store_init(&s);
    CHECK(platinum_token_store_add(&s, urandom, "did:plc:a\tb", 1, "tab", t) >= 0);
    CHECK(platinum_token_store_serialize(&s, buf, sizeof buf) == -1);
    CHECK(platinum_token_store_add(&s, urandom, "did:plc:ok", 1, "ok", t) >= 0);
    /* A store with one clean and one tabbed slot still refuses to serialize. */
    CHECK(platinum_token_store_serialize(&s, buf, sizeof buf) == -1);
    /* A buffer too small is refused rather than cut. */
    {
        platinum_token_store clean;
        platinum_token_store_init(&clean);
        CHECK(platinum_token_store_add(&clean, urandom, "did:plc:ok", 1, "ok", t) >= 0);
        CHECK(platinum_token_store_serialize(&clean, buf, 10) == -1);
        CHECK(platinum_token_store_serialize(&clean, buf, sizeof buf) > 0);
    }
}

int
main(void)
{
    char dir[512];
    make_dir(dir, sizeof dir);
    test_round_trip(dir);
    test_no_file(dir);
    test_malformed_keeps_store(dir);
    test_serialize_rejects_tabs();
    remove_dir(dir);
    if (failures) {
        fprintf(stderr, "test_token_persist: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_token_persist: %d checks passed\n", checks);
    return 0;
}
