#ifndef PLATINUM_COMPOSE_H
#define PLATINUM_COMPOSE_H

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_compose {
    WindowPtr window;
    TEHandle text;
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

#ifdef __cplusplus
}
#endif

#endif
