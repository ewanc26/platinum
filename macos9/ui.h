#ifndef PLATINUM_UI_H
#define PLATINUM_UI_H

#include "session.h"

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

void platinum_ui_layout_compute(const Rect *content,
                                platinum_ui_layout *layout);
void platinum_ui_draw(GrafPtr port,
                      const platinum_ui_layout *layout,
                      const platinum_session *session);

#ifdef __cplusplus
}
#endif

#endif
