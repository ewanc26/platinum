#ifndef PLATINUM_HELP_H
#define PLATINUM_HELP_H

#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PLATINUM_HELP_ABOUT = 1,
    PLATINUM_HELP_GUIDE = 2
};

enum {
    PLATINUM_HELP_NONE = 0,
    PLATINUM_HELP_CLOSE = 1
};

typedef struct platinum_help {
    WindowPtr window;
    short kind;
} platinum_help;

void platinum_help_init(platinum_help *help);
OSErr platinum_help_open(platinum_help *help, short kind);
void platinum_help_close(platinum_help *help);
int platinum_help_handle_event(platinum_help *help,
                               EventRecord *event);
void platinum_help_draw(platinum_help *help);

#ifdef __cplusplus
}
#endif

#endif
