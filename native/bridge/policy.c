#include <ctype.h>
#include <string.h>

#include "policy.h"

#define POLICY_MAX_INPUT 2048
#define POLICY_MAX_HOST   253
#define POLICY_ORIGIN_PREFIX "https://"

/* Trims ASCII whitespace from s. Returns the trimmed length, sets *begin. */
static size_t trimmed(const char *s, const char **begin)
{
    const char *b = s;
    const char *e;

    while (*b != '\0' && isspace((unsigned char)*b)) {
        b++;
    }
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) {
        e--;
    }
    *begin = b;
    return (size_t)(e - b);
}

static int is_https_scheme(const char *s, size_t n)
{
    static const char want[] = "https";
    size_t i;

    if (n != 5) {
        return 0;
    }
    for (i = 0; i < 5; i++) {
        if (tolower((unsigned char)s[i]) != want[i]) {
            return 0;
        }
    }
    return 1;
}

static int valid_scheme(const char *s, size_t n)
{
    size_t i;

    if (n == 0 || !isalpha((unsigned char)s[0])) {
        return 0;
    }
    for (i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!isalnum(c) && c != '+' && c != '-' && c != '.') {
            return 0;
        }
    }
    return 1;
}

/* Empty means no port. Otherwise digits only, and the value must be 443. */
static int check_port(const char *s, size_t n)
{
    size_t i;
    long value = 0;

    if (n == 0) {
        return PLATINUM_POLICY_OK;
    }
    for (i = 0; i < n; i++) {
        if (!isdigit((unsigned char)s[i])) {
            return PLATINUM_POLICY_ERR_NOT_URL;
        }
    }
    while (n > 1 && *s == '0') {
        s++;
        n--;
    }
    if (n > 5) {
        return PLATINUM_POLICY_ERR_NOT_URL;
    }
    for (i = 0; i < n; i++) {
        value = value * 10 + (s[i] - '0');
    }
    if (value > 65535) {
        return PLATINUM_POLICY_ERR_NOT_URL;
    }
    if (value != 443) {
        return PLATINUM_POLICY_ERR_PORT;
    }
    return PLATINUM_POLICY_OK;
}

/*
 * True when the last dot-separated label is all decimal digits, or 0x-prefixed
 * hex. That is how WHATWG URL parsing treats a dotted-number host such as
 * "127.1" or "0x7f.0.0.1", so it counts as an IP literal.
 */
