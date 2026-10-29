/*
 * Atomic whole-file storage for the native bridge (Part 4).
 *
 * Every stored file is replaced atomically: the new content is written to
 * "<dir>/<name>.tmp", flushed, fsynced and closed, then renamed over
 * "<dir>/<name>". A crash at any point leaves either the old file or the new
 * file in place, never a partial one. A leftover "<name>.tmp" from a crash is
 * simply overwritten by the next write of the same name.
 *
 * Names must be a single path component (no '/' or '\\', not "." or "..").
 * Callers are expected to be the only writer of a given directory.
 */
#ifndef PLATINUM_NATIVE_BRIDGE_FSSTORE_H
#define PLATINUM_NATIVE_BRIDGE_FSSTORE_H

#include <stddef.h>

/* Return codes. */
#define PLATINUM_FS_OK              0   /* success (write: durable) */
#define PLATINUM_FS_NOT_FOUND       1   /* read/remove: file does not exist */
#define PLATINUM_FS_NOT_DURABLE     2   /* write only: renamed into place, but this build has no fsync */
#define PLATINUM_FS_ERR_ARG        -1   /* bad argument, including an invalid name */
#define PLATINUM_FS_ERR_IO         -2   /* open, write, flush, fsync, close, rename or remove failed */
#define PLATINUM_FS_ERR_TOO_LARGE  -3   /* read: file is larger than max_len */
#define PLATINUM_FS_ERR_NOMEM      -4   /* allocation failed */

/*
 * Writes dir/name atomically with the given bytes (len may be 0).
 * On POSIX the temporary file is created with mode 0600, and the directory is
 * fsynced after the rename.
 *
 * Returns PLATINUM_FS_OK when the file is in place and durable.
 * Returns PLATINUM_FS_NOT_DURABLE on non-POSIX builds, where the file is in
 * place but fsync is unavailable and durability is not guaranteed.
 * Returns a negative error otherwise. On error the temporary file is removed.
 * If the directory fsync fails after the rename, PLATINUM_FS_ERR_IO is
 * returned and the new content may already be visible.
 */
int platinum_fs_write_atomic(const char *dir, const char *name, const unsigned char *data, size_t len);

/* Creates dir and any missing parents with mode 0700 on POSIX. Returns
 * PLATINUM_FS_OK when the directory exists, or a negative error. */
int platinum_fs_ensure_dir(const char *dir);

/*
 * Reads all of dir/name into a newly allocated buffer and stores it in *out,
 * with its length in *out_len. The caller frees *out with free(). The buffer
 * is never NULL on success, even for an empty file.
 * Fails with PLATINUM_FS_ERR_TOO_LARGE if the file is larger than max_len.
 * A missing file returns PLATINUM_FS_NOT_FOUND and sets *out to NULL.
 */
int platinum_fs_read_all(const char *dir, const char *name, unsigned char **out, size_t *out_len, size_t max_len);

/* Removes dir/name. Returns PLATINUM_FS_OK when removed, PLATINUM_FS_NOT_FOUND when absent. */
int platinum_fs_remove(const char *dir, const char *name);

#endif /* PLATINUM_NATIVE_BRIDGE_FSSTORE_H */
