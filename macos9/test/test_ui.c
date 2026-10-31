/*
 * test_ui.c -- host-side tests for the main window's UI logic: layout
 * geometry, mouse hit-testing, selection, scrolling and keyboard shortcuts.
 *
 * Links the real ui.c and timeline.c over the stubbed Wolfram transport and the
 * geometry fixtures in ui_stubs.c. No QuickDraw is drawn and this is not a Mac
 * OS 9 run; it checks the state machine and the rectangles, which is where
 * off-by-one and clamp defects live.
 */

#include <stdio.h>
#include <string.h>

#include <Events.h>
#include <Quickdraw.h>

#include "scrollbar.h"
#include "session.h"
#include "timeline.h"
#include "ui.h"

static int failures;
static int checks;
static platinum_session session;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static void fill(platinum_timeline *timeline, int count, int cursor)
{
    int index;

    platinum_timeline_init(timeline);
    for (index = 0; index < count && index < PLATINUM_TIMELINE_MAX_POSTS;
         ++index) {
        sprintf(timeline->posts[index].author, "Author %d", index);
        sprintf(timeline->posts[index].handle, "@author%d.example", index);
        sprintf(timeline->posts[index].time, "%02d:%02d", 9 + index % 10, index);
        sprintf(timeline->posts[index].line1, "Post number %d", index);
        timeline->posts[index].line2[0] = '\0';
        timeline->posts[index].line3[0] = '\0';
        timeline->posts[index].like_count = index;
        timeline->posts[index].repost_count = index / 2;
    }
    timeline->count = (unsigned short)count;
    if (cursor)
        strcpy(timeline->cursor, "next-page");
    else
        timeline->cursor[0] = '\0';
}

static Point point(short x, short y)
{
    Point p;

    p.h = x;
    p.v = y;
    return p;
}

static void layout_for(platinum_ui_layout *layout, short width, short height)
{
    Rect content;

    SetRect(&content, 0, 0, width, height);
    platinum_ui_layout_compute(&content, layout);
}

static int key_action(const platinum_ui_layout *layout,
                      platinum_ui_state *state,
                      const platinum_timeline *timeline,
                      unsigned char key,
                      short modifiers)
{
    EventRecord event;

    memset(&event, 0, sizeof(event));
    event.what = keyDown;
    event.message = (long)key;
    event.modifiers = modifiers;
    return platinum_ui_handle_key(layout, state, &session, timeline, &event);
}

static int rect_ok(const Rect *rect)
{
    return rect->right >= rect->left && rect->bottom >= rect->top;
}

static void test_layout(void)
{
    platinum_ui_layout layout;

    layout_for(&layout, 640, 480);
    check(rect_ok(&layout.toolbar) && rect_ok(&layout.navigation) &&
              rect_ok(&layout.timeline) && rect_ok(&layout.detail) &&
              rect_ok(&layout.timeline_scrollbar),
          "640x480: every region is non-negative");
    check(layout.toolbar.top == 0 && layout.toolbar.bottom == 30,
          "640x480: toolbar is 30 points");
    check(layout.navigation.top == 31 && layout.navigation.right == 116 &&
              layout.navigation.bottom == 361,
          "640x480: navigation sits below the toolbar, above the inspector");
    check(layout.timeline.left == 117 && layout.timeline.bottom == 361,
          "640x480: timeline starts after the navigation column");
    check(layout.timeline_scrollbar.left == 625 &&
              layout.timeline.right == 624,
          "640x480: the scroll bar takes the timeline's last 15 points");
    check(layout.detail.top == 362 && layout.detail.bottom == 480,
          "640x480: inspector takes the documented 118 points");
    check(layout.toolbar.bottom <= layout.navigation.top &&
              layout.navigation.bottom < layout.detail.top,
          "640x480: regions are ordered top to bottom");
    check(layout.navigation.right < layout.timeline.left &&
              layout.timeline.right < layout.timeline_scrollbar.left,
          "640x480: columns are ordered left to right");

    layout_for(&layout, 800, 600);
    check(layout.detail.top == 482 && layout.detail.bottom == 600,
          "800x600: inspector grows with the window");
    check(layout.timeline_scrollbar.left == 785 && layout.timeline.right == 784,
          "800x600: scroll bar hugs the right edge");
    check(layout.navigation.right == 116,
          "800x600: navigation keeps its preferred width");

    layout_for(&layout, 400, 480);
    check(layout.navigation.right == 96,
          "a narrow window uses the compact navigation width");

    layout_for(&layout, 640, 280);
    check(layout.detail.top == 208 && layout.detail.bottom == 280,
          "a short window uses the 72-point inspector");
    check(layout.navigation.bottom == 207,
          "and the timeline stops above it");

    layout_for(&layout, 320, 100);
    check(rect_ok(&layout.toolbar) && rect_ok(&layout.navigation) &&
              rect_ok(&layout.timeline) && rect_ok(&layout.detail),
          "below the minimum size, every region stays non-negative");
    check(layout.navigation.bottom >= layout.navigation.top &&
              layout.detail.top >= layout.navigation.bottom,
          "below the minimum size, regions stay ordered");

    layout_for(&layout, 0, 0);
    check(rect_ok(&layout.toolbar) && rect_ok(&layout.navigation) &&
              rect_ok(&layout.timeline) && rect_ok(&layout.detail),
          "an empty content rect does not make a negative region");
}

