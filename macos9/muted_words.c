/*
 * muted_words.c -- see muted_words.h.
 *
 * A C89 port of Wolfram's wf_muted_words_match rules (wolfram/muted_words.h,
 * built on wf_mod_match_mute_words). There is no allocation and no Toolbox, so
 * the same code is host-tested and compiled for the Mac.
 */

#include "muted_words.h"

#include <string.h>

static int ascii_lower(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

/* An apostrophe is part of a word, so "cat" does not match inside "cat's". */
static int is_word_char(int c)
{
    c = ascii_lower(c);
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'';
}

static int equal_ci(const char *a, const char *b, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (ascii_lower((unsigned char)a[i]) !=
            ascii_lower((unsigned char)b[i]))
            return 0;
    }
    return 1;
}

static int contains_ci(const char *haystack, const char *needle)
{
    size_t h, n, i;
    if (!haystack || !needle) return 0;
    h = strlen(haystack);
    n = strlen(needle);
    if (n == 0 || n > h) return 0;
    for (i = 0; i + n <= h; i++) {
        if (equal_ci(haystack + i, needle, (int)n)) return 1;
    }
    return 0;
}

/* The official matcher treats a phrase, a value with punctuation in it, and a
 * one-character value as a substring; only a plain word gets boundaries. */
static int is_substring_value(const char *value)
{
    size_t i;
    if (strlen(value) <= 1) return 1;
    for (i = 0; value[i] != '\0'; i++) {
        int c = (unsigned char)value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9')))
            return 1;
    }
    return 0;
}

static int whole_word(const char *text, const char *word)
{
    size_t length, total, i;
    if (!text || !word) return 0;
    length = strlen(word);
    total = strlen(text);
    if (length == 0 || length > total) return 0;
    for (i = 0; i + length <= total; i++) {
        if (i > 0 && is_word_char((unsigned char)text[i - 1])) continue;
        if (i + length < total &&
            is_word_char((unsigned char)text[i + length]))
            continue;
        if (equal_ci(text + i, word, (int)length)) return 1;
    }
    return 0;
}

static int text_matches(const char *text, const char *value)
{
    if (!text || !value || value[0] == '\0') return 0;
    if (is_substring_value(value)) return contains_ci(text, value);
    return whole_word(text, value);
}

static int tag_matches(const char *tag, const char *value)
{
    size_t tag_length, value_length;

    if (!tag || !value) return 0;
    if (tag[0] == '#') tag++;
    if (value[0] == '#') value++;
    tag_length = strlen(tag);
    value_length = strlen(value);
    return tag_length > 0 && tag_length == value_length &&
           equal_ci(tag, value, (int)tag_length);
}

static int read_digits(const char *text, int count, int *out)
{
    int i, value = 0;
    for (i = 0; i < count; i++) {
        if (text[i] < '0' || text[i] > '9') return 0;
        value = value * 10 + (text[i] - '0');
    }
    *out = value;
    return 1;
}

/* Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm). */
static long days_from_civil(long year, long month, long day)
{
    long era, year_of_era, day_of_year, day_of_era;

    year -= (month <= 2);
    era = (year >= 0 ? year : year - 399) / 400;
    year_of_era = year - era * 400;
    day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    day_of_era = year_of_era * 365 + year_of_era / 4 -
                 year_of_era / 100 + day_of_year;
    return era * 146097 + day_of_era - 719468;
}

/* RFC3339 in UTC, as the lexicon writes it. A fractional second is allowed and
 * ignored; a numeric offset is refused rather than misread, and refusal means
 * "not expired" to the caller. */
static int parse_time(const char *stamp, long *out)
{
    int year, month, day, hour, minute, second, i;
    char separator;

    if (!stamp || !out) return 0;
    if (strlen(stamp) < 20) return 0;
    if (!read_digits(stamp, 4, &year) || stamp[4] != '-' ||
        !read_digits(stamp + 5, 2, &month) || stamp[7] != '-' ||
        !read_digits(stamp + 8, 2, &day))
        return 0;
    if (stamp[10] != 'T' && stamp[10] != 't') return 0;
    if (!read_digits(stamp + 11, 2, &hour) || stamp[13] != ':' ||
        !read_digits(stamp + 14, 2, &minute) || stamp[16] != ':' ||
        !read_digits(stamp + 17, 2, &second))
        return 0;

    i = 19;
    separator = stamp[i];
    if (separator == '.') {
        i++;
        while (stamp[i] >= '0' && stamp[i] <= '9') i++;
        if (i == 20) return 0;
        separator = stamp[i];
    }
    if (separator != 'Z' && separator != 'z') return 0;
    if (stamp[i + 1] != '\0') return 0;

    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
        minute > 59 || second > 60)
        return 0;

    *out = days_from_civil((long)year, (long)month, (long)day) * 86400L +
           (long)hour * 3600L + (long)minute * 60L + (long)second;
    return 1;
}

static int expired(const char *stamp, long now)
{
    long when;
    if (now <= 0 || !stamp || stamp[0] == '\0') return 0;
    if (!parse_time(stamp, &when)) return 0;
    return when <= now;
}

static void copy_bounded(char *destination, size_t capacity, const char *source)
{
    size_t i = 0;
    while (i + 1 < capacity && source[i] != '\0') {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

void platinum_muted_clear(platinum_muted_list *list)
{
    if (list) memset(list, 0, sizeof *list);
}

int platinum_muted_add(platinum_muted_list *list, const char *value,
                       int content, int tag, int exclude_following,
                       const char *expires_at)
{
    platinum_muted_entry *entry;

    if (!list || !value || value[0] == '\0' ||
        list->count >= PLATINUM_MUTED_MAX)
        return 0;

    entry = &list->words[list->count];
    memset(entry, 0, sizeof *entry);
    copy_bounded(entry->value, sizeof entry->value, value);
    entry->content = content || !tag;
    entry->tag = tag;
    entry->exclude_following = exclude_following;
    if (expires_at)
        copy_bounded(entry->expires_at, sizeof entry->expires_at, expires_at);
    list->count++;
    return 1;
}

int platinum_muted_match(const platinum_muted_list *list, const char *text,
                         const char *const *tags, int tag_count,
                         int author_followed)
{
    int i, t;

    if (!list || list->count <= 0) return 0;

    for (i = 0; i < list->count; i++) {
        const platinum_muted_entry *entry = &list->words[i];

        if (entry->value[0] == '\0') continue;
        if (entry->exclude_following && author_followed) continue;
        if (expired(entry->expires_at, list->now)) continue;

        if ((entry->content || !entry->tag) && text_matches(text, entry->value))
            return 1;

        if (entry->tag && tags) {
            for (t = 0; t < tag_count; t++) {
                if (tag_matches(tags[t], entry->value)) return 1;
            }
        }
    }
    return 0;
}
