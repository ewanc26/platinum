#include "validate.h"

#include <stdio.h>
#include <string.h>

#include "profile.h"

#define REPLY_MAX_CODEPOINTS 1000
#define QUERY_MAX_CODEPOINTS 100

long
platinum_utf8_length(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    long n = 0;

    if (s == NULL) return -1;
    for (; *p; p++) {
        if ((*p & 0xC0) != 0x80) n++;
    }
    return n;
}

static int
is_blank_byte(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int
platinum_valid_reply_text(const char *text)
{
    const char *p;
    int has_content = 0;

    if (text == NULL) return -1;
    for (p = text; *p; p++) {
        if (!is_blank_byte((unsigned char)*p)) {
            has_content = 1;
            break;
        }
    }
    if (!has_content) return -1;
    return platinum_utf8_length(text) > REPLY_MAX_CODEPOINTS ? -1 : 0;
}

static int
is_alnum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int
is_rkey_char(char c)
{
    return is_alnum(c) || c == '.' || c == '_' || c == '~' || c == ':' || c == '-';
}

/* did:<method>:<id>, the id from the DID alphabet, starting at s. Returns the
 * index just past the id, or 0 if s is not a DID. */
static size_t
scan_did(const char *s)
{
    size_t i = 0;

    if (strncmp(s, "did:", 4) != 0) return 0;
    i = 4;
    if (!(s[i] >= 'a' && s[i] <= 'z')) return 0;
    while (s[i] >= 'a' && s[i] <= 'z') i++;
    if (s[i] != ':') return 0;
    i++;
    if (!is_alnum(s[i]) && s[i] != '.' && s[i] != '_' && s[i] != ':' && s[i] != '%' && s[i] != '-') return 0;
    while (is_alnum(s[i]) || s[i] == '.' || s[i] == '_' || s[i] == ':' || s[i] == '%' || s[i] == '-') i++;
    return i;
}

int
platinum_valid_post_ref(const char *uri, const char *cid)
{
    const char *collection = "/app.bsky.feed.post/";
    size_t i;
    size_t rkey;
    size_t n;

    if (uri == NULL || cid == NULL) return -1;
    if (strlen(uri) > 512 || strlen(cid) > 255 || cid[0] == '\0') return -1;

    if (strncmp(uri, "at://", 5) != 0) return -1;
    i = scan_did(uri + 5);
    if (i == 0) return -1;
    i += 5;
    if (strncmp(uri + i, collection, strlen(collection)) != 0) return -1;
    rkey = i + strlen(collection);
    if (uri[rkey] == '\0') return -1;
    for (n = rkey; uri[n] != '\0'; n++) {
        if (!is_rkey_char(uri[n])) return -1;
    }

    for (i = 0; cid[i] != '\0'; i++) {
        if (!is_alnum(cid[i])) return -1;
    }
    return 0;
}

int
platinum_valid_subject_did(const char *did)
{
    char out[256];

    if (did == NULL || strncmp(did, "did:", 4) != 0) return -1;
    return platinum_valid_actor(did, out, sizeof out) == 0 ? 0 : -1;
}

int
platinum_valid_query(const char *in, char *out, size_t cap)
{
    const char *start;
    const char *end;
    size_t n;
    const unsigned char *p;

    if (in == NULL || out == NULL || cap == 0) return -1;
    out[0] = '\0';

    start = in;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
    n = (size_t)(end - start);
    if (n == 0 || n >= cap) return -1;

    for (p = (const unsigned char *)start; p < (const unsigned char *)end; p++) {
        if (*p < 0x20 || *p == 0x7F) return -1;
    }
    if (platinum_utf8_length(start) > QUERY_MAX_CODEPOINTS) {
        /* Count only the trimmed text. */
        char tmp[512];
        if (n >= sizeof tmp) return -1;
        memcpy(tmp, start, n);
        tmp[n] = '\0';
        if (platinum_utf8_length(tmp) > QUERY_MAX_CODEPOINTS) return -1;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return 0;
}

int
platinum_valid_muted_word(const char *in, char *out, size_t cap)
{
    return platinum_valid_query(in, out, cap);
}

/* Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm). */
static double
days_from_civil(long y, long m, long d)
{
    long era;
    long yoe;
    long doy;
    long doe;

    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (double)era * 146097.0 + (double)doe - 719468.0;
}

static int
digits(const char *p, int n, long *out)
{
    long v = 0;
    int i;
    for (i = 0; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') return 0;
        v = v * 10 + (p[i] - '0');
    }
    *out = v;
    return 1;
}

/* Writes ms since the epoch as YYYY-MM-DDTHH:MM:SS.mmmZ, the form toISOString uses. */
static void
format_iso(double ms, char *out)
{
    long z = (long)(ms / 86400000.0);
    double rem;
    long era, doe, yoe, doy, mp, y, d, m, hh, mi, ss, msec;

    /* Round toward negative infinity so times before 1970 keep the right day. */
    if (ms < 0 && (double)z * 86400000.0 != ms) z -= 1;
    rem = ms - (double)z * 86400000.0;

    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    y += m <= 2;

    msec = (long)rem;
    hh = msec / 3600000;
    mi = (msec / 60000) % 60;
    ss = (msec / 1000) % 60;
    msec = msec % 1000;
    sprintf(out, "%04ld-%02ld-%02ldT%02ld:%02ld:%02ld.%03ldZ", y, m, d, hh, mi, ss, msec);
}

int
platinum_valid_seen_at(const char *in, double now_ms, char *out, size_t cap)
{
    const char *p;
    long y, mo, d, hh, mi, ss, ms = 0, oh = 0, om = 0;
    double t;
    double offset_ms = 0;
    size_t n;
    int sign = 0;
    int frac_nonzero = 0;

    if (in == NULL || out == NULL || cap < 26) return -1;
    out[0] = '\0';
    n = strlen(in);
    if (n > 64 || n < 20) return -1;

    /* YYYY-MM-DDTHH:MM:SS */
    if (!digits(in, 4, &y) || in[4] != '-' || !digits(in + 5, 2, &mo) || in[7] != '-' ||
        !digits(in + 8, 2, &d) || in[10] != 'T' || !digits(in + 11, 2, &hh) || in[13] != ':' ||
        !digits(in + 14, 2, &mi) || in[16] != ':' || !digits(in + 17, 2, &ss)) {
        return -1;
    }
    p = in + 19;
    if (*p == '.') {
        size_t k = 1;
        long frac = 0;
        int scale = 0;
        while (p[k] >= '0' && p[k] <= '9') {
            if (p[k] != '0') frac_nonzero = 1;
            if (scale < 3) {
                frac = frac * 10 + (p[k] - '0');
                scale++;
            }
            k++;
        }
        if (k == 1) return -1;
        while (scale < 3) {
            frac *= 10;
            scale++;
        }
        ms = frac;
        p += k;
    }
    if (*p == 'Z' && p[1] == '\0') {
        /* UTC */
    } else if ((*p == '+' || *p == '-') && strlen(p) == 6 && p[3] == ':' &&
               digits(p + 1, 2, &oh) && digits(p + 4, 2, &om)) {
        sign = (*p == '+') ? 1 : -1;
        if (oh > 23 || om > 59) return -1;
        offset_ms = sign * (double)(oh * 3600000L + om * 60000L);
    } else {
        return -1;
    }

    /* As Date.parse does: a day of 1 to 31 is accepted in any month and rolls over
     * (2023-02-30 is 2023-03-02); hour 24 is only the end of the day, at 24:00:00. */
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return -1;
    if (hh == 24) {
        if (mi != 0 || ss != 0 || ms != 0 || frac_nonzero) return -1;
    } else if (hh > 23) {
        return -1;
    }
    if (mi > 59 || ss > 59) return -1;

    t = days_from_civil(y, mo, d) * 86400000.0 + (double)(hh * 3600000L + mi * 60000L + ss * 1000L + ms);
    t -= offset_ms;

    if (t > now_ms) t = now_ms;
    format_iso(t, out);
    return 0;
}
