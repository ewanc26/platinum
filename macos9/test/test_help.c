/*
 * test_help.c -- host-side tests for the About and Help windows.
 * Pure content and geometry logic; drawing is Toolbox-only.
 */

#include <stdio.h>
#include <string.h>

#include "help.h"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static int inside(const Rect *inner, const Rect *outer)
{
    return inner->left >= outer->left && inner->right <= outer->right &&
           inner->top >= outer->top && inner->bottom <= outer->bottom;
}

int main(void)
{
    Rect port;
    Rect close;
    Point where;
    short i;

    /* Both windows have content, and unknown kinds have none. */
    check(platinum_help_line_count(PLATINUM_HELP_ABOUT) ==
          PLATINUM_HELP_ABOUT_LINES,
          "the About box has its lines");
    check(platinum_help_line_count(PLATINUM_HELP_GUIDE) ==
          PLATINUM_HELP_GUIDE_LINES,
          "the guide has its lines");
    check(platinum_help_line_count(0) == 0, "an unknown kind has no lines");
    check(platinum_help_line(PLATINUM_HELP_ABOUT, -1) == NULL,
          "a negative About index gives nothing");
    check(platinum_help_line(PLATINUM_HELP_ABOUT,
                             PLATINUM_HELP_ABOUT_LINES) == NULL,
          "an About index past the end gives nothing");
    check(platinum_help_line(PLATINUM_HELP_GUIDE,
                             PLATINUM_HELP_GUIDE_LINES) == NULL,
          "a guide index past the end gives nothing");
    check(platinum_help_line(0, 0) == NULL, "an unknown kind gives nothing");

    /* Every line exists and is plain ASCII that fits a 255-char Pascal title
     * budget with room to spare. */
    for (i = 0; i < PLATINUM_HELP_ABOUT_LINES; ++i) {
        const char *line = platinum_help_line(PLATINUM_HELP_ABOUT, i);

        check(line != NULL && line[0] != '\0', "every About line exists");
        if (line != NULL) {
            size_t n;

            check(strlen(line) < 96, "every About line fits the window");
            for (n = 0; line[n] != '\0'; ++n) {
                if ((unsigned char)line[n] > 126) {
                    check(0, "the About text stays ASCII");
                    break;
                }
            }
        }
    }
    for (i = 0; i < PLATINUM_HELP_GUIDE_LINES; ++i) {
        const char *line = platinum_help_line(PLATINUM_HELP_GUIDE, i);

        check(line != NULL && line[0] != '\0', "every guide line exists");
        if (line != NULL) {
            size_t n;

            check(strlen(line) < 96, "every guide line fits the window");
            for (n = 0; line[n] != '\0'; ++n) {
                if ((unsigned char)line[n] > 126) {
                    check(0, "the guide text stays ASCII");
                    break;
                }
            }
        }
    }

    /* The first lines name their windows. */
    check(strcmp(platinum_help_line(PLATINUM_HELP_ABOUT, 0), "Platinum") == 0,
          "the About box opens on Platinum");
    check(strcmp(platinum_help_line(PLATINUM_HELP_GUIDE, 0), "Keyboard") == 0,
          "the guide opens on the keyboard");

    /* The guide names the current shortcuts. */
    {
        int saw_refresh;
        int saw_thread;
        int saw_search;
        int saw_like;

        saw_refresh = 0;
        saw_thread = 0;
        saw_search = 0;
        saw_like = 0;
        for (i = 0; i < PLATINUM_HELP_GUIDE_LINES; ++i) {
            const char *line = platinum_help_line(PLATINUM_HELP_GUIDE, i);

            if (line != NULL) {
                if (strstr(line, "Command-R") != NULL)
                    saw_refresh = 1;
                if (strstr(line, "Command-T") != NULL)
                    saw_thread = 1;
                if (strstr(line, "Command-F") != NULL)
                    saw_search = 1;
                if (strstr(line, "Command-L") != NULL)
                    saw_like = 1;
            }
        }
        check(saw_refresh, "the guide names refresh");
        check(saw_thread, "the guide names the thread window");
        check(saw_search, "the guide names search");
        check(saw_like, "the guide names like");
    }

    /* The Close button sits inside the window, bottom right. */
    SetRect(&port, 92, 62, 648, 430);
    platinum_help_close_rect(&port, &close);
    check(inside(&close, &port), "the Close button stays in the window");
    check(close.right == port.right - 10, "the Close button hugs the right");
    check(close.bottom == port.bottom - 10, "the Close button hugs the bottom");
    check(close.right - close.left == 58, "the Close button keeps its width");
    check(close.bottom - close.top == 24, "the Close button keeps its height");

    /* Clicks on the button close; clicks elsewhere do nothing. */
    where.h = (short)((close.left + close.right) / 2);
    where.v = (short)((close.top + close.bottom) / 2);
    check(platinum_help_close_at(&port, where) == 1, "the button itself hits");
    where.h = (short)(close.left - 4);
    where.v = (short)((close.top + close.bottom) / 2);
    check(platinum_help_close_at(&port, where) == 0,
          "left of the button misses");
    where.h = 100;
    where.v = 100;
    check(platinum_help_close_at(&port, where) == 0,
          "the text area misses");
    check(platinum_help_close_at(NULL, where) == 0,
          "a missing rect never hits");

    if (failures != 0) {
        printf("test_help: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_help: all %d checks passed\n", checks);
    return 0;
}
