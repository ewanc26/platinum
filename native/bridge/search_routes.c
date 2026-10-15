#include "search_routes.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "validate.h"

static int
clamp_count(double v)
{
    if (v != v) return 20; /* NaN: not a count */
    if (v > 50) return 50;
    if (v < 1) return 1;
    return (int) v;
}

int
platinum_query_limit(const char *value)
{
    char buf[64];
    const char *p;
    char *end;
    size_t n;
    double v;

    if (value == NULL) return 20;
    n = strlen(value);
    if (n >= sizeof buf) return 50; /* far too long to be a small count: clamp as a large number */
    memcpy(buf, value, n + 1);

    /* Trim as Number() does: leading and trailing whitespace is ignored. */
    p = buf;
    while (isspace((unsigned char)*p)) p++;
    end = (char *)p + strlen(p);
    while (end > p && isspace((unsigned char)end[-1])) end--;
    *end = '\0';

    if (*p == '\0') return 1; /* Number("") is 0, which clamps to 1 */

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        char *e;
        unsigned long hex = strtoul(p + 2, &e, 16);
        if (*e != '\0' || p[2] == '\0') return 20;
        return clamp_count((double) hex);
    }

    /* Only the characters of a plain decimal number are allowed; strtod would also take
     * "inf" and "nan", which Number() does not. */
    {
        const char *q;
        for (q = p; *q; q++) {
            if (!(isdigit((unsigned char)*q) || *q == '+' || *q == '-' || *q == '.' ||
                  *q == 'e' || *q == 'E')) {
                return 20;
            }
        }
    }
    v = strtod(p, &end);
    if (*end != '\0') return 20;
    return clamp_count(v);
}

int
platinum_route_search(void *ctx, platinum_search_fn fn, int posts, const char *q,
                      const char *limit, const char *cursor, char *body, size_t cap)
{
    char query[128];
    int count;
    platinum_search_result r;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (platinum_valid_query(q, query, sizeof query) != 0) {
        /* Same message as server.ts. */
        if (cap > 80) {
            strcpy(body, "{\"error\":\"invalid_query\",\"message\":\"q must be 1 to 100 characters.\"}");
        }
        return 400;
    }

    count = platinum_query_limit(limit);
    r = fn(ctx, posts, query, count, cursor, body, cap);
    if (r != PLATINUM_SEARCH_OK) {
        body[0] = '\0';
        if (cap > 96) {
            strcpy(body, "{\"error\":\"bridge_error\",\"message\":\"The backend could not complete the request.\"}");
        }
        return 500;
    }
    return 200;
}
