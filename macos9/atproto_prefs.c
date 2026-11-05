#include "atproto_prefs.h"

#include <string.h>

#include "atproto_url.h"
#include "json_min.h"

/* Worst case: PLATINUM_MUTED_MAX words, each with a fully escaped value and
 * expiresAt (every byte as \uXXXX), targets, keys and punctuation. */
#define PREFS_BODY_MAX 36864

static char prefs_body[PREFS_BODY_MAX];

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

/* ---- saved custom feeds ------------------------------------------------ */

/* A saved feed is a feed generator record, not the timeline or a list. */
static int prefs_is_feed_uri(const char *uri)
{
    if (uri == NULL || !platinum_atproto_valid_at_uri(uri))
        return 0;
    return strstr(uri, "/app.bsky.feed.generator/") != NULL;
}

static int prefs_has_feed(const platinum_saved_feeds *feeds, const char *uri)
{
    long index;

    for (index = 0; index < (long)feeds->count; index++)
        if (strcmp(feeds->uris[index], uri) == 0)
            return 1;
    return 0;
}

static void prefs_add_feed(platinum_saved_feeds *feeds, const char *uri,
                           int unique)
{
    if (feeds->count >= PLATINUM_SAVED_FEED_MAX)
        return;
    if (unique && prefs_has_feed(feeds, uri))
        return;
    strcpy(feeds->uris[feeds->count], uri);
    feeds->count++;
}

/* savedFeedsPrefV2: items are {type, value, pinned}; only feeds are shown. */
static void prefs_read_feed_items(platinum_saved_feeds *feeds,
                                  platinum_json items)
{
    platinum_json item;
    char type[16];
    char uri[PLATINUM_SAVED_FEED_URI_MAX + 1];
    long count;
    long index;

    if (platinum_json_count(items, &count) != WF_OK)
        return;
    for (index = 0; index < count && feeds->count < PLATINUM_SAVED_FEED_MAX;
         index++) {
        if (platinum_json_element(items, index, &item) != WF_OK)
            continue;
        type[0] = '\0';
        uri[0] = '\0';
        if (platinum_json_string(item, "type", type, sizeof(type)) != WF_OK ||
            strcmp(type, "feed") != 0)
            continue;
        if (platinum_json_string(item, "value", uri, sizeof(uri)) != WF_OK)
            continue;
        if (prefs_is_feed_uri(uri))
            prefs_add_feed(feeds, uri, 0);
    }
}

/* The older savedFeedsPref stores bare AT URI strings in `saved` and `pinned`,
 * and those arrays also name lists, so each entry is checked for the feed
 * generator collection. */
static void prefs_read_feed_array(platinum_saved_feeds *feeds,
                                  platinum_json entry, const char *name,
                                  int unique)
{
    platinum_json array;
    platinum_json item;
    char uri[PLATINUM_SAVED_FEED_URI_MAX + 1];
    long count;
    long index;

    if (platinum_json_member(entry, name, &array) != WF_OK)
        return;
    if (platinum_json_count(array, &count) != WF_OK)
        return;
    for (index = 0; index < count && feeds->count < PLATINUM_SAVED_FEED_MAX;
         index++) {
        if (platinum_json_element(array, index, &item) != WF_OK)
            continue;
        uri[0] = '\0';
        if (platinum_json_value_string(item, uri, sizeof(uri)) != WF_OK)
            continue;
        if (prefs_is_feed_uri(uri))
            prefs_add_feed(feeds, uri, unique);
    }
}

