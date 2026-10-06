/*
 * test_search.c -- host-side tests for preparing a typed search query. Not
 * Classic Mac OS 9 validation.
 */
#include <stdio.h>
#include <string.h>

#include "search.h"

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

int main(void)
{
    char out[PLATINUM_SEARCH_MAX * 4];
    char longq[PLATINUM_SEARCH_MAX + 20];

    check(platinum_search_prepare("hello", out, sizeof(out)) == 5 && strcmp(out, "hello") == 0,
          "a plain query passes through");
    check(platinum_search_prepare("  two words  ", out, sizeof(out)) == 9 && strcmp(out, "two words") == 0,
          "surrounding spaces are trimmed, inner ones kept");
    check(platinum_search_prepare("caf\x8E", out, sizeof(out)) == 5 && strcmp(out, "caf\xC3\xA9") == 0,
          "MacRoman is converted to UTF-8");
    check(platinum_search_prepare("", out, sizeof(out)) == -1, "an empty query is refused");
    check(platinum_search_prepare("   ", out, sizeof(out)) == -1, "an all-space query is refused");
    check(platinum_search_prepare(NULL, out, sizeof(out)) == -1, "NULL is refused");
    check(platinum_search_prepare("hello", out, 3) == -1, "a query that does not fit is refused");

    memset(longq, 'a', PLATINUM_SEARCH_MAX);
    longq[PLATINUM_SEARCH_MAX] = '\0';
    check(platinum_search_prepare(longq, out, sizeof(out)) == PLATINUM_SEARCH_MAX,
          "exactly the maximum is accepted");
    memset(longq, 'a', PLATINUM_SEARCH_MAX + 5);
    longq[PLATINUM_SEARCH_MAX + 5] = '\0';
    check(platinum_search_prepare(longq, out, sizeof(out)) == -1, "over the maximum is refused");

    if (failures != 0) {
        printf("test_search: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_search: all %d checks passed\n", checks);
    return 0;
}
