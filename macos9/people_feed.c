/*
 * people_feed.c -- loading and paging a list of accounts. No QuickDraw here, so
 * it links on a host for tests; the window is people.c.
 */
#include "people.h"
#include "text_codec.h"

#include "atproto_feed.h"
#include "atproto_url.h"
#include "json_min.h"
#include <string.h>

static void people_status(platinum_people *people, const char *status)
{
    long length;

    people->status[0] = '\0';
    if (status == NULL)
        return;
    length = (long)strlen(status);
    if (length > PLATINUM_PEOPLE_STATUS_MAX)
        length = PLATINUM_PEOPLE_STATUS_MAX;
    memcpy(people->status, status, (size_t)length);
    people->status[length] = '\0';
}

void platinum_people_set_status(platinum_people *people, const char *status)
{
    if (people != NULL)
        people_status(people, status);
}

void platinum_people_init(platinum_people *people)
{
    if (people == NULL)
        return;
    memset(people, 0, sizeof(*people));
    people->selected = -1;
    people_status(people, "No list loaded.");
}

/* Display copy of a string member as MacRoman; empty if absent or not a string. */
static void people_copy(char *destination, long capacity, platinum_json object,
                        const char *name)
{
    char utf8[PLATINUM_TEXT_UTF8_CAPACITY];

    destination[0] = '\0';
    if (platinum_json_string_truncating(object, name, utf8, sizeof(utf8))
        != WF_OK)
        return;
    if (platinum_text_utf8_to_macroman(utf8, destination, capacity, NULL) < 0)
        destination[0] = '\0';
}

/* An account needs a did; it is shown by display name, then handle, then did.
 * A feed or list row needs a name and a uri, which is copied exactly. */
static int people_parse(platinum_person *person, platinum_json item, int kind)
{
    char did[PLATINUM_PEOPLE_NAME_MAX];

    memset(person, 0, sizeof(*person));
    if (kind == PLATINUM_PEOPLE_WORDS) {
        /* A muted word is shown as MacRoman and kept exactly as UTF-8. A word
         * that cannot be shown cannot be picked, so it is left out. */
        char value[PLATINUM_TEXT_UTF8_CAPACITY];
        long lossy;

        person->name[0] = '\0';
        if (platinum_json_string_truncating(item, "value", value,
                                            sizeof(value)) != WF_OK)
            return 0;
        if (platinum_text_utf8_to_macroman(value, person->name,
                                           sizeof(person->name), &lossy) < 0 ||
            lossy != 0 || person->name[0] == '\0')
            return 0;
        if (platinum_json_string(item, "value", person->uri,
                                 sizeof(person->uri)) != WF_OK) {
            person->uri[0] = '\0';
            return 0;
        }
        return 1;
    }
    if (kind != PLATINUM_PEOPLE_ACCOUNTS) {
        people_copy(person->name, sizeof(person->name), item, "name");
        if (person->name[0] == '\0' ||
            platinum_json_string(item, "uri", person->uri, sizeof(person->uri))
                != WF_OK) {
            person->uri[0] = '\0';
            return 0;
        }
        return 1;
    }
    people_copy(did, sizeof(did), item, "did");
    if (did[0] == '\0')
        return 0;
    /* The DID, copied exactly, is how the row is opened. */
    if (platinum_json_string(item, "did", person->uri, sizeof(person->uri))
        != WF_OK) {
        person->uri[0] = '\0';
        return 0;
    }
    people_copy(person->name, sizeof(person->name), item, "displayName");
    people_copy(person->handle + 1, sizeof(person->handle) - 1, item, "handle");
    if (person->handle[1] != '\0')
        person->handle[0] = '@';
    else
        person->handle[0] = '\0';
    if (person->name[0] == '\0')
        strcpy(person->name, person->handle[0] != '\0' ? person->handle + 1 : did);
    return 1;
}

