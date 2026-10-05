#include "json_min.h"

#include <string.h>

/*
 * A bounded JSON scanner for the Classic Mac OS 9 side.
 *
 * Strict C89 throughout: every declaration is at the top of its block, there
 * are no `//` comments, nothing here allocates, and recursion is capped. The
 * bridge protocol only exchanges small objects with string and integer
 * members, so this is a scanner for exactly that rather than a general
 * parser: it never builds a tree.
 *
 * Two rules shape the whole file:
 *
 *   - A malformed document is rejected, never repaired. A truncated string, a
 *     raw control character inside a string, an unknown escape or an
 *     unpaired surrogate all fail, because accepting half of a token or DID is
 *     how a client ends up authenticating as the wrong thing.
 *
 *   - Every output is bounded by an explicit capacity, and running out of room
 *     is reported as WF_ERR_ALLOC. That is the status Wolfram's own Mac OS 9
 *     transport uses when one of its internal buffers overflows, so the Mac
 *     side keeps a single error system rather than adding its own.
 */

/* Longest member name platinum_json_* will compare. Bridge member names are
 * short; a longer one cannot match and is skipped rather than truncated. */
#define JSON_MAX_KEY 48

/* An integer wider than this cannot be represented, so it is rejected
 * instead of silently wrapping. */
#define JSON_MAX_INT_DIGITS 9

static int json_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char *json_skip_space(const char *p)
{
    while (json_is_space(*p))
        p++;
    return p;
}

/* Write one byte if it fits. Always keeps `dst` NUL-terminated. */
static int json_put(char *dst, size_t cap, size_t *used, char c)
{
    if (*used + 1 >= cap)
        return 0;
    dst[*used] = c;
    *used += 1;
    dst[*used] = '\0';
    return 1;
}

