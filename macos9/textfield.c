#include "textfield.h"

#include <Memory.h>
#include <Quickdraw.h>
#include <string.h>

TEHandle platinum_textfield_new(WindowPtr window, const Rect *view)
{
    GrafPtr old_port;
    TEHandle field;

    if (window == NULL || view == NULL)
        return NULL;

    /* TENew takes the current port's font, so make the window's the current one. */
    GetPort(&old_port);
    SetPort((GrafPtr)window);
    field = TENew(view, view);
    SetPort(old_port);
    if (field == NULL)
        return NULL;

    TEAutoView(0, field);
    TESetSelect(0, 0, field);
    return field;
}

long platinum_textfield_length(TEHandle field)
{
    if (field == NULL)
        return 0;
    return (long)(*field)->teLength;
}

long platinum_textfield_copy(TEHandle field, char *buffer, long capacity)
{
    Handle text;
    SignedByte state;
    long length;

    if (field == NULL || buffer == NULL || capacity <= 0)
        return -1;

    /* The handle's own size is the allocation, not the text: the length is the
     * record's teLength. */
    length = platinum_textfield_length(field);
    if (length >= capacity) {
        buffer[0] = '\0';
        return -1;
    }

    text = TEGetText(field);
    if (text == NULL && length > 0) {
        buffer[0] = '\0';
        return -1;
    }
    if (length > 0) {
        state = HGetState(text);
        HLock(text);
        memcpy(buffer, *text, (size_t)length);
        HSetState(text, state);
    }
    buffer[length] = '\0';
    return length;
}

void platinum_textfield_set(TEHandle field, const char *text)
{
    long length;

    if (field == NULL || text == NULL)
        return;
    length = (long)strlen(text);
    TESetText((Ptr)text, length, field);
    TESetSelect(length, length, field);
}

int platinum_textfield_key(TEHandle field, const EventRecord *event,
                           long max_chars, int single_line)
{
    unsigned char key;
    long room;

    if (field == NULL || event == NULL)
        return 0;
    if (event->what != keyDown && event->what != autoKey)
        return 0;
    if ((event->modifiers & cmdKey) != 0)
        return 0;

    key = (unsigned char)(event->message & charCodeMask);
    if (single_line && (key == '\r' || key == 3))
        return 0;

    /* Control characters (delete, arrows) are editing, not text, so they are
     * never limited. Return and Enter in a multi-line field add a character. */
    if (key >= 32 || key == '\r') {
        room = max_chars - platinum_textfield_length(field) +
               ((long)(*field)->selEnd - (long)(*field)->selStart);
        if (room < 1 && key != 127)
            return 0;
    }

    TEKey((char)key, field);
    return 1;
}