static wf_status people_fetch(platinum_people *people,
                              platinum_bridge_client *bridge,
                              int append,
                              unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json actors;
    platinum_json item;
    char request[PLATINUM_PEOPLE_PATH_MAX + 3 * 256];
    char cursor[sizeof(people->cursor)];
    long available;
    long index;
    long drop;
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;

    strcpy(request, people->path);
    if (append) {
        strcat(request, "&cursor=");
        if (platinum_bridge_query_escape(people->cursor,
                                         request + strlen(request),
                                         (long)(sizeof(request) - strlen(request)))
            < 0)
            return WF_ERR_INVALID_ARG;
    } else {
        people->count = 0;
        people->scroll_row = 0;
        people->selected = -1;
        people->cursor[0] = '\0';
    }

    memset(&response, 0, sizeof(response));
    people->loading = 1;
    people_status(people, "Loading...");
    status = platinum_bridge_get(bridge, request, &response);
    if (status != WF_OK) {
        people->loading = 0;
        if (status == WF_ERR_AUTH)
            people_status(people, "Session expired. Pair the account again.");
        else if (response.status == 404)
            people_status(people, "That account or post no longer exists.");
        else
            people_status(people, "The list could not be loaded.");
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, people->kind == PLATINUM_PEOPLE_ACCOUNTS
                                        ? "actors"
                                        : people->kind == PLATINUM_PEOPLE_WORDS
                                              ? "words" : "items", &actors) != WF_OK ||
        platinum_json_count(actors, &available) != WF_OK) {
        people->loading = 0;
        people_status(people, "The bridge returned an invalid list.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }
    if (available > PLATINUM_PEOPLE_PAGE)
        available = PLATINUM_PEOPLE_PAGE;

    /* Known good, so only now drop the first rows to stay within the cap. */
    drop = (long)people->count + available - PLATINUM_PEOPLE_MAX;
    if (drop > (long)people->count)
        drop = (long)people->count;
    if (drop > 0) {
        memmove(&people->items[0], &people->items[drop],
                (size_t)((long)people->count - drop) * sizeof(people->items[0]));
        people->count = (unsigned short)((long)people->count - drop);
        if (dropped != NULL)
            *dropped = (unsigned short)drop;
    }

    for (index = 0; index < available && people->count < PLATINUM_PEOPLE_MAX;
         ++index) {
        if (platinum_json_element(actors, index, &item) != WF_OK)
            continue;
        if (people_parse(&people->items[people->count], item, people->kind))
            ++people->count;
    }

    /* A cursor is copied exactly or not at all. */
    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) == WF_OK)
        strcpy(people->cursor, cursor);
    else
        people->cursor[0] = '\0';
    wf_response_free(&response);

    people->loading = 0;
    people_status(people, people->count == 0
                              ? (people->kind == PLATINUM_PEOPLE_WORDS
                                     ? "No muted words." : "Nobody to show.")
                              : NULL);
    return WF_OK;
}

wf_status platinum_people_load_kind(platinum_people *people,
                                    platinum_bridge_client *bridge,
                                    int kind,
                                    const char *route,
                                    const char *key,
                                    const char *value,
                                    const char *heading)
{
    long length;

    if (people == NULL || bridge == NULL || route == NULL)
        return WF_ERR_INVALID_ARG;
    if (key != NULL && (value == NULL || value[0] == '\0'))
        return WF_ERR_INVALID_ARG;
    if (kind < PLATINUM_PEOPLE_ACCOUNTS || kind > PLATINUM_PEOPLE_WORDS)
        return WF_ERR_INVALID_ARG;

    strcpy(people->path, route);
    if (key != NULL) {
        strcat(people->path, "?");
        strcat(people->path, key);
        strcat(people->path, "=");
        length = (long)strlen(people->path);
        if (platinum_bridge_query_escape(value, people->path + length,
                                         (long)sizeof(people->path) - length) < 0) {
            people->path[0] = '\0';
            return WF_ERR_INVALID_ARG;
        }
    }

    people->kind = kind;
    people->native_list = 0;
    people->native_follows = 0;
    people->native_engagement = 0;
    people->heading[0] = '\0';
    if (heading != NULL) {
        strncpy(people->heading, heading, sizeof(people->heading) - 1);
        people->heading[sizeof(people->heading) - 1] = '\0';
    }
    return people_fetch(people, bridge, 0, NULL);
}