wf_status platinum_json_escape(char *dst, size_t cap, const char *src)
{
    static const char hex[] = "0123456789abcdef";
    size_t used;

    if (dst == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;

    dst[0] = '\0';
    if (src == NULL)
        return WF_OK;

    used = 0;
    while (*src != '\0') {
        unsigned char c = (unsigned char)*src;

        src++;

        if (c == '"' || c == '\\') {
            if (!json_put(dst, cap, &used, '\\') ||
                !json_put(dst, cap, &used, (char)c))
                return WF_ERR_ALLOC;
        } else if (c == '\b') {
            if (!json_put(dst, cap, &used, '\\') ||
                !json_put(dst, cap, &used, 'b'))
                return WF_ERR_ALLOC;
        } else if (c == '\f') {
            if (!json_put(dst, cap, &used, '\\') ||
                !json_put(dst, cap, &used, 'f'))
                return WF_ERR_ALLOC;
        } else if (c == '\n') {
            if (!json_put(dst, cap, &used, '\\') ||
                !json_put(dst, cap, &used, 'n'))
                return WF_ERR_ALLOC;
        } else if (c == '\r') {
            if (!json_put(dst, cap, &used, '\\') ||
                !json_put(dst, cap, &used, 'r'))
                return WF_ERR_ALLOC;
        } else if (c == '\t') {
            if (!json_put(dst, cap, &used, '\\') ||
                !json_put(dst, cap, &used, 't'))
                return WF_ERR_ALLOC;
        } else if (c < 0x20) {
            /* No shorthand exists, so emit \u00XX. The bridge never sends one;
             * text typed by a user can contain one. */
            if (!json_put(dst, cap, &used, '\\') ||
                !json_put(dst, cap, &used, 'u') ||
                !json_put(dst, cap, &used, '0') ||
                !json_put(dst, cap, &used, '0') ||
                !json_put(dst, cap, &used, hex[(c >> 4) & 0x0f]) ||
                !json_put(dst, cap, &used, hex[c & 0x0f]))
                return WF_ERR_ALLOC;
        } else {
            if (!json_put(dst, cap, &used, (char)c))
                return WF_ERR_ALLOC;
        }
    }

    return WF_OK;
}

/* Read four hex digits as a code point. Returns 0 on success, -1 otherwise. */
static int json_read_hex4(const char *p, unsigned int *out)
{
    unsigned int value;
    int i;

    value = 0;
    for (i = 0; i < 4; i++) {
        char c = p[i];
        unsigned int digit;

        if (c >= '0' && c <= '9')
            digit = (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (unsigned int)(c - 'a') + 10u;
        else if (c >= 'A' && c <= 'F')
            digit = (unsigned int)(c - 'A') + 10u;
        else
            return -1;

        value = (value << 4) | digit;
    }

    *out = value;
    return 0;
}

/* Emit one code point as UTF-8. Returns 0 on success, -1 if it does not fit. */
static int json_put_utf8(char *dst, size_t cap, size_t *used, unsigned int cp)
{
    if (cp < 0x80u)
        return json_put(dst, cap, used, (char)cp) ? 0 : -1;

    if (cp < 0x800u) {
        if (!json_put(dst, cap, used, (char)(0xc0u | (cp >> 6))))
            return -1;
        return json_put(dst, cap, used, (char)(0x80u | (cp & 0x3fu))) ? 0 : -1;
    }

    if (cp < 0x10000u) {
        if (!json_put(dst, cap, used, (char)(0xe0u | (cp >> 12))))
            return -1;
        if (!json_put(dst, cap, used, (char)(0x80u | ((cp >> 6) & 0x3fu))))
            return -1;
        return json_put(dst, cap, used, (char)(0x80u | (cp & 0x3fu))) ? 0 : -1;
    }

    if (!json_put(dst, cap, used, (char)(0xf0u | (cp >> 18))))
        return -1;
    if (!json_put(dst, cap, used, (char)(0x80u | ((cp >> 12) & 0x3fu))))
        return -1;
    if (!json_put(dst, cap, used, (char)(0x80u | ((cp >> 6) & 0x3fu))))
        return -1;
    return json_put(dst, cap, used, (char)(0x80u | (cp & 0x3fu))) ? 0 : -1;
}

/*
 * Decode the JSON string at `p` (which must be the opening quote).
 *
 * When `dst` is NULL nothing is copied and the string is only validated, which
 * is how members that are skipped stay cheap.
 *
 * Returns WF_OK, WF_ERR_PARSE for a malformed string, or WF_ERR_ALLOC when a
 * real destination could not hold the value. On success `*out_next`, when
 * given, points just past the closing quote.
 */
static wf_status json_decode_string(const char *p,
                                    char *dst,
                                    size_t cap,
                                    const char **out_next)
{
    size_t used;

    if (dst != NULL && cap != 0)
        dst[0] = '\0';

    if (*p != '"')
        return WF_ERR_PARSE;
    p++;
    used = 0;

    while (*p != '"') {
        unsigned char c = (unsigned char)*p;

        if (c == '\0')
            return WF_ERR_PARSE;

        if (c == '\\') {
            char esc = p[1];

            p += 2;
            switch (esc) {
            case '"':
            case '\\':
            case '/':
                if (dst != NULL && !json_put(dst, cap, &used, esc))
                    return WF_ERR_ALLOC;
                break;
            case 'b':
                if (dst != NULL && !json_put(dst, cap, &used, '\b'))
                    return WF_ERR_ALLOC;
                break;
            case 'f':
                if (dst != NULL && !json_put(dst, cap, &used, '\f'))
                    return WF_ERR_ALLOC;
                break;
            case 'n':
                if (dst != NULL && !json_put(dst, cap, &used, '\n'))
                    return WF_ERR_ALLOC;
                break;
            case 'r':
                if (dst != NULL && !json_put(dst, cap, &used, '\r'))
                    return WF_ERR_ALLOC;
                break;
            case 't':
                if (dst != NULL && !json_put(dst, cap, &used, '\t'))
                    return WF_ERR_ALLOC;
                break;
            case 'u': {
                unsigned int cp;

                if (json_read_hex4(p, &cp) != 0)
                    return WF_ERR_PARSE;
                p += 4;

                /* A high surrogate is only valid when its low half follows. */
                if (cp >= 0xd800u && cp <= 0xdbffu) {
                    unsigned int low;

                    if (p[0] != '\\' || p[1] != 'u')
                        return WF_ERR_PARSE;
                    if (json_read_hex4(p + 2, &low) != 0)
                        return WF_ERR_PARSE;
                    if (low < 0xdc00u || low > 0xdfffu)
                        return WF_ERR_PARSE;
                    p += 6;
                    cp = 0x10000u + ((cp - 0xd800u) << 10) + (low - 0xdc00u);
                } else if (cp >= 0xdc00u && cp <= 0xdfffu) {
                    return WF_ERR_PARSE;
                }

                if (dst != NULL && json_put_utf8(dst, cap, &used, cp) != 0)
                    return WF_ERR_ALLOC;
                break;
            }
            default:
                return WF_ERR_PARSE;
            }
            continue;
        }

        /* Control characters must be escaped inside a JSON string. */
        if (c < 0x20)
            return WF_ERR_PARSE;

        if (dst != NULL && !json_put(dst, cap, &used, (char)c))
            return WF_ERR_ALLOC;
        p++;
    }

    p++;
    if (dst != NULL && cap != 0)
        dst[used] = '\0';
    if (out_next != NULL)
        *out_next = p;
    return WF_OK;
}

/* Advance past a JSON number, or return NULL if it is not one. */
static const char *json_skip_number(const char *p)
{
    const char *start = p;

    if (*p == '-')
        p++;
    if (*p == '0') {
        p++;
    } else if (*p >= '1' && *p <= '9') {
        while (*p >= '0' && *p <= '9')
            p++;
    } else {
        return NULL;
    }

    if (*p == '.') {
        p++;
        if (*p < '0' || *p > '9')
            return NULL;
        while (*p >= '0' && *p <= '9')
            p++;
    }

    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (*p < '0' || *p > '9')
            return NULL;
        while (*p >= '0' && *p <= '9')
            p++;
    }

    return (p == start) ? NULL : p;
}

/* Advance past "true", "false" or "null", or return NULL. */
static const char *json_skip_literal(const char *p)
{
    if (strncmp(p, "true", 4) == 0)
        return p + 4;
    if (strncmp(p, "false", 5) == 0)
        return p + 5;
    if (strncmp(p, "null", 4) == 0)
        return p + 4;
    return NULL;
}

/*
 * Advance past any JSON value. Returns NULL if the value is malformed or
 * nested deeper than PLATINUM_JSON_MAX_DEPTH, which bounds the recursion so a
 * hostile body cannot exhaust a 1990s stack.
 */
static const char *json_skip_value(const char *p, int depth)
{
    if (depth > PLATINUM_JSON_MAX_DEPTH)
        return NULL;

    if (*p == '"') {
        const char *next = NULL;

        if (json_decode_string(p, NULL, 0, &next) != WF_OK)
            return NULL;
        return next;
    }

    if (*p == '{' || *p == '[') {
        char close = (*p == '{') ? '}' : ']';

        p = json_skip_space(p + 1);
        if (*p == close)
            return p + 1;

        for (;;) {
            if (close == '}') {
                const char *next = NULL;

                if (*p != '"')
                    return NULL;
                if (json_decode_string(p, NULL, 0, &next) != WF_OK)
                    return NULL;
                p = json_skip_space(next);
                if (*p != ':')
                    return NULL;
                p++;
            }

            p = json_skip_space(p);
            p = json_skip_value(p, depth + 1);
            if (p == NULL)
                return NULL;
            p = json_skip_space(p);

            if (*p == ',') {
                p = json_skip_space(p + 1);
                continue;
            }
            if (*p == close)
                return p + 1;
            return NULL;
        }
    }

    if (*p == '-' || (*p >= '0' && *p <= '9'))
        return json_skip_number(p);

    return json_skip_literal(p);
}

/*
 * Find the top-level member `name` in the object `body` and return a pointer
 * to its first character.
 *
 * Only top-level members are searched: this is not a path lookup, and the
 * bridge protocol does not need one. Members that appear before the wanted one
 * are skipped without being interpreted, so an unrelated member of any type
 * cannot make the lookup fail.
 *
 * Returns WF_OK, WF_ERR_NOT_FOUND, WF_ERR_PARSE or WF_ERR_INVALID_ARG.
 */
static wf_status json_find_member(const char *body,
                                  const char *name,
                                  const char **out_value)
{
    size_t name_len;
    const char *p;

    if (body == NULL || name == NULL || out_value == NULL)
        return WF_ERR_INVALID_ARG;

    name_len = strlen(name);
    p = json_skip_space(body);
    if (*p != '{')
        return WF_ERR_PARSE;
    p = json_skip_space(p + 1);
    if (*p == '}')
        return WF_ERR_NOT_FOUND;

    for (;;) {
        char key[JSON_MAX_KEY];
        const char *next = NULL;
        wf_status status;
        int is_match;

        if (*p != '"')
            return WF_ERR_PARSE;

        status = json_decode_string(p, key, sizeof(key), &next);
        if (status == WF_ERR_ALLOC) {
            /* The key is longer than any name this client looks up, so it
             * cannot be the member being searched for. Skip it whole rather
             * than compare a truncated prefix, which could match by
             * accident. */
            if (json_decode_string(p, NULL, 0, &next) != WF_OK)
                return WF_ERR_PARSE;
            is_match = 0;
        } else if (status != WF_OK) {
            return status;
        } else {
            is_match = (strlen(key) == name_len) &&
                       (memcmp(key, name, name_len) == 0);
        }

        p = json_skip_space(next);
        if (*p != ':')
            return WF_ERR_PARSE;
        p = json_skip_space(p + 1);

        if (is_match) {
            *out_value = p;
            return WF_OK;
        }

        p = json_skip_value(p, 1);
        if (p == NULL)
            return WF_ERR_PARSE;
        p = json_skip_space(p);

        if (*p == ',') {
            p = json_skip_space(p + 1);
            continue;
        }
        if (*p == '}')
            return WF_ERR_NOT_FOUND;
        return WF_ERR_PARSE;
    }
}

wf_status platinum_json_get_string(const char *body,
                                   const char *name,
                                   char *dst,
                                   size_t cap)
{
    const char *value = NULL;
    wf_status status;

    if (dst == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;

    dst[0] = '\0';

    /* The whole document is validated first, not just up to the member being
     * read. A response that is well-formed where the wanted member sits but
     * truncated or corrupt after it is still a broken response, and returning
     * a value from it would mean acting on a body the client never fully
     * understood. */
    status = platinum_json_valid(body);
    if (status != WF_OK)
        return status;

    status = json_find_member(body, name, &value);
    if (status != WF_OK)
        return status;

    if (*value != '"')
        return WF_ERR_PARSE;

    /* WF_OK, or WF_ERR_ALLOC if the value does not fit `cap`. The document is
     * already known to be well formed, so a parse error here is not expected,
     * but it is passed through rather than mapped onto something else. */
    return json_decode_string(value, dst, cap, NULL);
}

wf_status platinum_json_get_int(const char *body, const char *name, long *out)
{
    const char *value = NULL;
    wf_status status;
    const char *p;
    long parsed;
    int negative;
    int digits;

    if (out == NULL)
        return WF_ERR_INVALID_ARG;

    *out = 0;

    /* Validated for the same reason as platinum_json_get_string: a body that
     * only parses as far as the member being read is a broken body. */
    status = platinum_json_valid(body);
    if (status != WF_OK)
        return status;

    status = json_find_member(body, name, &value);
    if (status != WF_OK)
        return status;

    p = value;
    negative = 0;
    if (*p == '-') {
        negative = 1;
        p++;
    }
    if (*p < '0' || *p > '9')
        return WF_ERR_PARSE;

    parsed = 0;
    digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (digits >= JSON_MAX_INT_DIGITS)
            return WF_ERR_PARSE;
        parsed = parsed * 10 + (long)(*p - '0');
        digits++;
        p++;
    }

    /* JSON has a single number type. A fractional or exponential value is not
     * an integer, and quietly truncating one would turn a limit or a count
     * into a different number. */
    if (*p == '.' || *p == 'e' || *p == 'E')
        return WF_ERR_PARSE;

    *out = negative ? -parsed : parsed;
    return WF_OK;
}

wf_status platinum_json_valid(const char *body)
{
    const char *p;

    if (body == NULL)
        return WF_ERR_INVALID_ARG;

    p = json_skip_space(body);
    p = json_skip_value(p, 1);
    if (p == NULL)
        return WF_ERR_PARSE;

    /* Exactly one document: trailing content is malformed, not ignorable. */
    p = json_skip_space(p);
    if (*p != '\0')
        return WF_ERR_PARSE;

    return WF_OK;
}
