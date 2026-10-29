#include "timeline_route.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search_routes.h"

void
platinum_timeline_free(platinum_timeline *out)
{
    if (out == NULL) return;
    free(out->posts);
    memset(out, 0, sizeof *out);
}

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

/* Appends s at *len. Returns 0, or -1 if it would not fit. */
static int
append_raw(char *body, size_t cap, size_t *len, const char *s)
{
    size_t n = strlen(s);
    if (*len + n >= cap) return -1;
    memcpy(body + *len, s, n);
    *len += n;
    body[*len] = '\0';
    return 0;
}

/*
 * Appends s as a JSON string. The bytes are copied unchanged, which is correct for the
 * UTF-8 the AppView sends; control characters are escaped because a raw one makes the
 * body invalid JSON. Returns 0, or -1 if it would not fit.
 */
static int
append_string(char *body, size_t cap, size_t *len, const char *s)
{
    const unsigned char *p;

    if (*len + 1 >= cap) return -1;
    body[(*len)++] = '"';
    for (p = (const unsigned char *) s; *p != '\0'; p++) {
        const char *piece = NULL;
        char unicode[7];

        switch (*p) {
        case '"':  piece = "\\\""; break;
        case '\\': piece = "\\\\"; break;
        case '\n': piece = "\\n"; break;
        case '\r': piece = "\\r"; break;
        case '\t': piece = "\\t"; break;
        default:
            if (*p < 0x20) {
                sprintf(unicode, "\\u%04x", (unsigned) *p);
                piece = unicode;
            }
            break;
        }
        if (piece != NULL) {
            if (append_raw(body, cap, len, piece) != 0) return -1;
        } else {
            if (*len + 1 >= cap) return -1;
            body[(*len)++] = (char) *p;
        }
    }
    if (*len + 1 >= cap) return -1;
    body[(*len)++] = '"';
    body[*len] = '\0';
    return 0;
}

static int
append_long(char *body, size_t cap, size_t *len, long value)
{
    char number[32];
    sprintf(number, "%ld", value);
    return append_raw(body, cap, len, number);
}

static int
append_key_string(char *body, size_t cap, size_t *len, const char *key, const char *value)
{
    if (append_raw(body, cap, len, ",\"") != 0) return -1;
    if (append_raw(body, cap, len, key) != 0) return -1;
    if (append_raw(body, cap, len, "\":") != 0) return -1;
    return append_string(body, cap, len, value);
}

static int
append_author(char *body, size_t cap, size_t *len, const platinum_timeline_author *a)
{
    if (append_raw(body, cap, len, "{\"did\":") != 0 ||
        append_string(body, cap, len, a->did) != 0) {
        return -1;
    }
    /* The optional fields are left out rather than sent empty, as api.ts does. */
    if (a->handle[0] != '\0' && append_key_string(body, cap, len, "handle", a->handle) != 0) {
        return -1;
    }
    if (a->display_name[0] != '\0' &&
        append_key_string(body, cap, len, "displayName", a->display_name) != 0) {
        return -1;
    }
    if (a->avatar[0] != '\0' && append_key_string(body, cap, len, "avatar", a->avatar) != 0) {
        return -1;
    }
    return append_raw(body, cap, len, "}");
}

static int
append_card(char *body, size_t cap, size_t *len, const platinum_timeline_card *card)
{
    if (append_raw(body, cap, len, ",\"card\":{\"uri\":") != 0 ||
        append_string(body, cap, len, card->uri) != 0 ||
        append_key_string(body, cap, len, "title", card->title) != 0 ||
        append_key_string(body, cap, len, "domain", card->domain) != 0) {
        return -1;
    }
    return append_raw(body, cap, len, "}");
}

