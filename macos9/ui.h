#ifndef PLATINUM_UI_H
#define PLATINUM_UI_H

#include "session.h"
#include "timeline.h"

#include <Events.h>
#include <Quickdraw.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_ui_layout {
    Rect toolbar;
    Rect navigation;
    Rect timeline;
    Rect detail;
} platinum_ui_layout;

typedef struct platinum_ui_state {
    short navigation;
    short selected_post;
    short scroll_row;
    int show_detail;
} platinum_ui_state;

enum {
    PLATINUM_UI_ACTION_NONE = 0,
    PLATINUM_UI_ACTION_REFRESH = 1,
    PLATINUM_UI_ACTION_COMPOSE = 2,
    PLATINUM_UI_ACTION_QUIT = 3
};

void platinum_ui_state_init(platinum_ui_state *state);
void platinum_ui_layout_compute(const Rect *content,
                                platinum_ui_layout *layout);
void platinum_ui_draw(GrafPtr port,
                      const platinum_ui_layout *layout,
                      const platinum_ui_state *state,
                      const platinum_session *session);

int platinum_ui_handle_mouse(const platinum_ui_layout *layout,
                             platinum_ui_state *state,
                             Point where);

int platinum_ui_handle_key(const platinum_ui_layout *layout,
                           platinum_ui_state *state,
                           EventRecord *event);

#ifdef __cplusplus
}
#endif

#endif
