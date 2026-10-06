/*
 * search_query.c -- preparing a typed query. No Toolbox, so it links on a host
 * for tests; the window is search.c.
 */
#include "search.h"
#include "text_codec.h"

#include <string.h>

long platinum_search_prepare(const char *macroman, char *utf8, long capacity)
{
    char trimmed[PLATINUM_SEARCH_MAX + 1];
    long start;
    long end;
    long length;
    long converted;

    if (macroman == NULL || utf8 == NULL || capacity <= 0)
        return -1;

    length = (long)strlen(macroman);
    start = 0;
    while (start < length && macroman[start] == ' ')
        ++start;
    end = length;
    while (end > start && macroman[end - 1] == ' ')
        --end;
    if (end == start || end - start > PLATINUM_SEARCH_MAX)
        return -1;

    memcpy(trimmed, macroman + start, (size_t)(end - start));
    trimmed[end - start] = '\0';

    converted = platinum_text_macroman_to_utf8(trimmed, utf8, capacity, NULL);
    return converted > 0 ? converted : -1;
}
