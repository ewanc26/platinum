#include "profile.h"

#include <stdio.h>
#include <string.h>

static int
is_alnum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int
is_did_char(char c)
{
    return is_alnum(c) || c == '.' || c == '_' || c == ':' || c == '%' || c == '-';
}

/* did:<method>:<id>, method lowercase letters only, id from the DID alphabet. */
static int
valid_did(const char *s, size_t n)
{
    size_t i = 0;
    size_t method_start;
    size_t id_start;

    if (n < 4 || strncmp(s, "did:", 4) != 0) return 0;
    i = 4;
    method_start = i;
    while (i < n && s[i] >= 'a' && s[i] <= 'z') i++;
    if (i == method_start || i >= n || s[i] != ':') return 0;
    i++;
    id_start = i;
    for (; i < n; i++) {
        if (!is_did_char(s[i])) return 0;
    }
    return i > id_start;
}

/* One label: alnum at both ends, hyphens allowed inside, at most 63 bytes. */
static int
valid_label(const char *s, size_t n)
{
    size_t i;
    if (n == 0 || n > 63) return 0;
    if (!is_alnum(s[0]) || !is_alnum(s[n - 1])) return 0;
    for (i = 0; i < n; i++) {
        if (!is_alnum(s[i]) && s[i] != '-') return 0;
    }
    return 1;
}

/* A handle is one or more labels then a TLD that starts with a letter. */
static int
valid_handle(const char *s, size_t n)
{
    size_t start = 0;
    size_t i;
    size_t last_start = 0;
    size_t last_len = 0;
    int labels = 0;

    for (i = 0; i <= n; i++) {
        if (i == n || s[i] == '.') {
            if (!valid_label(s + start, i - start)) return 0;
            last_start = start;
            last_len = i - start;
            labels++;
            start = i + 1;
        }
    }
    if (labels < 2) return 0;
    /* The top-level label must start with a letter; the rule above only checks alnum. */
    if (last_len == 0) return 0;
    return (s[last_start] >= 'a' && s[last_start] <= 'z') ||
           (s[last_start] >= 'A' && s[last_start] <= 'Z');
}

int
platinum_valid_actor(const char *in, char *out, size_t cap)
{
    size_t n;
    size_t i;

    if (in == NULL || out == NULL || cap == 0) return -1;
    n = strlen(in);
    if (n == 0 || n > 253 || n >= cap) return -1;

    if (valid_did(in, n)) {
        memcpy(out, in, n + 1);
        return 0;
    }
    if (valid_handle(in, n)) {
        /* Handles are stored lowercase, as the TypeScript returns them. */
        for (i = 0; i < n; i++) {
            char c = in[i];
            out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        out[n] = '\0';
        return 0;
    }
    out[0] = '\0';
    return -1;
}

/* Appends s to a bounded buffer as a JSON string, escaping what JSON requires. */
typedef struct json_out {
    char *buf;
    size_t cap;
    size_t len;
    int overflow;
} json_out;

static void
put_char(json_out *o, char c)
{
    if (o->overflow) return;
    if (o->len + 1 >= o->cap) {
        o->overflow = 1;
        return;
    }
    o->buf[o->len++] = c;
    o->buf[o->len] = '\0';
}

static void
put_str(json_out *o, const char *s)
{
    while (*s) put_char(o, *s++);
}

static void
put_json_string(json_out *o, const char *s)
{
    static const char hex[] = "0123456789abcdef";
    const unsigned char *p = (const unsigned char *)s;

    put_char(o, '"');
    for (; *p; p++) {
        if (*p == '"') {
            put_str(o, "\\\"");
        } else if (*p == '\\') {
            put_str(o, "\\\\");
        } else if (*p == '\n') {
            put_str(o, "\\n");
        } else if (*p == '\r') {
            put_str(o, "\\r");
        } else if (*p == '\t') {
            put_str(o, "\\t");
        } else if (*p == '\b') {
            put_str(o, "\\b");
        } else if (*p == '\f') {
            put_str(o, "\\f");
        } else if (*p < 0x20) {
            put_str(o, "\\u00");
            put_char(o, hex[*p >> 4]);
            put_char(o, hex[*p & 15]);
        } else {
            put_char(o, (char)*p);
        }
    }
    put_char(o, '"');
}

static void
put_field_sep(json_out *o, int *first)
{
    if (!*first) put_char(o, ',');
    *first = 0;
}

static void
put_string_field(json_out *o, int *first, const char *key, const char *value)
{
    put_field_sep(o, first);
    put_json_string(o, key);
    put_char(o, ':');
    put_json_string(o, value);
}

static void
put_long_field(json_out *o, int *first, const char *key, long value)
{
    char num[32];
    put_field_sep(o, first);
    put_json_string(o, key);
    put_char(o, ':');
    sprintf(num, "%ld", value);
    put_str(o, num);
}

static void
put_bool_field(json_out *o, int *first, const char *key, int value)
{
    put_field_sep(o, first);
    put_json_string(o, key);
    put_str(o, value ? ":true" : ":false");
}

int
platinum_profile_json(const platinum_profile *p, char *buf, size_t cap)
{
    json_out o;
    int first = 1;

    if (p == NULL || buf == NULL || cap == 0) return -1;
    o.buf = buf;
    o.cap = cap;
    o.len = 0;
    o.overflow = 0;
    buf[0] = '\0';

    put_char(&o, '{');
    put_string_field(&o, &first, "did", p->did);
    if (p->has_handle) put_string_field(&o, &first, "handle", p->handle);
    if (p->has_display_name) put_string_field(&o, &first, "displayName", p->display_name);
    if (p->has_description) put_string_field(&o, &first, "description", p->description);
    if (p->has_avatar) put_string_field(&o, &first, "avatar", p->avatar);
    if (p->has_banner) put_string_field(&o, &first, "banner", p->banner);
    if (p->has_followers_count) put_long_field(&o, &first, "followersCount", p->followers_count);
    if (p->has_follows_count) put_long_field(&o, &first, "followsCount", p->follows_count);
    if (p->has_posts_count) put_long_field(&o, &first, "postsCount", p->posts_count);
    if (p->following >= 0) put_bool_field(&o, &first, "following", p->following);
    if (p->followed_by >= 0) put_bool_field(&o, &first, "followedBy", p->followed_by);
    if (p->muted >= 0) put_bool_field(&o, &first, "muted", p->muted);
    if (p->blocking >= 0) put_bool_field(&o, &first, "blocking", p->blocking);
    put_char(&o, '}');

    if (o.overflow) {
        buf[0] = '\0';
        return -1;
    }
    return (int)o.len;
}