wf_status platinum_people_load(platinum_people *people,
                               platinum_bridge_client *bridge,
                               const char *route,
                               const char *key,
                               const char *value,
                               const char *heading)
{
    if (key == NULL)
        return WF_ERR_INVALID_ARG;
    return platinum_people_load_kind(people, bridge, PLATINUM_PEOPLE_ACCOUNTS,
                                     route, key, value, heading);
}

wf_status platinum_people_set_words(platinum_people *people,
                                    const platinum_muted_list *words,
                                    const char *heading)
{
    char name[PLATINUM_PEOPLE_NAME_MAX];
    const platinum_muted_entry *word;
    long count;
    long index;
    long lossy;

    if (people == NULL || words == NULL)
        return WF_ERR_INVALID_ARG;

    people->kind = PLATINUM_PEOPLE_WORDS;
    people->native_list = 0;
    people->native_follows = 0;
    people->native_engagement = 0;
    people->heading[0] = '\0';
    if (heading != NULL) {
        strncpy(people->heading, heading, sizeof(people->heading) - 1);
        people->heading[sizeof(people->heading) - 1] = '\0';
    }
    people->count = 0;
    people->scroll_row = 0;
    people->selected = -1;
    people->cursor[0] = '\0';
    people->path[0] = '\0';

    count = words->count;
    if (count > PLATINUM_PEOPLE_MAX)
        count = PLATINUM_PEOPLE_MAX;
    for (index = 0; index < count; index++) {
        word = &words->words[index];
        /* A word that cannot be shown cannot be picked, so it is left out. */
        if (platinum_text_utf8_to_macroman(word->value, name, sizeof(name),
                                           &lossy) < 0 ||
            lossy != 0 || name[0] == '\0')
            continue;
        memset(&people->items[people->count], 0,
               sizeof(people->items[people->count]));
        strcpy(people->items[people->count].name, name);
        strcpy(people->items[people->count].uri, word->value);
        people->count++;
    }

    people->loading = 0;
    people_status(people, people->count == 0 ? "No muted words." : NULL);
    return WF_OK;
}

void platinum_people_take_named(platinum_people *people, platinum_json items,
                                const char *name)
{
    platinum_json item;
    platinum_person person;
    long available;
    long index;

    if (people == NULL || name == NULL ||
        platinum_json_count(items, &available) != WF_OK)
        return;

    for (index = 0; index < available && people->count < PLATINUM_PEOPLE_MAX;
         ++index) {
        if (platinum_json_element(items, index, &item) != WF_OK)
            continue;
        memset(&person, 0, sizeof(person));
        people_copy(person.name, sizeof(person.name), item, name);
        if (person.name[0] == '\0' ||
            platinum_json_string(item, "uri", person.uri,
                                 sizeof(person.uri)) != WF_OK)
            continue;
        people->items[people->count] = person;
        ++people->count;
    }
}

