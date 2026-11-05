/*
 * words_native.c -- the muted-words screen's native path.
 *
 * See words_native.h. Every operation is a read-modify-write of the account's
 * single mutedWordsPref item: the list is loaded, changed locally, and put
 * back whole. putPreferences replaces only the item with the same $type, so
 * the account's other preferences are untouched.
 */

#include "words_native.h"

#include <string.h>

#include "atproto_prefs.h"

static const char *words_heading(void)
{
    return "Muted words (click one, then View > Remove Muted Word)";
}

wf_status platinum_words_show_native(platinum_people *people,
                                     platinum_atproto_client *client)
{
    platinum_muted_list list;
    wf_status status;

    if (people == NULL || client == NULL)
        return WF_ERR_INVALID_ARG;

    people->loading = 1;
    platinum_people_set_status(people, "Loading...");
    status = platinum_prefs_muted_load(client, &list, 0);
    if (status != WF_OK) {
        people->loading = 0;
        platinum_people_set_status(people, "The words could not be loaded.");
        return status;
    }
    return platinum_people_set_words(people, &list, words_heading());
}

wf_status platinum_words_add_native(platinum_atproto_client *client,
                                    const char *word)
{
    platinum_muted_list list;
    wf_status status;

    if (client == NULL || word == NULL || word[0] == '\0')
        return WF_ERR_INVALID_ARG;

    status = platinum_prefs_muted_load(client, &list, 0);
    if (status != WF_OK)
        return status;
    if (!platinum_muted_add(&list, word, 0, 0, 0, NULL))
        return WF_ERR_STATE; /* the list is full */
    return platinum_prefs_muted_save(client, &list);
}

wf_status platinum_words_remove_native(platinum_atproto_client *client,
                                       const char *value)
{
    platinum_muted_list list;
    wf_status status;

    if (client == NULL || value == NULL || value[0] == '\0')
        return WF_ERR_INVALID_ARG;

    status = platinum_prefs_muted_load(client, &list, 0);
    if (status != WF_OK)
        return status;
    if (!platinum_muted_remove(&list, value))
        return WF_ERR_NOT_FOUND;
    return platinum_prefs_muted_save(client, &list);
}
