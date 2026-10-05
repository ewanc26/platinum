#ifndef PLATINUM_COMPOSE_H
#define PLATINUM_COMPOSE_H

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>
#include "window_kind.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_COMPOSE_MAX_TEXT 300
#define PLATINUM_COMPOSE_STATUS_MAX 127

typedef struct platinum_compose {
    platinum_window_owner owner;
    WindowPtr window;
    TEHandle text;
    int posting;
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