static void people_take_profiles(platinum_people *people, platinum_json items,
                                 const char *member, const char *cursor,
                                 unsigned short *dropped)
{
    /* Static, not on the stack: a page of rows is about 13KB and a Classic Mac
     * stack is small. Not re-entrant, which nothing here needs. */
    static platinum_person parsed[PLATINUM_PEOPLE_PAGE];
    platinum_json item;
    platinum_json subject;
    long available;
    long index;
    long got;
    long drop;

    if (dropped != NULL)
        *dropped = 0;
    if (people == NULL || platinum_json_count(items, &available) != WF_OK)
        return;
    if (available > PLATINUM_PEOPLE_PAGE)
        available = PLATINUM_PEOPLE_PAGE;

    got = 0;
    for (index = 0; index < available && got < PLATINUM_PEOPLE_PAGE;
         ++index) {
        if (platinum_json_element(items, index, &item) != WF_OK)
            continue;
        if (member != NULL) {
            if (platinum_json_member(item, member, &subject) != WF_OK)
                continue;
        } else {
            subject = item;
        }
        if (people_parse(&parsed[got], subject, PLATINUM_PEOPLE_ACCOUNTS))
            ++got;
    }

    /* Known good, so only now drop the first rows to stay within the cap. */
    drop = (long)people->count + got - PLATINUM_PEOPLE_MAX;
    if (drop > (long)people->count)
        drop = (long)people->count;
    if (drop > 0) {
        memmove(&people->items[0], &people->items[drop],
                (size_t)((long)people->count - drop) * sizeof(people->items[0]));
        people->count = (unsigned short)((long)people->count - drop);
        if (dropped != NULL)
            *dropped = (unsigned short)drop;
    }
    for (index = 0; index < got; ++index) {
        people->items[people->count] = parsed[index];
        ++people->count;
    }

    /* A cursor is copied exactly or not at all. */
    people->cursor[0] = '\0';
    if (cursor != NULL && cursor[0] != '\0') {
        strncpy(people->cursor, cursor, sizeof(people->cursor) - 1);
        people->cursor[sizeof(people->cursor) - 1] = '\0';
    }
}

void platinum_people_take_subjects(platinum_people *people, platinum_json items,
                                   const char *cursor,
                                   unsigned short *dropped)
{
    people_take_profiles(people, items, "subject", cursor, dropped);
}

const platinum_person *platinum_people_selection(const platinum_people *people)
{
    if (people == NULL || people->selected < 0 ||
        people->selected >= (short)people->count)
        return NULL;
    return &people->items[people->selected];
}

int platinum_people_has_more(const platinum_people *people)
{
    return people != NULL && people->cursor[0] != '\0' && people->count > 0 &&
           people->path[0] != '\0';
}

wf_status platinum_people_load_more(platinum_people *people,
                                    platinum_bridge_client *bridge,
                                    unsigned short *dropped)
{
    if (dropped != NULL)
        *dropped = 0;
    if (people == NULL || bridge == NULL || !platinum_people_has_more(people))
        return WF_ERR_INVALID_ARG;
    return people_fetch(people, bridge, 1, dropped);
}

