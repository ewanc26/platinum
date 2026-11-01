#include "atproto_prefs.h"

#include <string.h>

#include "json_min.h"

static int prefs_is_muted_words(const char *type)
{
    return type != NULL &&
        strcmp(type, "app.bsky.actor.defs#mutedWordsPref") == 0;
}

static void prefs_read_word(platinum_muted_list *list, platinum_json item)
{
    char value[PLATINUM_MUTED_VALUE_MAX + 1];
    char target_text[32];
    char actor_text[32];
    char expires[PLATINUM_MUTED_EXPIRES_MAX];
    platinum_json targets;
    platinum_json target_item;
    long target_count;
    long index;
    int content;
    int tag;
    int exclude;

    value[0] = '\0';
    if (platinum_json_string_truncating(item, "value", value,
                                        sizeof(value)) != WF_OK)
        return;
    if (value[0] == '\0')
        return;

    content = 0;
    tag = 0;
    if (platinum_json_member(item, "targets", &targets) == WF_OK &&
        platinum_json_count(targets, &target_count) == WF_OK) {
        if (target_count > 16)
            target_count = 16;
        for (index = 0; index < target_count; index++) {
            if (platinum_json_element(targets, index, &target_item) != WF_OK)
                continue;
            target_text[0] = '\0';
            if (platinum_json_value_string(target_item, target_text,
                                           sizeof(target_text)) != WF_OK)
                continue;
            if (strcmp(target_text, "content") == 0)
                content = 1;
            else if (strcmp(target_text, "tag") == 0)
                tag = 1;
        }
    }

    exclude = 0;
    actor_text[0] = '\0';
    if (platinum_json_string(item, "actorTarget", actor_text,
                             sizeof(actor_text)) == WF_OK &&
        strcmp(actor_text, "exclude-following") == 0)
        exclude = 1;

    expires[0] = '\0';
    if (platinum_json_string(item, "expiresAt", expires,
                             sizeof(expires)) != WF_OK)
        expires[0] = '\0';

    /* A word with neither target is content-only, per the lexicon default. */
    (void)platinum_muted_add(list, value, content, tag, exclude, expires);
}

wf_status platinum_prefs_muted_parse(const char *body,
                                     platinum_muted_list *list,
                                     long now)
{
    platinum_json root;
    platinum_json preferences;
    platinum_json entry;
    platinum_json items;
    platinum_json word;
    char entry_type[128];
    long pref_count;
    long item_count;
    long i;
    long j;
    wf_status status;

    if (list == NULL)
        return WF_ERR_INVALID_ARG;
    platinum_muted_clear(list);
    list->now = now;
    if (body == NULL)
        return WF_ERR_INVALID_ARG;

    status = platinum_json_open(&root, body);
    if (status != WF_OK)
        return WF_ERR_PARSE;
    if (platinum_json_member(root, "preferences", &preferences) != WF_OK)
        return WF_ERR_PARSE;
    if (platinum_json_count(preferences, &pref_count) != WF_OK)
        return WF_ERR_PARSE;
    if (pref_count > 64)
        pref_count = 64;

    for (i = 0; i < pref_count; i++) {
        if (platinum_json_element(preferences, i, &entry) != WF_OK)
            continue;
        entry_type[0] = '\0';
        if (platinum_json_string(entry, "$type", entry_type,
                                 sizeof(entry_type)) != WF_OK)
            continue;
        if (!prefs_is_muted_words(entry_type))
            continue;
        if (platinum_json_member(entry, "items", &items) != WF_OK)
            continue;
        if (platinum_json_count(items, &item_count) != WF_OK)
            continue;
        for (j = 0; j < item_count; j++) {
            if (platinum_json_element(items, j, &word) != WF_OK)
                continue;
            prefs_read_word(list, word);
            if (list->count >= PLATINUM_MUTED_MAX)
                return WF_OK;
        }
    }
    return WF_OK;
}

wf_status platinum_prefs_muted_load(platinum_atproto_client *client,
                                    platinum_muted_list *list,
                                    long now)
{
    wf_response response;
    wf_status status;

    if (list == NULL)
        return WF_ERR_INVALID_ARG;
    platinum_muted_clear(list);
    list->now = now;
    if (client == NULL)
        return WF_ERR_INVALID_ARG;

    memset(&response, 0, sizeof(response));
    status = platinum_atproto_client_get(
        client, "app.bsky.actor.getPreferences", NULL, NULL, 0, &response);
    if (status != WF_OK) {
        wf_response_free(&response);
        return status;
    }
    status = platinum_prefs_muted_parse(
        response.body != NULL ? response.body : "", list, now);
    wf_response_free(&response);
    return status;
}
