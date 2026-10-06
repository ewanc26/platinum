#ifndef PLATINUM_PREFS_FILE_H
#define PLATINUM_PREFS_FILE_H

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Small files in the Preferences folder: the settings and the unsent draft.
 * One place does the folder lookup and the open, create, write and read, so
 * the two cannot drift apart. `name` is a Pascal string.
 */

/* Replace the file's contents with `length` bytes, creating it if needed. */
OSErr platinum_prefs_write(const unsigned char *name, OSType creator,
                           OSType file_type, const char *data, long length);

/*
 * Read the whole file into `buffer` and NUL-terminate it; `*length` (optional)
 * is the number of bytes read. A file that does not fit in `capacity` minus one
 * is refused (paramErr), never truncated. A missing file is fnfErr.
 */
OSErr platinum_prefs_read(const unsigned char *name, char *buffer,
                          long capacity, long *length);

/* Delete the file. A file that is not there is not an error. */
OSErr platinum_prefs_delete(const unsigned char *name);

#ifdef __cplusplus
}
#endif

#endif
