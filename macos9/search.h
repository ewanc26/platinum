#ifndef PLATINUM_SEARCH_H
#define PLATINUM_SEARCH_H

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The bridge limits a query to 100 characters. */
#define PLATINUM_SEARCH_MAX 100

enum {
    PLATINUM_SEARCH_ACCOUNTS = 0,
    PLATINUM_SEARCH_POSTS = 1
};

typedef struct platinum_search {
    WindowPtr window;
    TEHandle field;
    int mode; /* PLATINUM_SEARCH_ACCOUNTS or _POSTS */
    char status[96];
} platinum_search;

enum {
    PLATINUM_SEARCH_NONE = 0,
    PLATINUM_SEARCH_RUN = 1,
    PLATINUM_SEARCH_CANCEL = 2
};

void platinum_search_init(platinum_search *search);
OSErr platinum_search_open(platinum_search *search, int mode);
void platinum_search_close(platinum_search *search);
int platinum_search_handle_event(platinum_search *search, EventRecord *event);
void platinum_search_draw(platinum_search *search);
void platinum_search_set_status(platinum_search *search, const char *status);

/*
 * Turn what was typed (MacRoman) into the query to send: surrounding spaces
 * trimmed, converted to UTF-8. Returns its length, or -1 for an empty query,
 * more than PLATINUM_SEARCH_MAX characters, or one that does not fit `capacity`.
 */
long platinum_search_prepare(const char *macroman, char *utf8, long capacity);

/*
 * The query as UTF-8, trimmed of surrounding spaces, ready for the bridge.
 * Returns the length, or -1 for an empty query or one that does not fit.
 */
long platinum_search_query(const platinum_search *search, char *utf8,
                           long capacity);

#ifdef __cplusplus
}
#endif

#endif
