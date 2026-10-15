/* POSIX declarations (mkdir, opendir, getpid, rmdir) under strict -std=c89. */
#define _POSIX_C_SOURCE 200112L

#include "fsstore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* _POSIX_VERSION is only defined once <unistd.h> is included, so include it on Unix first. */
#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _POSIX_VERSION

/* Joins two path parts into out; returns 0, or -1 if it would not fit. */
static int path_join(char *out, size_t cap, const char *a, const char *b)
{
    size_t n = strlen(a);
    size_t m = strlen(b);
    if (n + 1 + m >= cap) return -1;
    memcpy(out, a, n);
    out[n] = '/';
    memcpy(out + n + 1, b, m + 1);
    return 0;
}

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: FAIL: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

/* Number of entries in dir, not counting "." and "..". */
static int dir_count(const char *dir)
{
    DIR *d;
    struct dirent *e;
    int n = 0;

    d = opendir(dir);
    if (d == NULL) return -1;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) n++;
    }
    closedir(d);
    return n;
}

/* 1 if dir has an entry with this exact name, 0 otherwise. */
static int dir_has(const char *dir, const char *name)
{
    DIR *d;
    struct dirent *e;
    int found = 0;

    d = opendir(dir);
    if (d == NULL) return 0;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, name) == 0) found = 1;
    }
    closedir(d);
    return found;
}

/* Removes every entry in dir, then dir itself. Best effort. */
static void remove_tree(const char *dir)
{
    DIR *d;
    struct dirent *e;
    char path[1024];

    d = opendir(dir);
    if (d != NULL) {
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            {
                size_t n = strlen(dir);
                size_t m = strlen(e->d_name);
                if (n + 1 + m < sizeof(path)) {
                    memcpy(path, dir, n);
                    path[n] = '/';
                    memcpy(path + n + 1, e->d_name, m + 1);
                    remove(path);
                }
            }
        }
        closedir(d);
    }
    rmdir(dir);
}

static void write_raw(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fwrite(text, 1, strlen(text), f);
        fclose(f);
    }
}

static void test_roundtrip_and_overwrite(const char *dir)
{
    static const unsigned char first[] = { 'h', 'e', 'l', 'l', 'o', 0, 'w', 'o', 'r', 'l', 'd' };
    static const unsigned char second[] = { 's', 'e', 'c', 'o', 'n', 'd', ',', ' ', 'l', 'o', 'n', 'g', 'e', 'r' };
    unsigned char *got = NULL;
    size_t got_len = 0;
    int rc;

    rc = platinum_fs_write_atomic(dir, "value.bin", first, sizeof(first));
    CHECK(rc == PLATINUM_FS_OK);
    rc = platinum_fs_read_all(dir, "value.bin", &got, &got_len, 1024);
    CHECK(rc == PLATINUM_FS_OK);
    CHECK(got_len == sizeof(first));
    CHECK(got != NULL && got_len == sizeof(first) && memcmp(got, first, sizeof(first)) == 0);
    free(got);
    got = NULL;

    /* Overwrite replaces the content and leaves no temporary file behind. */
    rc = platinum_fs_write_atomic(dir, "value.bin", second, sizeof(second));
    CHECK(rc == PLATINUM_FS_OK);
    rc = platinum_fs_read_all(dir, "value.bin", &got, &got_len, 1024);
    CHECK(rc == PLATINUM_FS_OK);
    CHECK(got != NULL && got_len == sizeof(second) && memcmp(got, second, sizeof(second)) == 0);
    free(got);
    got = NULL;

    CHECK(!dir_has(dir, "value.bin.tmp"));
    CHECK(dir_count(dir) == 1);
}

static void test_empty_file(const char *dir)
{
    unsigned char *got = NULL;
    size_t got_len = 99;
    int rc;

    rc = platinum_fs_write_atomic(dir, "empty.bin", NULL, 0);
    CHECK(rc == PLATINUM_FS_OK);
    rc = platinum_fs_read_all(dir, "empty.bin", &got, &got_len, 16);
    CHECK(rc == PLATINUM_FS_OK);
    CHECK(got_len == 0);
    CHECK(got != NULL);
    free(got);
}