static void test_mouse(void)
{
    platinum_ui_layout layout;
    platinum_ui_state state;
    platinum_timeline timeline;
    int action;

    layout_for(&layout, 640, 480);
    fill(&timeline, 10, 1);
    platinum_ui_state_init(&state);

    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(96, 10));
    check(action == PLATINUM_UI_ACTION_REFRESH, "Refresh starts at its left edge");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(95, 10));
    check(action == PLATINUM_UI_ACTION_NONE, "one point left of Refresh is not Refresh");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(181, 10));
    check(action == PLATINUM_UI_ACTION_REFRESH, "Refresh includes its last point");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(182, 10));
    check(action == PLATINUM_UI_ACTION_NONE, "the gap after Refresh does nothing");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(190, 10));
    check(action == PLATINUM_UI_ACTION_COMPOSE, "Post... starts at its left edge");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(189, 10));
    check(action == PLATINUM_UI_ACTION_NONE, "one point left of Post... is not Post...");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(10, 31));
    check(action == PLATINUM_UI_ACTION_NONE && state.navigation == 0,
          "the first navigation row is Home");

    state.selected_post = 7;
    state.scroll_row = 3;
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(10, 65));
    check(action == PLATINUM_UI_ACTION_NOTIFICATIONS && state.navigation == 1,
          "the second navigation row opens Notifications");
    check(state.selected_post == 0 && state.scroll_row == 0,
          "changing navigation resets the post selection and scroll");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(10, 93));
    check(action == PLATINUM_UI_ACTION_PROFILE && state.navigation == 2,
          "the third navigation row opens Profile");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(10, 121));
    check(action == PLATINUM_UI_ACTION_NONE && state.navigation == 2,
          "a click past the last navigation row changes nothing");

    platinum_ui_state_init(&state);
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(200, 37));
    check(state.selected_post == 0 && state.show_detail == 1,
          "the first timeline row selects post zero");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(200, 100));
    check(state.selected_post == 0, "63 points into a row is still that row");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(200, 101));
    check(state.selected_post == 1, "the next row starts 64 points later");

    state.selected_post = 0;
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(200, 36));
    check(state.selected_post == 0,
          "clicking the Timeline heading does not select row zero");

    state.scroll_row = 2;
    platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(200, 37));
    check(state.selected_post == 2, "a scrolled list selects from the first visible row");

    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(200, 229));
    check(state.selected_post == 5 && action == PLATINUM_UI_ACTION_NONE,
          "a click three rows down selects the sixth visible post");

    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(1, 470));
    check(action == PLATINUM_UI_ACTION_NONE && state.selected_post == 5,
          "clicking the inspector does not change the timeline selection");
}