static int
append_post(char *body, size_t cap, size_t *len, const platinum_timeline_post *p)
{
    int i;
    int images = 0;

    if (append_raw(body, cap, len, "{\"uri\":") != 0 ||
        append_string(body, cap, len, p->uri) != 0 ||
        append_key_string(body, cap, len, "cid", p->cid) != 0 ||
        append_raw(body, cap, len, ",\"author\":") != 0 ||
        append_author(body, cap, len, &p->author) != 0 ||
        append_key_string(body, cap, len, "text", p->text) != 0 ||
        append_key_string(body, cap, len, "createdAt", p->created_at) != 0) {
        return -1;
    }
    if (append_raw(body, cap, len, ",\"likeCount\":") != 0 ||
        append_long(body, cap, len, p->like_count) != 0 ||
        append_raw(body, cap, len, ",\"repostCount\":") != 0 ||
        append_long(body, cap, len, p->repost_count) != 0 ||
        append_raw(body, cap, len, ",\"replyCount\":") != 0 ||
        append_long(body, cap, len, p->reply_count) != 0 ||
        append_raw(body, cap, len, ",\"quoteCount\":") != 0 ||
        append_long(body, cap, len, p->quote_count) != 0 ||
        append_raw(body, cap, len, p->liked ? ",\"liked\":true" : ",\"liked\":false") != 0 ||
        append_raw(body, cap, len, p->reposted ? ",\"reposted\":true" : ",\"reposted\":false") != 0) {
        return -1;
    }

    if (p->has_images) {
        for (i = 0; i < PLATINUM_TIMELINE_IMAGES_MAX; i++) {
            if (p->images[i].ref[0] != '\0') images++;
        }
    }
    if (images > 0) {
        int first = 1;
        if (append_raw(body, cap, len, ",\"images\":[") != 0) return -1;
        for (i = 0; i < PLATINUM_TIMELINE_IMAGES_MAX; i++) {
            if (p->images[i].ref[0] == '\0') continue;
            if (!first && append_raw(body, cap, len, ",") != 0) return -1;
            first = 0;
            if (append_raw(body, cap, len, "{\"ref\":") != 0 ||
                append_string(body, cap, len, p->images[i].ref) != 0 ||
                append_key_string(body, cap, len, "alt", p->images[i].alt) != 0 ||
                append_raw(body, cap, len, "}") != 0) {
                return -1;
            }
        }
        if (append_raw(body, cap, len, "]") != 0) return -1;
    }

    if (p->has_card && append_card(body, cap, len, &p->card) != 0) return -1;
    return append_raw(body, cap, len, "}");
}

int
platinum_write_posts(const platinum_timeline_post *posts, size_t count,
                     const char *cursor, char *body, size_t cap)
{
    size_t len = 0;
    size_t i;

    if (body == NULL || cap == 0) return -1;
    body[0] = '\0';
    if (append_raw(body, cap, &len, "{\"posts\":[") != 0) return -1;
    for (i = 0; i < count; i++) {
        if (i > 0 && append_raw(body, cap, &len, ",") != 0) return -1;
        if (append_post(body, cap, &len, &posts[i]) != 0) return -1;
    }
    if (append_raw(body, cap, &len, "]") != 0) return -1;
    if (cursor != NULL && cursor[0] != '\0') {
        if (append_key_string(body, cap, &len, "cursor", cursor) != 0) return -1;
    }
    if (append_raw(body, cap, &len, "}") != 0) return -1;
    return 0;
}

int
platinum_route_timeline(void *ctx, platinum_timeline_fetch_fn fetch,
                        const char *limit_param, const char *cursor_param,
                        char *body, size_t cap)
{
    platinum_timeline page;
    int count;

    if (body == NULL || cap == 0 || fetch == NULL) return 500;
    body[0] = '\0';

    /* A cursor is passed through exactly; one that cannot fit the response field is
     * refused rather than truncated, since a cut cursor silently pages nowhere. */
    if (cursor_param != NULL && strlen(cursor_param) >= sizeof page.cursor) {
        return write_error(body, cap, 400, "invalid_cursor",
                           "cursor must be 255 bytes or fewer.");
    }

    count = platinum_query_limit(limit_param);
    memset(&page, 0, sizeof page);
    if (fetch(ctx, count, cursor_param, &page) != PLATINUM_TIMELINE_OK) {
        platinum_timeline_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }
    if (page.post_count > PLATINUM_TIMELINE_POSTS_MAX) {
        platinum_timeline_free(&page);
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }

    if (platinum_write_posts(page.posts, page.post_count,
                             page.has_cursor ? page.cursor : NULL, body, cap) != 0) {
        goto too_big;
    }

    platinum_timeline_free(&page);
    return 200;

too_big:
    platinum_timeline_free(&page);
    body[0] = '\0';
    return write_error(body, cap, 500, "bridge_error",
                       "The backend could not complete the request.");
}
