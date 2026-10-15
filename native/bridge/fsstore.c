/* POSIX declarations (fsync, fileno, chmod, open dir) under strict -std=c89. */
#define _POSIX_C_SOURCE 200112L

#include "fsstore.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _POSIX_VERSION is only defined once <unistd.h> is included, so include it on Unix first. */
#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* Fixed temporary suffix, same convention as the TypeScript installations save. */
#define PLATINUM_FS_TMP_SUFFIX ".tmp"

static int fs_name_ok(const char *name)
{
    const char *p;

    if (name == NULL || name[0] == '\0') return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (p = name; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') return 0;
    }
    return 1;
}

/* Builds dir + "/" + name + suffix. The caller frees *out. */
static int fs_join(const char *dir, const char *name, const char *suffix, char **out)
{
    size_t dl = strlen(dir);
    size_t nl = strlen(name);
    size_t sl = strlen(suffix);
    int need_sep = !(dl > 0 && dir[dl - 1] == '/');
    char *p;

    p = (char *)malloc(dl + (need_sep ? 1 : 0) + nl + sl + 1);
    if (p == NULL) return PLATINUM_FS_ERR_NOMEM;
    memcpy(p, dir, dl);
    if (need_sep) p[dl] = '/';
    memcpy(p + dl + (need_sep ? 1 : 0), name, nl);
    memcpy(p + dl + (need_sep ? 1 : 0) + nl, suffix, sl + 1);
    *out = p;
    return PLATINUM_FS_OK;
}

#ifdef _POSIX_VERSION
static int fs_fsync_dir(const char *dir)
{
    int fd = open(dir, O_RDONLY);
    int rc = 0;

    if (fd < 0) return -1;
    if (fsync(fd) != 0) rc = -1;
    if (close(fd) != 0) rc = -1;
    return rc;
}
#endif

int platinum_fs_write_atomic(const char *dir, const char *name, const unsigned char *data, size_t len)
{
    char *final_path = NULL;
    char *tmp_path = NULL;
    FILE *f = NULL;
    int rc;
    int status = PLATINUM_FS_OK;

    if (dir == NULL || !fs_name_ok(name) || (data == NULL && len != 0)) return PLATINUM_FS_ERR_ARG;

    rc = fs_join(dir, name, "", &final_path);
    if (rc != PLATINUM_FS_OK) return rc;
    rc = fs_join(dir, name, PLATINUM_FS_TMP_SUFFIX, &tmp_path);
    if (rc != PLATINUM_FS_OK) {
        free(final_path);
        return rc;
    }

#ifdef _POSIX_VERSION
    /* Create the temporary file with mode 0600 from the start, so secrets are
     * never readable under the umask. A leftover from a crash is removed first,
     * because O_EXCL refuses to reuse it. */
    remove(tmp_path);
    {
        int fd = open(tmp_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        f = fd < 0 ? NULL : fdopen(fd, "wb");
        if (f == NULL && fd >= 0) close(fd);
    }
#else
    f = fopen(tmp_path, "wb");
#endif
    if (f == NULL) {
        status = PLATINUM_FS_ERR_IO;
    } else {
        if (status == PLATINUM_FS_OK && len > 0 && fwrite(data, 1, len, f) != len) status = PLATINUM_FS_ERR_IO;
        if (status == PLATINUM_FS_OK && fflush(f) != 0) status = PLATINUM_FS_ERR_IO;
#ifdef _POSIX_VERSION
        if (status == PLATINUM_FS_OK && fsync(fileno(f)) != 0) status = PLATINUM_FS_ERR_IO;
#endif
        if (fclose(f) != 0 && status == PLATINUM_FS_OK) status = PLATINUM_FS_ERR_IO;
        f = NULL;
    }

    if (status == PLATINUM_FS_OK && rename(tmp_path, final_path) != 0) status = PLATINUM_FS_ERR_IO;

    if (status != PLATINUM_FS_OK) {
        /* The rename did not happen, so the temporary file is ours to remove. */
        remove(tmp_path);
    } else {
#ifdef _POSIX_VERSION
        if (fs_fsync_dir(dir) != 0) status = PLATINUM_FS_ERR_IO;
#else
        status = PLATINUM_FS_NOT_DURABLE;
#endif
    }

    free(tmp_path);
    free(final_path);
    return status;
}

int platinum_fs_read_all(const char *dir, const char *name, unsigned char **out, size_t *out_len, size_t max_len)
{
    char *path = NULL;
    FILE *f = NULL;
    unsigned char *buf = NULL;
    long pos;
    size_t size = 0;
    size_t got;
    int rc;

    if (out == NULL || out_len == NULL || dir == NULL || !fs_name_ok(name)) return PLATINUM_FS_ERR_ARG;
    *out = NULL;
    *out_len = 0;

    rc = fs_join(dir, name, "", &path);
    if (rc != PLATINUM_FS_OK) return rc;

    f = fopen(path, "rb");
    if (f == NULL) {
        rc = (errno == ENOENT) ? PLATINUM_FS_NOT_FOUND : PLATINUM_FS_ERR_IO;
        free(path);
        return rc;
    }

    rc = PLATINUM_FS_OK;
    if (fseek(f, 0L, SEEK_END) != 0) {
        rc = PLATINUM_FS_ERR_IO;
    } else {
        pos = ftell(f);
        if (pos < 0) {
            rc = PLATINUM_FS_ERR_IO;
        } else {
            size = (size_t)pos;
            if (size > max_len) {
                rc = PLATINUM_FS_ERR_TOO_LARGE;
            } else if (fseek(f, 0L, SEEK_SET) != 0) {
                rc = PLATINUM_FS_ERR_IO;
            } else {
                /* Allocate at least one byte so that *out is never NULL on success. */
                buf = (unsigned char *)malloc(size > 0 ? size : 1);
                if (buf == NULL) {
                    rc = PLATINUM_FS_ERR_NOMEM;
                } else {
                    got = (size > 0) ? fread(buf, 1, size, f) : 0;
                    /* The file must end exactly where the size said, or it changed underneath us. */
                    if (got != size || ferror(f) || getc(f) != EOF) rc = PLATINUM_FS_ERR_IO;
                }
            }
        }
    }

    if (fclose(f) != 0 && rc == PLATINUM_FS_OK) rc = PLATINUM_FS_ERR_IO;
    free(path);

    if (rc != PLATINUM_FS_OK) {
        free(buf);
        return rc;
    }
    *out = buf;
    *out_len = size;
    return PLATINUM_FS_OK;
}

int platinum_fs_remove(const char *dir, const char *name)
{
    char *path = NULL;
    int rc;

    if (dir == NULL || !fs_name_ok(name)) return PLATINUM_FS_ERR_ARG;

    rc = fs_join(dir, name, "", &path);
    if (rc != PLATINUM_FS_OK) return rc;

    if (remove(path) == 0) {
        rc = PLATINUM_FS_OK;
    } else {
        rc = (errno == ENOENT) ? PLATINUM_FS_NOT_FOUND : PLATINUM_FS_ERR_IO;
    }
    free(path);
    return rc;
}
