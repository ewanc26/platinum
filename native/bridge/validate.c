#include "validate.h"

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
