#include "draft.h"
#include "prefs_file.h"

#include <stddef.h>
#include <string.h>

static const unsigned char kDraftFileName[] = {
    14, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', 'D', 'r', 'a', 'f', 't'
};

OSErr platinum_draft_clear(void)
{
    return platinum_prefs_delete(kDraftFileName);
}

OSErr platinum_draft_save(const char *text)
{
    long length;

    if (text == NULL || text[0] == '\0')
        return platinum_draft_clear();
    length = (long)strlen(text);
    if (length > PLATINUM_DRAFT_MAX)
        return paramErr;
    return platinum_prefs_write(kDraftFileName, 'PTLM', 'TEXT', text, length);
}

OSErr platinum_draft_load(char *buffer, long capacity)
{
    if (buffer == NULL || capacity < PLATINUM_DRAFT_MAX + 1)
        return paramErr;
    return platinum_prefs_read(kDraftFileName, buffer, PLATINUM_DRAFT_MAX + 1,
                               NULL);
}
