/*
 * test_text_codec.c -- host-side tests for the MacRoman/UTF-8 conversion and
 * its deliberate placeholder for text the Mac cannot represent. Links the real
 * text_codec.c. Not Classic Mac OS 9 validation: no Toolbox text is drawn here.
 */

#include <stdio.h>
#include <string.h>

#include "text_codec.h"

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

static char utf8[PLATINUM_TEXT_UTF8_CAPACITY];
static char macroman[PLATINUM_TEXT_MAX_CODEPOINTS + 1];

static void test_macroman_to_utf8(void)
{
    long length;

    length = platinum_text_macroman_to_utf8("caf\x8e", utf8, sizeof(utf8), NULL);
    check(length == 5 && strcmp(utf8, "caf\xc3\xa9") == 0,
          "MacRoman e-acute becomes two UTF-8 bytes");

    length = platinum_text_macroman_to_utf8("\xdb", utf8, sizeof(utf8), NULL);
    check(length == 3 && strcmp(utf8, "\xe2\x82\xac") == 0,
          "the MacRoman euro sign becomes three UTF-8 bytes");

    check(platinum_text_macroman_to_utf8("abc", utf8, 4, NULL) == 3 &&
              strcmp(utf8, "abc") == 0,
          "a buffer that exactly fits the text and its terminator is enough");
    check(platinum_text_macroman_to_utf8("abc", utf8, 3, NULL) == -1,
          "a buffer one byte short is refused rather than truncated");
    check(platinum_text_macroman_to_utf8(NULL, utf8, sizeof(utf8), NULL) == -1 &&
              platinum_text_macroman_to_utf8("a", NULL, sizeof(utf8), NULL) == -1 &&
              platinum_text_macroman_to_utf8("a", utf8, 0, NULL) == -1,
          "missing arguments and a zero capacity are refused");
    check(platinum_text_macroman_to_utf8("", utf8, sizeof(utf8), NULL) == 0 &&
              utf8[0] == '\0',
          "the empty string converts to the empty string");
}

static void test_round_trip(void)
{
    char one[2];
    long lossy;
    int value;
    int ok;

    ok = 1;
    for (value = 0x80; value <= 0xFF; ++value) {
        one[0] = (char)value;
        one[1] = '\0';
        if (platinum_text_macroman_to_utf8(one, utf8, sizeof(utf8), NULL) < 0 ||
            platinum_text_utf8_to_macroman(utf8, macroman, sizeof(macroman),
                                          &lossy) != 1 ||
            lossy != 0 || macroman[0] != (char)value)
            ok = 0;
    }
    check(ok, "every high MacRoman byte survives a UTF-8 round trip");
}

static void test_placeholder(void)
{
    long lossy;
    long length;

    length = platinum_text_utf8_to_macroman(
        "a" "\xf0\x9f\x98\x80" "b", macroman, sizeof(macroman), &lossy);
    check(length == 3 && strcmp(macroman, "a?b") == 0 && lossy == 1,
          "an emoji becomes one placeholder and is counted lossy");

    length = platinum_text_utf8_to_macroman(
        "\xf0\x9f\x91\xa8" "\xe2\x80\x8d" "\xf0\x9f\x91\xa9" "\xe2\x80\x8d"
        "\xf0\x9f\x91\xa7",
        macroman, sizeof(macroman), &lossy);
    check(length == 1 && strcmp(macroman, "?") == 0 && lossy == 5,
          "a ZWJ family emoji collapses to one placeholder");

    length = platinum_text_utf8_to_macroman(
        "\xf0\x9f\x87\xaf" "\xf0\x9f\x87\xb5",
        macroman, sizeof(macroman), &lossy);
    check(length == 1 && strcmp(macroman, "?") == 0 && lossy == 2,
          "a regional-indicator flag collapses to one placeholder");

    length = platinum_text_utf8_to_macroman(
        "\xe6\x97\xa5" "\xe6\x9c\xac", macroman, sizeof(macroman), &lossy);
    check(length == 1 && strcmp(macroman, "?") == 0 && lossy == 2,
          "a run of CJK text collapses to one placeholder");

    length = platinum_text_utf8_to_macroman(
        "\xf0\x9f\x98\x80" "?" "\xf0\x9f\x98\x80",
        macroman, sizeof(macroman), &lossy);
    check(length == 3 && strcmp(macroman, "???") == 0 && lossy == 2,
          "a literal question mark between placeholders stays its own glyph");

    length = platinum_text_utf8_to_macroman(
        "\xe6\x97\xa5 \xe6\x9c\xac", macroman, sizeof(macroman), &lossy);
    check(length == 3 && strcmp(macroman, "? ?") == 0 && lossy == 2,
          "a representable character ends the placeholder run");

    check(platinum_text_utf8_to_macroman("plain", macroman, sizeof(macroman),
                                         NULL) == 5 &&
              strcmp(macroman, "plain") == 0,
          "ASCII passes through and a null lossy pointer is allowed");
}

static void test_bounds_and_malformed(void)
{
    long lossy;

    check(platinum_text_utf8_to_macroman("a" "\xf0\x9f\x98\x80" "b",
                                         macroman, 4, &lossy) == 3 &&
              strcmp(macroman, "a?b") == 0,
          "the placeholder fits a buffer exactly sized for the result");
    check(platinum_text_utf8_to_macroman("a" "\xf0\x9f\x98\x80" "b",
                                         macroman, 3, &lossy) == -1,
          "a buffer too short for the result is refused");

    check(platinum_text_utf8_to_macroman("\x80", macroman, sizeof(macroman),
                                         &lossy) == -1,
          "a bare continuation byte is malformed");
    check(platinum_text_utf8_to_macroman("\xe6\x97", macroman,
                                         sizeof(macroman), &lossy) == -1,
          "a truncated three-byte sequence is malformed");
    check(platinum_text_utf8_to_macroman("\xc0\x80", macroman,
                                         sizeof(macroman), &lossy) == -1,
          "an overlong two-byte sequence is malformed");
    check(platinum_text_utf8_to_macroman("\xed\xa0\x80", macroman,
                                         sizeof(macroman), &lossy) == -1,
          "a UTF-16 surrogate is malformed");
    check(platinum_text_utf8_to_macroman("\xf4\x90\x80\x80", macroman,
                                         sizeof(macroman), &lossy) == -1,
          "a code point above U+10FFFF is malformed");

    check(platinum_text_utf8_to_macroman(NULL, macroman, sizeof(macroman),
                                         &lossy) == -1 &&
              platinum_text_utf8_to_macroman("a", NULL, sizeof(macroman),
                                             &lossy) == -1 &&
              platinum_text_utf8_to_macroman("a", macroman, 0, &lossy) == -1,
          "missing arguments and a zero capacity are refused");
    check(platinum_text_utf8_to_macroman("", macroman, sizeof(macroman),
                                         &lossy) == 0 &&
              macroman[0] == '\0' && lossy == 0,
          "the empty string converts to the empty string");
}

int main(void)
{
    test_macroman_to_utf8();
    test_round_trip();
    test_placeholder();
    test_bounds_and_malformed();
    if (failures != 0) {
        printf("test_text_codec: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_text_codec: all %d checks passed\n", checks);
    return 0;
}
