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

/* The About box lines. */
#define PLATINUM_HELP_ABOUT_LINES 6
/* The guide lines: headings, shortcuts, account and windows. */
#define PLATINUM_HELP_GUIDE_LINES 21

typedef struct platinum_help {
    WindowPtr window;
    short kind;
    /* 0 the text, 1 Close. */
    short focus;
} platinum_help;

/* The line count for `kind` (0 for an unknown kind). */
short platinum_help_line_count(short kind);

/* One content line for `kind`, or NULL when `index` is out of range. */
const char *platinum_help_line(short kind, short index);

/* The Close button rectangle inside `port`. */
void platinum_help_close_rect(const Rect *port, Rect *close);

/* Whether `where` (local coordinates) hits the Close button. */
int platinum_help_close_at(const Rect *port, Point where);

void platinum_help_init(platinum_help *help);
OSErr platinum_help_open(platinum_help *help, short kind);
void platinum_help_close(platinum_help *help);
int platinum_help_handle_event(platinum_help *help, EventRecord *event);
void platinum_help_draw(platinum_help *help);

#ifdef __cplusplus
}
#endif

#endif
