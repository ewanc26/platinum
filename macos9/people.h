#ifndef PLATINUM_PEOPLE_H
#define PLATINUM_PEOPLE_H

#include "atproto_client.h"
#include "bridge_client.h"
#include "json_min.h"
#include "muted_words.h"
#include "scrollbar.h"
#include "state_panel.h"

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
    /* 1 when this list is app.bsky.graph.getList's members, whose list URI is
     * in `path`; 0 for a bridge list, search, muted words or named rows. */
    int native_list;
    /* 1 for app.bsky.graph.getFollows, 2 for getFollowers, with the actor in
     * `path`; 0 for every other source. */
    int native_follows;
    int kind;      /* PLATINUM_PEOPLE_ACCOUNTS, _FEEDS or _LISTS */
    short selected; /* row index, or -1 */
    char heading[PLATINUM_PEOPLE_HEADING_MAX];
    char status[PLATINUM_PEOPLE_STATUS_MAX + 1];
    int loading;
    /* 0 the list, 1 Close, 2 the panel action. */
    short focus;
} platinum_people;

enum {
    PLATINUM_PEOPLE_ACCOUNTS = 0,
    PLATINUM_PEOPLE_FEEDS = 1,
    PLATINUM_PEOPLE_LISTS = 2,
    PLATINUM_PEOPLE_WORDS = 3 /* your muted words; the row uri is the word */
};

enum {
    PLATINUM_PEOPLE_NONE = 0,
    PLATINUM_PEOPLE_CLOSE = 1,
    PLATINUM_PEOPLE_LOAD_MORE = 2,
    PLATINUM_PEOPLE_OPEN = 3, /* a row was clicked: see platinum_people_selection */
    PLATINUM_PEOPLE_SIGN_IN = 4,
    PLATINUM_PEOPLE_PAIR = 5
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

/* Fill the muted-words list from the native model. No paging: the list is at
 * most PLATINUM_MUTED_MAX words and the window shows up to its row cap, the
 * same limit the bridge path had. A word that cannot be shown in MacRoman is
 * left out, exactly like the bridge parse. */
wf_status platinum_people_set_words(platinum_people *people,
                                    const platinum_muted_list *words,
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

/*
 * Take named rows from an AppView object array. `name` is the member to show:
 * "displayName" for feed generators, "name" for lists. A row needs a
 * displayable name and an exact AT URI in `uri`; a row missing either is
 * skipped. The caller sets the kind, clears the list and owns the document.
 * No rows past PLATINUM_PEOPLE_MAX are kept.
 */
void platinum_people_take_named(platinum_people *people, platinum_json items,
                                const char *name);

/*
 * Take one page of app.bsky.graph.getList items (`{"subject":profileView}`)
 * into the list: parse each item's subject as an account, drop from the front
 * to stay within the cap, append, and copy `cursor` exactly. A subject without
 * a did is skipped. The caller has cleared the list for a fresh page and owns
 * the document.
 */
void platinum_people_take_subjects(platinum_people *people, platinum_json items,
                                   const char *cursor,
                                   unsigned short *dropped);

/*
 * Open an account's follows or followers through the native session. The actor
 * is a DID or handle, copied exactly for paging. Both keep the People window's
 * 20-row pages and 40-row cap; the next page is
 * platinum_people_follows_more_native. The bridge fallback is
 * platinum_people_load.
 */
wf_status platinum_people_follows_native(platinum_people *people,
                                         platinum_atproto_client *client,
                                         const char *actor,
                                         const char *heading);
wf_status platinum_people_followers_native(platinum_people *people,
                                           platinum_atproto_client *client,
                                           const char *actor,
                                           const char *heading);
wf_status platinum_people_follows_more_native(platinum_people *people,
                                              platinum_atproto_client *client,
                                              unsigned short *dropped);

/* The clicked row after PLATINUM_PEOPLE_OPEN, or NULL. */
const platinum_person *platinum_people_selection(const platinum_people *people);

/* Append the next page; on success `dropped` rows left the front. A failed page
 * keeps what was loaded. WF_ERR_INVALID_ARG when there is no more. */
wf_status platinum_people_load_more(platinum_people *people,
                                    platinum_bridge_client *bridge,
                                    unsigned short *dropped);
int platinum_people_has_more(const platinum_people *people);
/* Set the status line without touching any window. */
void platinum_people_set_status(platinum_people *people, const char *status);

/* The window; none of this runs on a host. */
OSErr platinum_people_open(platinum_people *people);
void platinum_people_close(platinum_people *people);
int platinum_people_handle_event(platinum_people *people, EventRecord *event);
void platinum_people_draw(platinum_people *people);

#ifdef __cplusplus
}
#endif

#endif
