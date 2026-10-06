/*
 * apppw_input.c -- the pure parts of the app-password window: what counts as a
 * plain-http address, and how the typed handle is prepared. No QuickDraw, so it
 * links on a host for tests; the window is apppw.c.
 */
#include "apppw.h"
#include "text_codec.h"

#include <string.h>

int platinum_apppw_is_plain_http(const char *url)
{
    static const char scheme[] = "http://";
    long i;

    if (url == NULL)
        return 0;
    for (i = 0; scheme[i] != '\0'; ++i) {
        char c = url[i];

        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != scheme[i])
            return 0;
    }
    return 1;
}

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