static void test_keys(void)
{
    platinum_ui_layout layout;
    platinum_ui_state state;
    platinum_timeline timeline;
    int action;
    int i;

    layout_for(&layout, 640, 480);
    fill(&timeline, 10, 1);
    platinum_ui_state_init(&state);

    check(key_action(&layout, &state, &timeline, downArrow, 0) ==
              PLATINUM_UI_ACTION_NONE &&
              state.selected_post == 1,
          "Down moves the selection by one");

    state.selected_post = 0;
    check(key_action(&layout, &state, &timeline, upArrow, 0) ==
              PLATINUM_UI_ACTION_NONE &&
              state.selected_post == 0,
          "Up at the first post stays there");

    for (i = 0; i < 5; ++i)
        key_action(&layout, &state, &timeline, downArrow, 0);
    check(state.selected_post == 5 && state.scroll_row == 2,
          "moving past the visible rows scrolls one row at a time");

    for (i = 0; i < 4; ++i)
        key_action(&layout, &state, &timeline, upArrow, 0);
    check(state.selected_post == 1 && state.scroll_row == 1,
          "moving back up scrolls the list to keep the selection visible");

    for (i = 0; i < 12; ++i)
        key_action(&layout, &state, &timeline, rightArrow, 0);
    check(state.scroll_row == 6, "Right stops at the last full page");

    for (i = 0; i < 12; ++i)
        key_action(&layout, &state, &timeline, leftArrow, 0);
    check(state.scroll_row == 0, "Left stops at the first row");

    key_action(&layout, &state, &timeline, pageDown, 0);
    check(state.scroll_row == 4, "Page Down moves one page");
    key_action(&layout, &state, &timeline, pageDown, 0);
    check(state.scroll_row == 6, "Page Down clamps to the last full page");
    key_action(&layout, &state, &timeline, pageUp, 0);
    check(state.scroll_row == 2, "Page Up moves one page back");

    state.selected_post = 9;
    action = key_action(&layout, &state, &timeline, downArrow, 0);
    check(action == PLATINUM_UI_ACTION_LOAD_OLDER && state.selected_post == 9,
          "Down on the last post asks for the next page");

    timeline.cursor[0] = '\0';
    action = key_action(&layout, &state, &timeline, downArrow, 0);
    check(action == PLATINUM_UI_ACTION_NONE && state.selected_post == 9,
          "with nothing older, Down on the last post does nothing");

    check(key_action(&layout, &state, &timeline, 'r', cmdKey) ==
              PLATINUM_UI_ACTION_REFRESH,
          "Command-R refreshes");
    check(key_action(&layout, &state, &timeline, 'n', cmdKey) ==
              PLATINUM_UI_ACTION_COMPOSE,
          "Command-N composes");
    check(key_action(&layout, &state, &timeline, 'q', cmdKey) ==
              PLATINUM_UI_ACTION_QUIT,
          "Command-Q quits");
    check(key_action(&layout, &state, &timeline, 'l', cmdKey) ==
              PLATINUM_UI_ACTION_LIKE,
          "Command-L likes");
    check(key_action(&layout, &state, &timeline, 'e', cmdKey) ==
              PLATINUM_UI_ACTION_REPOST,
          "Command-E reposts");
    check(key_action(&layout, &state, &timeline, 'j', cmdKey) ==
              PLATINUM_UI_ACTION_REPLY,
          "Command-J replies");
    check(key_action(&layout, &state, &timeline, 't', cmdKey) ==
              PLATINUM_UI_ACTION_THREAD,
          "Command-T shows the thread");
    check(key_action(&layout, &state, &timeline, 'i', cmdKey) ==
              PLATINUM_UI_ACTION_AUTHOR,
          "Command-I shows the author");
    check(key_action(&layout, &state, &timeline, 'y', cmdKey) ==
              PLATINUM_UI_ACTION_FOLLOW,
          "Command-Y follows");
    check(key_action(&layout, &state, &timeline, 'z', cmdKey) ==
              PLATINUM_UI_ACTION_NONE,
          "an unbound Command key does nothing");

    /* A keyDown the UI does not handle must not consume the selection. */
    state.selected_post = 3;
    check(key_action(&layout, &state, &timeline, 'z', 0) ==
              PLATINUM_UI_ACTION_NONE &&
              state.selected_post == 3,
          "a printable key leaves the selection alone");
}

static void test_edge_cases(void)
{
    platinum_ui_layout layout;
    platinum_ui_state state;
    platinum_timeline timeline;
    int action;

    layout_for(&layout, 640, 480);

    platinum_ui_state_init(&state);
    check(state.navigation == 0 && state.selected_post == 0 &&
              state.scroll_row == 0 && state.show_detail == 1,
          "the initial state is Home, post zero, showing detail");

    fill(&timeline, 0, 0);
    action = key_action(&layout, &state, &timeline, downArrow, 0);
    check(action == PLATINUM_UI_ACTION_NONE && state.selected_post == 0,
          "an empty timeline ignores Down");
    action = key_action(&layout, &state, &timeline, upArrow, 0);
    check(action == PLATINUM_UI_ACTION_NONE && state.selected_post == 0,
          "an empty timeline ignores Up");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(200, 37));
    check(action == PLATINUM_UI_ACTION_NONE && state.selected_post == 0,
          "an empty timeline cannot be clicked into a selection");
    key_action(&layout, &state, &timeline, pageDown, 0);
    check(state.scroll_row == 0, "an empty timeline does not scroll");

    fill(&timeline, 1, 1);
    platinum_ui_state_init(&state);
    action = key_action(&layout, &state, &timeline, downArrow, 0);
    check(action == PLATINUM_UI_ACTION_LOAD_OLDER && state.selected_post == 0,
          "a one-post timeline asks for more at its end");
    action = platinum_ui_handle_mouse(&layout, &state, &session, &timeline, point(200, 37));
    check(state.selected_post == 0 && state.show_detail == 1,
          "the single row can be selected");

    /* Resizing recomputes geometry but must not reset what the reader chose. */
    state.selected_post = 0;
    state.scroll_row = 0;
    layout_for(&layout, 800, 600);
    check(state.navigation == 0 && state.selected_post == 0,
          "recomputing the layout leaves the selection alone");

    /* Scrolling clamps against the count, not the page size. */
    fill(&timeline, 5, 0);
    layout_for(&layout, 640, 480);
    platinum_ui_state_init(&state);
    key_action(&layout, &state, &timeline, pageDown, 0);
    check(state.scroll_row == 1,
          "with five posts a page-down stops at the last full page");
}

int main(void)
{
    test_layout();
    test_mouse();
    test_keys();
    test_edge_cases();

    if (failures != 0) {
        printf("test_ui: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_ui: all %d checks passed\n", checks);
    return 0;
}
