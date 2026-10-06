#ifndef PLATINUM_DRAFT_H
#define PLATINUM_DRAFT_H

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The compose window's limit. A draft is plain MacRoman text, one byte each. */
#define PLATINUM_DRAFT_MAX 300

/*
 * The unsent text of a new post, kept in the Preferences folder so it survives
 * a close or a crash. Saving empty text clears the draft instead.
 */
OSErr platinum_draft_save(const char *text);
/* Into `buffer` (capacity at least PLATINUM_DRAFT_MAX + 1). fnfErr if there is
 * no draft; a file that is too big is paramErr and is not returned. */
OSErr platinum_draft_load(char *buffer, long capacity);
OSErr platinum_draft_clear(void);

#ifdef __cplusplus
}
#endif

#endif
