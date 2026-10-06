/*
 * people_feed.c -- loading and paging a list of accounts. No QuickDraw here, so
 * it links on a host for tests; the window is people.c.
 */
#include "people.h"
#include "text_codec.h"

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
                                        ? "actors" : "items", &actors) != WF_OK ||
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
    people_status(people, people->count == 0 ? "Nobody to show." : NULL);
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
    if (kind < PLATINUM_PEOPLE_ACCOUNTS || kind > PLATINUM_PEOPLE_LISTS)
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
