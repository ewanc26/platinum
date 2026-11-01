/*
 * apppw_input.c -- the pure part of the app-password window: how the typed
 * handle is prepared, and the focus-stop and Return rules. No QuickDraw, so it
 * links on a host for tests; the window is apppw.c.
 */
#include "apppw.h"
#include "text_codec.h"

#include <string.h>

long platinum_apppw_prepare_handle(const char *macroman, char *utf8,
                                   long capacity)
{
    char trimmed[PLATINUM_APPPW_HANDLE_MAX + 2];
    const char *start;
    long length;
    long i;

    if (macroman == NULL || utf8 == NULL || capacity <= 0)
        return -1;
    start = macroman;
    while (*start == ' ')
        ++start;
    if (*start == '@')
        ++start;
    length = (long)strlen(start);
    while (length > 0 && start[length - 1] == ' ')
        --length;
    if (length == 0 || length > PLATINUM_APPPW_HANDLE_MAX)
        return -1;
    for (i = 0; i < length; ++i)
        if ((unsigned char)start[i] < 32 || start[i] == 127)
            return -1;
    memcpy(trimmed, start, (size_t)length);
    trimmed[length] = '\0';
    return platinum_text_macroman_to_utf8(trimmed, utf8, capacity, NULL);
}

short platinum_apppw_next_focus(short focus, int backward, int signed_in)
{
    if (signed_in)
        return PLATINUM_APPPW_FOCUS_CANCEL;
    if (focus < PLATINUM_APPPW_FOCUS_HANDLE ||
        focus >= PLATINUM_APPPW_FOCUS_COUNT)
        focus = PLATINUM_APPPW_FOCUS_HANDLE;
    if (backward)
        return (short)((focus + PLATINUM_APPPW_FOCUS_COUNT - 1) %
                       PLATINUM_APPPW_FOCUS_COUNT);
    return (short)((focus + 1) % PLATINUM_APPPW_FOCUS_COUNT);
}

int platinum_apppw_return_action(short focus, int signed_in)
{
    if (signed_in)
        return PLATINUM_APPPW_CANCEL;
    if (focus == PLATINUM_APPPW_FOCUS_CANCEL)
        return PLATINUM_APPPW_CANCEL;
    return PLATINUM_APPPW_SIGN_IN;
}
