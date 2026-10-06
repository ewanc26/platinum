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
    /* The line under the text: "Replying to @x" or "Quoting @x". */
    char caption[72];
    /* Set when quoting: the post being quoted. */
    char quote_uri[513];
    char quote_cid[256];
    /* Who may reply to a new post: an index for platinum_bridge_reply_gate_*. */
    int reply_gate;
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
/*
 * Make this compose window a quote of a post. Like a reply it names the post by
 * uri and cid; unlike a reply it keeps the reply gate. Replaces any reply.
 */
int platinum_compose_set_quote(platinum_compose *compose,
                               const char *uri,
                               const char *cid,
                               const char *handle);
/* Step the reply gate to the next choice, wrapping. A reply has no gate (it
 * belongs to the thread root), so this does nothing on one and returns 0. */
int platinum_compose_cycle_gate(platinum_compose *compose);
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
