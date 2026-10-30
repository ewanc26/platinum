#include "post_routes.h"

#include <stdio.h>
#include <string.h>

#include "validate.h"

static int
write_error(char *body, size_t cap, int status, const char *code, const char *message)
{
    if (strlen(code) + strlen(message) + 32 >= cap) {
        body[0] = '\0';
        return 500;
    }
    strcpy(body, "{\"error\":\"");
    strcat(body, code);
    strcat(body, "\",\"message\":\"");
    strcat(body, message);
    strcat(body, "\"}");
    return status;
}

long
platinum_utf16_length(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    long n = 0;

    if (s == NULL) return -1;
    while (*p) {
        unsigned char c = *p;
        if (c < 0x80) {
            n += 1; p += 1;
        } else if ((c & 0xE0) == 0xC0) {
            n += 1; p += 2;
        } else if ((c & 0xF0) == 0xE0) {
            n += 1; p += 3;
        } else if ((c & 0xF8) == 0xF0) {
            n += 2; p += 4;   /* a surrogate pair in UTF-16 */
        } else {
            n += 1; p += 1;   /* stray continuation byte: count it once */
        }
    }
    return n;
}

/* The code points JavaScript's String.prototype.trim() removes, as checked by
 * node -e (U+0009-000D, U+0020, U+00A0, U+1680, U+2000-200A, U+2028, U+2029,
 * U+202F, U+205F, U+3000, U+FEFF). */
static int
is_trim_space(unsigned long cp)
{
    if (cp == 0x20 || cp == 0xA0 || cp == 0x1680 || cp == 0x2028 || cp == 0x2029 ||
        cp == 0x202F || cp == 0x205F || cp == 0x3000 || cp == 0xFEFF) {
        return 1;
    }
    if (cp >= 0x09 && cp <= 0x0D) return 1;
    return cp >= 0x2000 && cp <= 0x200A;
}

/* True when s holds nothing but what trim() removes. */
static int
is_blank_text(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;

    while (*p) {
        unsigned long cp;
        size_t len;
        if (*p < 0x80) {
            cp = *p; len = 1;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = ((unsigned long)(*p & 0x1F) << 6) | (p[1] & 0x3F); len = 2;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = ((unsigned long)(*p & 0x0F) << 12) | ((unsigned long)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); len = 3;
        } else {
            cp = ((unsigned long)(*p & 0x07) << 18) | ((unsigned long)(p[1] & 0x3F) << 12) |
                 ((unsigned long)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); len = 4;
        }
        if (!is_trim_space(cp)) return 0;
        p += len;
    }
    return 1;
}

static int
gate_ok(const char *g)
{
    return strcmp(g, "everyone") == 0 || strcmp(g, "nobody") == 0 || strcmp(g, "mentioned") == 0 ||
           strcmp(g, "following") == 0 || strcmp(g, "followers") == 0;
}

static int
ref_ok(int is_object, const platinum_ref *r)
{
    if (!is_object || r->uri == NULL || r->cid == NULL) return 0;
    return platinum_valid_post_ref(r->uri, r->cid) == 0;
}

int
platinum_route_post(void *ctx, platinum_publish_fn fn, const platinum_post_request *req,
                    char *body, size_t cap)
{
    platinum_post_result r;
    const char *gate = NULL;
    const platinum_ref *reply = NULL;
    const platinum_ref *quote = NULL;

    if (body == NULL || cap == 0 || fn == NULL || req == NULL) return 500;
    body[0] = '\0';

    /* The TypeScript tests trim(), so a text of only spaces counts as missing. */
    if (req->text == NULL || is_blank_text(req->text)) {
        return write_error(body, cap, 400, "missing_text", "Post text is required.");
    }
    if (platinum_utf16_length(req->text) > PLATINUM_POST_TEXT_MAX) {
        return write_error(body, cap, 400, "text_too_long", "Post text must not exceed 300 characters.");
    }
    if (req->has_reply && !ref_ok(req->reply_is_object, &req->reply)) {
        return write_error(body, cap, 400, "invalid_post_ref", "replyTo must name a post by uri and cid.");
    }
    if (req->has_reply) reply = &req->reply;
    if (req->has_quote && !ref_ok(req->quote_is_object, &req->quote)) {
        return write_error(body, cap, 400, "invalid_post_ref", "quote must name a post by uri and cid.");
    }
    if (req->has_quote) quote = &req->quote;

    if (req->has_gate) {
        if (req->gate == NULL || !gate_ok(req->gate) || (req->has_reply && strcmp(req->gate, "everyone") != 0)) {
            return write_error(body, cap, 400, "invalid_reply_gate",
                "replyGate is everyone, nobody, mentioned, following or followers, and only on a post that is not itself a reply.");
        }
        gate = req->gate;
    }

    r = fn(ctx, req->text, reply, quote, gate, body, cap);
    if (r == PLATINUM_POST_NOT_FOUND) {
        body[0] = '\0';
        return write_error(body, cap, 404, "post_not_found", "The post being replied to or quoted no longer exists.");
    }
    if (r != PLATINUM_POST_OK) {
        body[0] = '\0';
        return write_error(body, cap, 500, "bridge_error", "The backend could not complete the request.");
    }
    return 200;
}