static wf_status people_fetch_native(platinum_people *people,
                                     platinum_atproto_client *client,
                                     int append, unsigned short *dropped)
{
    wf_response response;
    platinum_json root;
    platinum_json actors;
    const char *member;
    const char *profile_member;
    const char *request_cursor;
    char cursor[sizeof(people->cursor)];
    long available;
    wf_status status;

    if (dropped != NULL)
        *dropped = 0;
    if (!append) {
        people->count = 0;
        people->scroll_row = 0;
        people->selected = -1;
        people->cursor[0] = '\0';
    }

    request_cursor = append ? people->cursor : NULL;
    memset(&response, 0, sizeof(response));
    people->loading = 1;
    people_status(people, "Loading...");
    if (people->native_engagement == 1)
        status = platinum_atproto_likes_page(client, people->path,
                                             request_cursor,
                                             PLATINUM_PEOPLE_PAGE, &response);
    else if (people->native_engagement == 2)
        status = platinum_atproto_reposts_page(client, people->path,
                                               request_cursor,
                                               PLATINUM_PEOPLE_PAGE,
                                               &response);
    else if (people->native_follows == 2)
        status = platinum_atproto_followers_page(client, people->path,
                                                 request_cursor,
                                                 PLATINUM_PEOPLE_PAGE,
                                                 &response);
    else
        status = platinum_atproto_follows_page(client, people->path,
                                               request_cursor,
                                               PLATINUM_PEOPLE_PAGE,
                                               &response);
    if (status != WF_OK) {
        people->loading = 0;
        people_status(people, status == WF_ERR_AUTH
                                  ? "Session expired. Sign in again."
                                  : "The list could not be loaded.");
        wf_response_free(&response);
        return status;
    }

    if (people->native_engagement == 1) {
        member = "likes";
        profile_member = "actor";
    } else if (people->native_engagement == 2) {
        member = "repostedBy";
        profile_member = NULL;
    } else {
        member = people->native_follows == 2 ? "followers" : "follows";
        profile_member = NULL;
    }
    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_member(root, member, &actors) != WF_OK ||
        platinum_json_count(actors, &available) != WF_OK) {
        people->loading = 0;
        people_status(people, "The server returned an invalid list.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }
    (void)available;

    if (platinum_json_string(root, "cursor", cursor, sizeof(cursor)) != WF_OK)
        cursor[0] = '\0';
    people_take_profiles(people, actors, profile_member, cursor, dropped);
    wf_response_free(&response);

    people->loading = 0;
    people_status(people, people->count == 0 ? "Nobody to show." : NULL);
    return WF_OK;
}

static void people_native_begin(platinum_people *people, const char *path,
                                const char *heading)
{
    people->kind = PLATINUM_PEOPLE_ACCOUNTS;
    people->native_list = 0;
    people->native_follows = 0;
    people->native_engagement = 0;
    people->count = 0;
    people->scroll_row = 0;
    people->selected = -1;
    strncpy(people->path, path, sizeof(people->path) - 1);
    people->path[sizeof(people->path) - 1] = '\0';
    people->cursor[0] = '\0';
    people->heading[0] = '\0';
    if (heading != NULL) {
        strncpy(people->heading, heading, sizeof(people->heading) - 1);
        people->heading[sizeof(people->heading) - 1] = '\0';
    }
}

static wf_status people_load_graph_native(platinum_people *people,
                                          platinum_atproto_client *client,
                                          const char *actor, int followers,
                                          const char *heading)
{
    if (people == NULL || client == NULL || actor == NULL || actor[0] == '\0' ||
        (followers != 0 && followers != 1))
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_did(actor) &&
        !platinum_atproto_valid_handle(actor))
        return WF_ERR_INVALID_ARG;
    if (strlen(actor) >= sizeof(people->path))
        return WF_ERR_INVALID_ARG;

    people_native_begin(people, actor, heading);
    people->native_follows = followers ? 2 : 1;
    return people_fetch_native(people, client, 0, NULL);
}

wf_status platinum_people_follows_native(platinum_people *people,
                                         platinum_atproto_client *client,
                                         const char *actor,
                                         const char *heading)
{
    return people_load_graph_native(people, client, actor, 0, heading);
}

wf_status platinum_people_followers_native(platinum_people *people,
                                           platinum_atproto_client *client,
                                           const char *actor,
                                           const char *heading)
{
    return people_load_graph_native(people, client, actor, 1, heading);
}

static wf_status people_load_post_native(platinum_people *people,
                                         platinum_atproto_client *client,
                                         const char *post_uri, int engagement,
                                         const char *heading)
{
    if (people == NULL || client == NULL || post_uri == NULL ||
        post_uri[0] == '\0' || (engagement != 1 && engagement != 2))
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_at_uri(post_uri))
        return WF_ERR_INVALID_ARG;
    if (strlen(post_uri) >= sizeof(people->path))
        return WF_ERR_INVALID_ARG;

    people_native_begin(people, post_uri, heading);
    people->native_engagement = engagement;
    return people_fetch_native(people, client, 0, NULL);
}

wf_status platinum_people_likes_native(platinum_people *people,
                                       platinum_atproto_client *client,
                                       const char *post_uri,
                                       const char *heading)
{
    return people_load_post_native(people, client, post_uri, 1, heading);
}

wf_status platinum_people_reposts_native(platinum_people *people,
                                         platinum_atproto_client *client,
                                         const char *post_uri,
                                         const char *heading)
{
    return people_load_post_native(people, client, post_uri, 2, heading);
}

wf_status platinum_people_more_native(platinum_people *people,
                                      platinum_atproto_client *client,
                                      unsigned short *dropped)
{
    if (dropped != NULL)
        *dropped = 0;
    if (people == NULL || client == NULL ||
        (people->native_follows == 0 && people->native_engagement == 0) ||
        !platinum_people_has_more(people))
        return WF_ERR_INVALID_ARG;
    return people_fetch_native(people, client, 1, dropped);
}
