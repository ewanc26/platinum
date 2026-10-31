#include "atproto_feed.h"

#include <stdio.h>
#include <string.h>

wf_status platinum_atproto_timeline_page(platinum_atproto_client *client,
                                        const char *cursor, size_t limit,
                                        wf_response *out)
{
    const char *names[2];
    const char *values[2];
    char limit_text[8];
    size_t count;

    if (client == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;
    if (limit < 1 || limit > 100)
        return WF_ERR_INVALID_ARG;
    if (cursor != NULL && strlen(cursor) > PLATINUM_ATPROTO_CURSOR_MAX)
        return WF_ERR_INVALID_ARG;

    sprintf(limit_text, "%lu", (unsigned long)limit);

    names[0] = "limit";
    values[0] = limit_text;
    count = 1;

    if (cursor != NULL && cursor[0] != '\0') {
        names[1] = "cursor";
        values[1] = cursor;
        count = 2;
    }

    return platinum_atproto_client_get(client, "app.bsky.feed.getTimeline",
                                       names, values, count, out);
}