static void test_missing_and_limits(const char *dir)
{
    unsigned char *got = NULL;
    size_t got_len = 0;
    int rc;

    rc = platinum_fs_read_all(dir, "missing.bin", &got, &got_len, 1024);
    CHECK(rc == 1);
    CHECK(got == NULL);
    CHECK(got_len == 0);

    /* value.bin holds 14 bytes: one byte under the limit fails, exactly the limit succeeds. */
    rc = platinum_fs_read_all(dir, "value.bin", &got, &got_len, 13);
    CHECK(rc < 0);
    CHECK(got == NULL);

    rc = platinum_fs_read_all(dir, "value.bin", &got, &got_len, 14);
    CHECK(rc == PLATINUM_FS_OK);
    CHECK(got_len == 14);
    free(got);
}

static void test_crash_leftover_tmp(const char *dir)
{
    static const unsigned char fresh[] = { 'n', 'e', 'w', '-', 'd', 'a', 't', 'a' };
    char tmp_path[512];
    unsigned char *got = NULL;
    size_t got_len = 0;
    int rc;

    /* Simulate a crash mid-write: a junk "<name>.tmp" is left in the directory. */
    if (path_join(tmp_path, sizeof(tmp_path), dir, "crash.bin.tmp") != 0) return;
    write_raw(tmp_path, "JUNK-FROM-A-CRASHED-WRITE-THAT-IS-LONGER-THAN-THE-NEW-DATA");
    CHECK(dir_has(dir, "crash.bin.tmp"));

    rc = platinum_fs_write_atomic(dir, "crash.bin", fresh, sizeof(fresh));
    CHECK(rc == PLATINUM_FS_OK);
    rc = platinum_fs_read_all(dir, "crash.bin", &got, &got_len, 1024);
    CHECK(rc == PLATINUM_FS_OK);
    CHECK(got != NULL && got_len == sizeof(fresh) && memcmp(got, fresh, sizeof(fresh)) == 0);
    free(got);

    CHECK(!dir_has(dir, "crash.bin.tmp"));
}

static void test_remove(const char *dir)
{
    int rc;

    rc = platinum_fs_remove(dir, "value.bin");
    CHECK(rc == PLATINUM_FS_OK);
    rc = platinum_fs_remove(dir, "value.bin");
    CHECK(rc == 1);
    rc = platinum_fs_remove(dir, "never-existed.bin");
    CHECK(rc == 1);
}

static void test_bad_arguments(const char *dir)
{
    static const unsigned char data[] = { 'x' };
    char missing_dir[512];
    int rc;

    rc = platinum_fs_write_atomic(dir, "sub/name.bin", data, sizeof(data));
    CHECK(rc < 0);
    rc = platinum_fs_write_atomic(dir, "..", data, sizeof(data));
    CHECK(rc < 0);
    rc = platinum_fs_remove(dir, "");
    CHECK(rc < 0);

    /* A write into a directory that does not exist must fail and leave nothing behind. */
    if (path_join(missing_dir, sizeof(missing_dir), dir, "no-such-subdir") != 0) return;
    rc = platinum_fs_write_atomic(missing_dir, "file.bin", data, sizeof(data));
    CHECK(rc < 0);
    CHECK(!dir_has(dir, "file.bin"));
    CHECK(!dir_has(dir, "file.bin.tmp"));
}

static int make_scratch_dir(char *buf, size_t cap)
{
    const char *base = getenv("TMPDIR");
    long pid = (long)getpid();
    long now = (long)time(NULL);

    if (base == NULL || base[0] == '\0') base = "/tmp";
    if (strlen(base) + 80 >= cap) return -1;
    {
        char name[64];
        sprintf(name, "platinum-fsstore-%ld-%ld", pid, now);
        if (path_join(buf, cap, base, name) != 0) return -1;
    }
    return mkdir(buf, 0700);
}

int main(void)
{
    char dir[1024];

    if (make_scratch_dir(dir, sizeof(dir)) != 0) {
        fprintf(stderr, "could not create scratch directory\n");
        return 1;
    }

    test_roundtrip_and_overwrite(dir);
    test_empty_file(dir);
    test_missing_and_limits(dir);
    test_crash_leftover_tmp(dir);
    test_remove(dir);
    test_bad_arguments(dir);

    remove_tree(dir);

    if (failures != 0) {
        fprintf(stderr, "test_fsstore: %d check(s) failed\n", failures);
        return 1;
    }
    printf("test_fsstore: all checks passed\n");
    return 0;
}

#else /* !_POSIX_VERSION */

int main(void)
{
    printf("test_fsstore: skipped (directory tests need POSIX)\n");
    return 0;
}

#endif
