#include "profile_wolfram.h"

#include <string.h>

#define PROFILE_DESCRIPTION_MAX_CP 511

long
platinum_utf8_clip_codepoints(const char *src, long max_cp, char *dst, size_t cap)
{
    const unsigned char *p = (const unsigned char *)src;
    size_t out = 0;
    long cp = 0;

    if (src == NULL || dst == NULL || cap == 0) return -1;
    while (p[out] != '\0' && cp < max_cp) {
        /* A code point is one lead byte plus its continuation bytes (10xxxxxx). */
        size_t len = 1;
        size_t k;
        while (p[out + len] != '\0' && (p[out + len] & 0xC0) == 0x80) len++;
        if (out + len + 1 > cap) return -1;
        for (k = 0; k < len; k++) dst[out + k] = (char)p[out + k];
        out += len;
        cp++;
    }
    dst[out] = '\0';
    return (long)out;
}

static int
copy_bounded(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n >= cap) return -1;
    memcpy(dst, src, n + 1);
    return 0;
}

int
platinum_profile_from_wolfram(const wf_agent_profile *src, int mine, platinum_profile *out)
{
    if (src == NULL || out == NULL || src->did == NULL) return -1;

    memset(out, 0, sizeof *out);
    out->following = -1;
    out->followed_by = -1;
    out->muted = -1;
    out->blocking = -1;

    if (copy_bounded(out->did, sizeof out->did, src->did) != 0) return -1;

    if (src->handle != NULL) {
        if (copy_bounded(out->handle, sizeof out->handle, src->handle) != 0) return -1;
        out->has_handle = 1;
    }
    if (src->display_name != NULL) {
        if (copy_bounded(out->display_name, sizeof out->display_name, src->display_name) != 0) return -1;
        out->has_display_name = 1;
    }
    if (src->description != NULL) {
        long n = platinum_utf8_clip_codepoints(src->description, PROFILE_DESCRIPTION_MAX_CP,
                                               out->description, sizeof out->description);
        if (n < 0) return -1;
        out->has_description = 1;
    }

    /* Wolfram always carries the counts; the TypeScript omits them when the
     * server did not send them. Treated as present, which is the usual case. */
    out->has_followers_count = 1;
    out->followers_count = src->followers_count;
    out->has_follows_count = 1;
    out->follows_count = src->follows_count;
    out->has_posts_count = 1;
    out->posts_count = src->posts_count;

    if (!mine) {
        out->following = src->following != NULL;
        out->muted = src->muted ? 1 : 0;
        out->blocking = src->blocking != NULL;
    }
    return 0;
}
