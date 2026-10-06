#ifndef PLATINUM_COMPOSE_H
#define PLATINUM_COMPOSE_H

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_COMPOSE_MAX_TEXT 300
#define PLATINUM_COMPOSE_STATUS_MAX 127

typedef struct platinum_compose {
    WindowPtr window;
    TEHandle text;
    int posting;
    /* Set when replying: the post being answered, as the bridge named it. */
    char reply_uri[513];
    char reply_cid[256];
    char reply_to[72];
    char status[PLATINUM_COMPOSE_STATUS_MAX + 1];
} platinum_compose;

enum {
    PLATINUM_COMPOSE_NONE = 0,
    PLATINUM_COMPOSE_POST = 1,
    PLATINUM_COMPOSE_CANCEL = 2
};

OSErr platinum_compose_open(platinum_compose *compose);
void platinum_compose_close(platinum_compose *compose);
int platinum_compose_handle_event(platinum_compose *compose,
                                   EventRecord *event);
/*
 * Make this compose window a reply. `handle` (with or without its @) is only
 * shown to the writer; the uri and cid are sent to the bridge. Fails, leaving
 * a plain post, when either identifier is empty or does not fit.
 */
int platinum_compose_set_reply(platinum_compose *compose,
                               const char *uri,
                               const char *cid,
                               const char *handle);
void platinum_compose_draw(platinum_compose *compose);

OSErr platinum_compose_get_text(const platinum_compose *compose,
                                char *buffer,
                                long capacity);
void platinum_compose_set_posting(platinum_compose *compose,
                                  int posting);
void platinum_compose_set_status(platinum_compose *compose,
                                 const char *status);

#ifdef __cplusplus
}
#endif

#endif
