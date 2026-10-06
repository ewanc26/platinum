#ifndef PLATINUM_PEOPLE_H
#define PLATINUM_PEOPLE_H

#include "bridge_client.h"
#include "scrollbar.h"

#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Rows kept, and rows per request; past the cap the first rows are dropped. */
#define PLATINUM_PEOPLE_MAX 40
#define PLATINUM_PEOPLE_PAGE 20
#define PLATINUM_PEOPLE_NAME_MAX 64
#define PLATINUM_PEOPLE_HEADING_MAX 120
#define PLATINUM_PEOPLE_STATUS_MAX 127
#define PLATINUM_PEOPLE_PATH_MAX 640

typedef struct platinum_person {
    char name[PLATINUM_PEOPLE_NAME_MAX];
    char handle[PLATINUM_PEOPLE_NAME_MAX];
    /* The account's DID, or a feed or list row's AT URI, copied exactly. */
    char uri[513];
} platinum_person;

/*
 * One list of accounts: followers, following, who liked a post, who reposted
 * it. They share a shape and a window, so there is one model.
 */
typedef struct platinum_people {
    WindowPtr window;
    platinum_scrollbar scrollbar;
    platinum_person items[PLATINUM_PEOPLE_MAX];
    unsigned short count;
    short scroll_row;
    char path[PLATINUM_PEOPLE_PATH_MAX]; /* the request without the cursor */
    char cursor[256];
    int kind;      /* PLATINUM_PEOPLE_ACCOUNTS, _FEEDS or _LISTS */
    short selected; /* row index, or -1 */
    char heading[PLATINUM_PEOPLE_HEADING_MAX];
    char status[PLATINUM_PEOPLE_STATUS_MAX + 1];
    int loading;
} platinum_people;

enum {
    PLATINUM_PEOPLE_ACCOUNTS = 0,
    PLATINUM_PEOPLE_FEEDS = 1,
    PLATINUM_PEOPLE_LISTS = 2
};

enum {
    PLATINUM_PEOPLE_NONE = 0,
    PLATINUM_PEOPLE_CLOSE = 1,
    PLATINUM_PEOPLE_LOAD_MORE = 2,
    PLATINUM_PEOPLE_OPEN = 3 /* a row was clicked: see platinum_people_selection */
};

void platinum_people_init(platinum_people *people);

/*
 * Load the first page of `route` (for example "/v1/follows") with one query
 * parameter `key=value` (value percent-encoded here), replacing the list.
 * `heading` is shown above it. On failure the list is empty and the status says
 * why.
 */
wf_status platinum_people_load(platinum_people *people,
                               platinum_bridge_client *bridge,
                               const char *route,
                               const char *key,
                               const char *value,
                               const char *heading);

/*
 * Like platinum_people_load, for a list of accounts, saved feeds or the
 * account's own lists. `kind` says what the rows are; with no `key` the route is
 * requested without a parameter (feeds and lists take none). Feed and list rows
 * come from the bridge's {"items":[{"uri","name"}]} shape and carry their URI.
 */
wf_status platinum_people_load_kind(platinum_people *people,
                                    platinum_bridge_client *bridge,
                                    int kind,
                                    const char *route,
                                    const char *key,
                                    const char *value,
                                    const char *heading);

/* The clicked row after PLATINUM_PEOPLE_OPEN, or NULL. */
const platinum_person *platinum_people_selection(const platinum_people *people);

/* Append the next page; on success `dropped` rows left the front. A failed page
 * keeps what was loaded. WF_ERR_INVALID_ARG when there is no more. */
wf_status platinum_people_load_more(platinum_people *people,
                                    platinum_bridge_client *bridge,
                                    unsigned short *dropped);
int platinum_people_has_more(const platinum_people *people);

/* The window; none of this runs on a host. */
OSErr platinum_people_open(platinum_people *people);
void platinum_people_close(platinum_people *people);
int platinum_people_handle_event(platinum_people *people, EventRecord *event);
void platinum_people_draw(platinum_people *people);

#ifdef __cplusplus
}
#endif

#endif
