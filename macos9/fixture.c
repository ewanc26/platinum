/*
 * fixture.c -- PlatinumFixture: the production UI drawing and event handling,
 * with deterministic fixture data and no network at all.
 *
 * Why it exists: the real application links Wolfram's Mac OS 9 transport, which
 * needs Open Transport and macTLS, and its data needs a signed-in bridge. A test
 * that is to run unattended under QEMU needs neither. This application links
 * ui.c, timeline.c and the rest of the drawing path unchanged and fills the
 * timeline directly instead of fetching it, so a screenshot shows what
 * platinum_ui_draw actually draws on a Mac.
 *
 * It is built only when -DPLATINUM_BUILD_FIXTURE=ON (CI does, releases do not),
 * carries the PTLF creator rather than PTLM, and its data is compiled in. It has
 * no token, no preferences file and no transport: it cannot talk to a bridge.
 *
 * Keys: 1..9 and 0 switch scenario; arrows and the mouse go through the real
 * platinum_ui_handle_* functions; Command-Q quits. The numbers are the mapping
 * tools/macos9-qemu/harness.py sends.
 */
#include <Events.h>
#include <Fonts.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <TextEdit.h>
#include <Windows.h>

#include <stdio.h>
#include <string.h>

#include "drawutil.h"
#include "session.h"
#include "timeline.h"
#include "ui.h"

static unsigned char kTitle[] = {
    16, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', 'F', 'i', 'x', 't', 'u',
    'r', 'e'
};

static WindowPtr window;
static platinum_timeline timeline;
static platinum_session session;
static platinum_ui_state state;
static platinum_ui_layout layout;
static int running = 1;
static int paired = 1;

/* The production draw path asks the session; this is the fixture's answer. */
int platinum_session_is_paired(const platinum_session *session_value)
{
    (void)session_value;
    return paired;
}

static void fixture_status(const char *text)
{
    strncpy(timeline.status, text, sizeof(timeline.status) - 1);
    timeline.status[sizeof(timeline.status) - 1] = '\0';
}

static void fixture_fill(int count, int long_text)
{
    int index;

    platinum_timeline_init(&timeline);
    for (index = 0; index < count && index < PLATINUM_TIMELINE_MAX_POSTS;
         ++index) {
        sprintf(timeline.posts[index].author, "Fixture Author %d", index);
        sprintf(timeline.posts[index].handle, "@fixture%d.example", index);
        sprintf(timeline.posts[index].time, "%02d:%02d", 9 + index % 10, index);
        if (long_text && index == 0) {
            strcpy(timeline.posts[index].line1, "A deliberately long line that reaches the right edge of the timeline");
            strcpy(timeline.posts[index].line2, "A second long line, clear of the scroll bar, showing wrapping");
            strcpy(timeline.posts[index].line3, "Third line: the row still fits its 62-point height.");
        } else {
            sprintf(timeline.posts[index].line1,
                    "Fixture post %d: a short line of text.", index);
            strcpy(timeline.posts[index].line2,
                   index % 2 == 0 ? "A second line of fixture text." : "");
        }
        timeline.posts[index].like_count = index;
        timeline.posts[index].repost_count = index / 3;
        timeline.posts[index].liked = index % 2;
        timeline.posts[index].reposted = index % 3 == 0;
    }
    timeline.count = (unsigned short)count;
    if (count > 0)
        strcpy(timeline.cursor, "fixture-older-page");
}

static void fixture_open_window(short width, short height)
{
    Rect bounds;

    if (window != NULL)
        DisposeWindow(window);

    SetRect(&bounds, 20, 40, (short)(20 + width), (short)(40 + height));
    window = NULL;
    if (platinum_window_open(&window, &bounds, kTitle) != noErr) {
        running = 0;
        return;
    }

    {
        Rect content;

        content = window->portRect;
        platinum_ui_layout_compute(&content, &layout);
    }
}

static void fixture_draw(void)
{
    if (window == NULL)
        return;

    SetPort((GrafPtr)window);
    platinum_ui_draw(window, &layout, &state, &session, &timeline);
}

static void fixture_scenario(int digit)
{
    platinum_ui_state_init(&state);

    switch (digit) {
        case 1:
            paired = 1;
            fixture_fill(10, 0);
            fixture_status("Loaded 10 fixture posts.");
            break;
        case 2:
            paired = 1;
            fixture_fill(0, 0);
            fixture_status("No posts yet.");
            break;
        case 3:
            paired = 1;
            fixture_fill(1, 0);
            fixture_status("Loaded 1 fixture post.");
            break;
        case 4:
            paired = 1;
            fixture_fill(3, 1);
            fixture_status("Long and wrapped text.");
            break;
        case 5:
            paired = 1;
            fixture_fill(10, 0);
            state.selected_post = 3;
            state.show_detail = 1;
            fixture_status("The inspector follows the selected post.");
            break;
        case 6:
            paired = 1;
            fixture_fill(0, 0);
            timeline.loading = 1;
            fixture_status("Loading posts...");
            break;
        case 7:
            paired = 1;
            fixture_fill(0, 0);
            fixture_status("Could not reach the bridge.");
            break;
        case 8:
            paired = 0;
            fixture_fill(6, 0);
            fixture_status("Not paired: the detail pane says so.");
            break;
        case 9:
            fixture_open_window(640, 480);
            paired = 1;
            fixture_fill(10, 0);
            fixture_status("640x480 content area.");
            break;
        case 0:
            fixture_open_window(800, 600);
            paired = 1;
            fixture_fill(10, 0);
            fixture_status("800x600 content area.");
            break;
        default:
            return;
    }

    InvalRect(&window->portRect);
    fixture_draw();
}

static void fixture_event(EventRecord *event)
{
    int action;
    unsigned char key;

    switch (event->what) {
        case updateEvt:
            if ((WindowPtr)(long)event->message == window) {
                BeginUpdate(window);
                fixture_draw();
                EndUpdate(window);
            }
            break;

        case activateEvt:
            if ((WindowPtr)(long)event->message == window)
                HiliteWindow(window, (event->modifiers & activeFlag) != 0);
            break;

        case mouseDown:
            {
                Point where;

                where = event->where;
                SetPort((GrafPtr)window);
                GlobalToLocal(&where);
                action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline,
                                                  where);
                if (action == PLATINUM_UI_ACTION_QUIT)
                    running = 0;
                fixture_draw();
            }
            break;

        case keyDown:
        case autoKey:
            key = (unsigned char)(event->message & charCodeMask);
            if ((event->modifiers & cmdKey) != 0 &&
                (key == 'q' || key == 'Q')) {
                running = 0;
                break;
            }
            if (key >= '0' && key <= '9') {
                fixture_scenario(key - '0');
                break;
            }
            action = platinum_ui_handle_key(&layout, &state, &session, &timeline, event);
            if (action == PLATINUM_UI_ACTION_QUIT)
                running = 0;
            if (action == PLATINUM_UI_ACTION_LOAD_OLDER) {
                timeline.cursor[0] = '\0';
                fixture_status("The fixture has no older page.");
            }
            fixture_draw();
            break;

        default:
            break;
    }
}

int main(void)
{
    EventRecord event;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitCursor();

    fixture_open_window(640, 480);
    if (!running)
        return 1;

    fixture_scenario(1);

    while (running) {
        if (!WaitNextEvent(everyEvent, &event, 30, NULL))
            continue;
        fixture_event(&event);
    }

    if (window != NULL)
        DisposeWindow(window);
    return 0;
}