static int looks_ipv4(const char *h, size_t n)
{
    size_t i, start, len;
    const char *last;

    if (n > 0 && h[n - 1] == '.') {
        n--;
    }
    start = n;
    while (start > 0 && h[start - 1] != '.') {
        start--;
    }
    last = h + start;
    len = n - start;
    if (len == 0) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (!isdigit((unsigned char)last[i])) {
            break;
        }
    }
    if (i == len) {
        return 1;
    }
    if (len >= 2 && last[0] == '0' && (last[1] == 'x' || last[1] == 'X')) {
        for (i = 2; i < len; i++) {
            if (!isxdigit((unsigned char)last[i])) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

static int has_suffix(const char *s, size_t n, const char *suffix)
{
    size_t m = strlen(suffix);

    return n >= m && memcmp(s + n - m, suffix, m) == 0;
}

int platinum_policy_validate_service(const char *input, char *out_origin, size_t out_size)
{
    char text[POLICY_MAX_INPUT + 1];
    char host[POLICY_MAX_HOST + 1];
    const char *begin;
    const char *colon;
    const char *auth;
    const char *auth_end;
    const char *close_bracket;
    const char *port_src;
    const char *pc;
    size_t len, scheme_len, auth_len, host_len, port_len, i, chk_len, origin_len;
    int rc;

    if (out_origin == NULL || out_size == 0) {
        return PLATINUM_POLICY_ERR_ARGS;
    }
    out_origin[0] = '\0';

    /* NULL, empty or whitespace-only input all mean the default service. */
    if (input == NULL) {
        input = PLATINUM_DEFAULT_SERVICE;
    }
    len = trimmed(input, &begin);
    if (len == 0) {
        len = trimmed(PLATINUM_DEFAULT_SERVICE, &begin);
    }
    if (len > POLICY_MAX_INPUT) {
        return PLATINUM_POLICY_ERR_TOO_LONG;
    }
    memcpy(text, begin, len);
    text[len] = '\0';

    /* scheme ":" then any slashes (WHATWG skips them for special schemes) */
    colon = strchr(text, ':');
    if (colon == NULL) {
        return PLATINUM_POLICY_ERR_NOT_URL;
    }
    scheme_len = (size_t)(colon - text);
    if (!valid_scheme(text, scheme_len)) {
        return PLATINUM_POLICY_ERR_NOT_URL;
    }
    if (!is_https_scheme(text, scheme_len)) {
        return PLATINUM_POLICY_ERR_NOT_HTTPS;
    }

    auth = colon + 1;
    while (*auth == '/' || *auth == '\\') {
        auth++;
    }
    auth_end = auth;
    while (*auth_end != '\0' && *auth_end != '/' && *auth_end != '\\'
           && *auth_end != '?' && *auth_end != '#') {
        auth_end++;
    }
    auth_len = (size_t)(auth_end - auth);

    if (memchr(auth, '@', auth_len) != NULL) {
        return PLATINUM_POLICY_ERR_CREDENTIALS;
    }

    if (auth_len > 0 && auth[0] == '[') {
        /* IPv6 literal: [host]  or  [host]:port */
        close_bracket = memchr(auth, ']', auth_len);
        if (close_bracket == NULL) {
            return PLATINUM_POLICY_ERR_NOT_URL;
        }
        host_len = (size_t)(close_bracket - (auth + 1));
        if (close_bracket + 1 < auth_end) {
            if (close_bracket[1] != ':') {
                return PLATINUM_POLICY_ERR_NOT_URL;
            }
            port_src = close_bracket + 2;
        } else {
            port_src = auth_end;
        }
        port_len = (size_t)(auth_end - port_src);
        rc = check_port(port_src, port_len);
        if (rc != PLATINUM_POLICY_OK) {
            return rc;
        }
        if (host_len == 0 || memchr(auth + 1, ':', host_len) == NULL) {
            return PLATINUM_POLICY_ERR_NOT_URL;
        }
        return PLATINUM_POLICY_ERR_IP_LITERAL;
    }

    /* host[:port] */
    pc = memchr(auth, ':', auth_len);
    host_len = pc != NULL ? (size_t)(pc - auth) : auth_len;
    port_src = pc != NULL ? pc + 1 : auth_end;
    port_len = (size_t)(auth_end - port_src);
    rc = check_port(port_src, port_len);
    if (rc != PLATINUM_POLICY_OK) {
        return rc;
    }
    if (host_len == 0) {
        return PLATINUM_POLICY_ERR_NOT_URL;
    }
    if (host_len > POLICY_MAX_HOST) {
        return PLATINUM_POLICY_ERR_TOO_LONG;
    }
    for (i = 0; i < host_len; i++) {
        unsigned char c = (unsigned char)auth[i];
        if (!isalnum(c) && c != '-' && c != '.') {
            return PLATINUM_POLICY_ERR_NOT_URL;
        }
        host[i] = (char)tolower(c);
    }
    host[host_len] = '\0';

    if (looks_ipv4(host, host_len)) {
        return PLATINUM_POLICY_ERR_IP_LITERAL;
    }

    /* Public name checks. A single trailing dot is ignored for these only. */
    chk_len = host_len;
    if (chk_len > 0 && host[chk_len - 1] == '.') {
        chk_len--;
    }
    if (chk_len == 0) {
        return PLATINUM_POLICY_ERR_NOT_URL;
    }
    if (memchr(host, '.', chk_len) == NULL
        || (chk_len == 9 && memcmp(host, "localhost", 9) == 0)
        || has_suffix(host, chk_len, ".localhost")
        || has_suffix(host, chk_len, ".local")
        || has_suffix(host, chk_len, ".internal")) {
        return PLATINUM_POLICY_ERR_NOT_PUBLIC;
    }

    origin_len = strlen(POLICY_ORIGIN_PREFIX) + host_len;
    if (origin_len + 1 > out_size) {
        return PLATINUM_POLICY_ERR_TOO_LONG;
    }
    memcpy(out_origin, POLICY_ORIGIN_PREFIX, strlen(POLICY_ORIGIN_PREFIX));
    memcpy(out_origin + strlen(POLICY_ORIGIN_PREFIX), host, host_len);
    out_origin[origin_len] = '\0';
    return PLATINUM_POLICY_OK;
}

/* ---- failure limiter ---------------------------------------------------- */

/* Keys are compared on their first PLATINUM_LIMITER_KEY_LEN - 1 bytes. */
static int find_slot(const platinum_failure_limiter *l, const char *key)
{
    int i;

    for (i = 0; i < PLATINUM_LIMITER_KEYS; i++) {
        if (l->entries[i].in_use
            && strncmp(l->entries[i].key, key, PLATINUM_LIMITER_KEY_LEN - 1) == 0) {
            return i;
        }
    }
    return -1;
}

/* Returns a free slot, or evicts the key whose latest failure is oldest. */
static int claim_slot(platinum_failure_limiter *l)
{
    int i, victim = -1;

    for (i = 0; i < PLATINUM_LIMITER_KEYS; i++) {
        if (!l->entries[i].in_use) {
            l->used++;
            return i;
        }
    }
    for (i = 0; i < PLATINUM_LIMITER_KEYS; i++) {
        long latest = l->entries[i].when[l->entries[i].count - 1];

        if (victim < 0
            || latest < l->entries[victim].when[l->entries[victim].count - 1]) {
            victim = i;
        }
    }
    return victim;
}

static int recent_count(const platinum_limiter_entry *e, long window, long now)
{
    long cutoff = now - window;
    int i, n = 0;

    for (i = 0; i < e->count; i++) {
        if (e->when[i] > cutoff) {
            n++;
        }
    }
    return n;
}

void platinum_limiter_init(platinum_failure_limiter *l, int max, long window_seconds)
{
    int i;

    if (l == NULL) {
        return;
    }
    if (max <= 0) {
        max = PLATINUM_LIMITER_DEFAULT_MAX;
    }
    if (max > PLATINUM_LIMITER_STAMPS) {
        max = PLATINUM_LIMITER_STAMPS;
    }
    if (window_seconds <= 0) {
        window_seconds = PLATINUM_LIMITER_DEFAULT_WINDOW;
    }
    l->max = max;
    l->window = window_seconds;
    l->used = 0;
    for (i = 0; i < PLATINUM_LIMITER_KEYS; i++) {
        l->entries[i].in_use = 0;
        l->entries[i].count = 0;
        l->entries[i].key[0] = '\0';
    }
}

int platinum_limiter_blocked(platinum_failure_limiter *l, const char *key, long now)
{
    int slot;

    if (l == NULL || key == NULL || key[0] == '\0') {
        return 0;
    }
    slot = find_slot(l, key);
    if (slot < 0) {
        return 0;
    }
    return recent_count(&l->entries[slot], l->window, now) >= l->max;
}

void platinum_limiter_fail(platinum_failure_limiter *l, const char *key, long now)
{
    int slot, i;
    platinum_limiter_entry *e;

    if (l == NULL || key == NULL || key[0] == '\0') {
        return;
    }
    slot = find_slot(l, key);
    if (slot < 0) {
        slot = claim_slot(l);
        e = &l->entries[slot];
        e->in_use = 1;
        e->count = 0;
        strncpy(e->key, key, PLATINUM_LIMITER_KEY_LEN - 1);
        e->key[PLATINUM_LIMITER_KEY_LEN - 1] = '\0';
    }
    e = &l->entries[slot];
    if (e->count < PLATINUM_LIMITER_STAMPS) {
        e->when[e->count] = now;
        e->count++;
    } else {
        /* Full: drop the oldest stamp, keep the newest PLATINUM_LIMITER_STAMPS. */
        for (i = 0; i < PLATINUM_LIMITER_STAMPS - 1; i++) {
            e->when[i] = e->when[i + 1];
        }
        e->when[PLATINUM_LIMITER_STAMPS - 1] = now;
    }
}
