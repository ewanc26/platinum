#ifndef PLATINUM_TEXTFIELD_H
#define PLATINUM_TEXTFIELD_H

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The small amount of TextEdit the client uses, in one place, against the real
 * Toolbox calls: TENew(dest, view), TEKey(char, te), TEClick(pt, extend, te),
 * TESetSelect, TESetText. TextEdit has no length limit parameter (TENew takes
 * only two rectangles), so the limit is enforced here, in the key path.
 */

/* A text field in `window`, drawn in the port's current font. NULL if TextEdit
 * cannot allocate it. */
TEHandle platinum_textfield_new(WindowPtr window, const Rect *view);

/* Bytes of text in the field. */
long platinum_textfield_length(TEHandle field);

/* Copy the text into `buffer` as a C string. Returns its length, or -1 if
 * `capacity` cannot hold it plus the terminator (nothing is truncated). */
long platinum_textfield_copy(TEHandle field, char *buffer, long capacity);

/* Replace the whole text; the insertion point goes to the end. */
void platinum_textfield_set(TEHandle field, const char *text);

/*
 * Handle a keyDown or autoKey event for the field. Returns 1 if the key went to
 * TextEdit and 0 if it was refused: a printable character that would take the
 * text past `max_chars` (the selection being replaced counts as room), or, for
 * a single-line field, Return and Enter. Command-key combinations are never
 * passed on.
 */
int platinum_textfield_key(TEHandle field, const EventRecord *event,
                           long max_chars, int single_line);

#ifdef __cplusplus
}
#endif

#endif