wf_status platinum_prefs_saved_feeds_parse(const char *body,
                                           platinum_saved_feeds *feeds)
{
    platinum_json root;
    platinum_json preferences;
    platinum_json entry;
    char type[128];
    long pref_count;
    long index;
    int found_v2;

    if (feeds == NULL)
        return WF_ERR_INVALID_ARG;
    feeds->count = 0;
    if (body == NULL)
        return WF_ERR_INVALID_ARG;

    if (platinum_json_open(&root, body) != WF_OK)
        return WF_ERR_PARSE;
    if (platinum_json_member(root, "preferences", &preferences) != WF_OK)
        return WF_ERR_PARSE;
    if (platinum_json_count(preferences, &pref_count) != WF_OK)
        return WF_ERR_PARSE;
    if (pref_count > 64)
        pref_count = 64;

    /* V2 wins when it exists, even when it holds no feeds: that is the current
     * preference, and an account that emptied it should not see an old one. */
    found_v2 = 0;
    for (index = 0; index < pref_count; index++) {
        platinum_json items;

        if (platinum_json_element(preferences, index, &entry) != WF_OK)
            continue;
        type[0] = '\0';
        if (platinum_json_string(entry, "$type", type, sizeof(type)) != WF_OK)
            continue;
        if (strcmp(type, "app.bsky.actor.defs#savedFeedsPrefV2") != 0)
            continue;
        if (platinum_json_member(entry, "items", &items) == WF_OK)
            prefs_read_feed_items(feeds, items);
        found_v2 = 1;
        break;
    }
    if (found_v2)
        return WF_OK;

    for (index = 0; index < pref_count; index++) {
        if (platinum_json_element(preferences, index, &entry) != WF_OK)
            continue;
        type[0] = '\0';
        if (platinum_json_string(entry, "$type", type, sizeof(type)) != WF_OK)
            continue;
        if (strcmp(type, "app.bsky.actor.defs#savedFeedsPref") != 0)
            continue;
        prefs_read_feed_array(feeds, entry, "saved", 1);
        prefs_read_feed_array(feeds, entry, "pinned", 1);
        break;
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

static wf_status prefs_append(char *body, size_t capacity, size_t *length,
                              const char *text)
{
    size_t add;

    if (body == NULL || text == NULL)
        return WF_ERR_INVALID_ARG;
    add = strlen(text);
    if (*length + add + 1 > capacity)
        return WF_ERR_ALLOC;
    strcpy(body + *length, text);
    *length += add;
    return WF_OK;
}

static wf_status prefs_append_escaped(char *body, size_t capacity,
                                      size_t *length, const char *text)
{
    char escaped[PLATINUM_MUTED_VALUE_MAX * 6 + 16];

    if (platinum_json_escape(escaped, sizeof(escaped), text) != WF_OK)
        return WF_ERR_ALLOC;
    return prefs_append(body, capacity, length, escaped);
}

wf_status platinum_prefs_muted_write(const platinum_muted_list *list,
                                     char *body, size_t capacity)
{
    size_t length = 0;
    const platinum_muted_entry *word;
    wf_status status;
    int first = 1;
    int i;

    if (list == NULL || body == NULL || capacity == 0)
        return WF_ERR_INVALID_ARG;

    status = prefs_append(body, capacity, &length,
        "{\"preferences\":[{\"$type\":\"app.bsky.actor.defs#mutedWordsPref\","
        "\"items\":[");
    if (status != WF_OK)
        return status;

    for (i = 0; i < list->count && i < PLATINUM_MUTED_MAX; i++) {
        word = &list->words[i];
        if (word->value[0] == '\0')
            continue;
        status = prefs_append(body, capacity, &length, first ? "" : ",");
        if (status != WF_OK)
            return status;
        first = 0;

        status = prefs_append(body, capacity, &length, "{\"value\":\"");
        if (status == WF_OK)
            status = prefs_append_escaped(body, capacity, &length,
                                          word->value);
        if (status == WF_OK)
            status = prefs_append(body, capacity, &length, "\",\"targets\":[");
        if (status != WF_OK)
            return status;

        /* A word with neither target is content-only, the lexicon default. */
        if (word->content || !word->tag) {
            status = prefs_append(body, capacity, &length, "\"content\"");
            if (status != WF_OK)
                return status;
        }
        if (word->tag) {
            status = prefs_append(body, capacity, &length,
                                  word->content ? ",\"tag\"" : "\"tag\"");
            if (status != WF_OK)
                return status;
        }
        status = prefs_append(body, capacity, &length, "]");
        if (status != WF_OK)
            return status;

        if (word->exclude_following) {
            status = prefs_append(body, capacity, &length,
                                  ",\"actorTarget\":\"exclude-following\"");
            if (status != WF_OK)
                return status;
        }
        if (word->expires_at[0] != '\0') {
            status = prefs_append(body, capacity, &length, ",\"expiresAt\":\"");
            if (status == WF_OK)
                status = prefs_append_escaped(body, capacity, &length,
                                              word->expires_at);
            if (status == WF_OK)
                status = prefs_append(body, capacity, &length, "\"");
            if (status != WF_OK)
                return status;
        }
        status = prefs_append(body, capacity, &length, "}");
        if (status != WF_OK)
            return status;
    }

    status = prefs_append(body, capacity, &length, "]}]}");
    if (status != WF_OK)
        return status;
    return WF_OK;
}

wf_status platinum_prefs_muted_save(platinum_atproto_client *client,
                                    const platinum_muted_list *list)
{
    wf_response response;
    wf_status status;

    if (client == NULL || list == NULL)
        return WF_ERR_INVALID_ARG;

    status = platinum_prefs_muted_write(list, prefs_body, sizeof(prefs_body));
    if (status != WF_OK)
        return status;

    memset(&response, 0, sizeof(response));
    status = platinum_atproto_client_post(
        client, "app.bsky.actor.putPreferences", prefs_body, &response);
    wf_response_free(&response);
    return status;
}
