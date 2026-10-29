#include "image_route.h"

#include <stdlib.h>
#include <string.h>

#include "../plim/plim.h"

static int
is_alnum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int
is_did_id_char(char c)
{
    return is_alnum(c) || c == '.' || c == '_' || c == ':' || c == '%' || c == '-';
}

/* did:[a-z0-9]+:<id> with the same id characters as the bridge's DID rule. */
static size_t
scan_did(const char *s)
{
    size_t i = 0;
    if (strncmp(s, "did:", 4) != 0) return 0;
    i = 4;
    if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9'))) return 0;
    while ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9')) i++;
    if (s[i] != ':') return 0;
    i++;
    if (!is_did_id_char(s[i])) return 0;
    while (is_did_id_char(s[i])) i++;
    return i;
}

int
platinum_valid_image_ref(const char *ref)
{
    static const char *kinds[] = { "avatar_thumbnail/", "feed_thumbnail/", "feed_fullsize/", "avatar/" };
    size_t k;
    size_t i;
    size_t n;
    const char *p = NULL;

    if (ref == NULL || strlen(ref) > 300) return -1;
    for (k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
        size_t len = strlen(kinds[k]);
        if (strncmp(ref, kinds[k], len) == 0) {
            p = ref + len;
            break;
        }
    }
    if (p == NULL || strncmp(p, "plain/", 6) != 0) return -1;
    p += 6;

    i = scan_did(p);
    if (i == 0 || p[i] != '/') return -1;
    p += i + 1;

    /* The CID: one or more letters or digits, then @jpeg or @png, and nothing after. */
    n = 0;
    while (is_alnum(p[n])) n++;
    if (n == 0 || p[n] != '@') return -1;
    if (strcmp(p + n, "@jpeg") != 0 && strcmp(p + n, "@png") != 0) return -1;
    return 0;
}

int
platinum_cdn_ref(const char *url, char *out, size_t cap)
{
    static const char prefix[] = "https://cdn.bsky.app/img/";
    const char *ref;
    size_t n;

    if (out == NULL || cap == 0) return -1;
    out[0] = '\0';
    if (url == NULL) return -1;
    if (strncmp(url, prefix, sizeof prefix - 1) != 0) return -1;
    ref = url + sizeof prefix - 1;
    if (platinum_valid_image_ref(ref) != 0) return -1;
    n = strlen(ref);
    if (n >= cap) return -1;
    memcpy(out, ref, n + 1);
    return 0;
}

int
platinum_avatar_ref(const char *url, char *out, size_t cap)
{
    static const char prefix[] = "avatar/";
    static const char thumb[] = "avatar_thumbnail/";
    char ref[301];
    size_t n;

    if (out == NULL || cap == 0) return -1;
    out[0] = '\0';
    if (platinum_cdn_ref(url, ref, sizeof ref) != 0) return -1;
    if (strncmp(ref, prefix, sizeof prefix - 1) != 0) return -1;
    n = strlen(ref + sizeof prefix - 1);
    if (sizeof thumb - 1 + n >= cap) return -1;
    memcpy(out, thumb, sizeof thumb - 1);
    memcpy(out + sizeof thumb - 1, ref + sizeof prefix - 1, n + 1);
    return 0;
}

int
platinum_parse_image_params(const char *w, const char *depth, int *width, int *colours)
{
    long v = 320;
    size_t i;

    if (width == NULL || colours == NULL) return -1;
    if (w != NULL) {
        size_t n = strlen(w);
        if (n == 0 || n > 3) return -1;
        for (i = 0; i < n; i++) {
            if (w[i] < '0' || w[i] > '9') return -1;
        }
        v = 0;
        for (i = 0; i < n; i++) v = v * 10 + (w[i] - '0');
        if (v < 16 || v > 320) return -1;
    }
    *width = (int) v;

    if (depth == NULL || strcmp(depth, "4") == 0) {
        *colours = 16;
    } else if (strcmp(depth, "8") == 0) {
        *colours = 256;
    } else {
        return -1;
    }
    return 0;
}

int
platinum_route_image(void *ctx, platinum_image_source_fn fn, const char *ref,
                     const char *w, const char *depth,
                     unsigned char **out, long *out_len)
{
    platinum_rgba src;
    int width;
    int colours;
    platinum_image_result r;
    int rc;

    if (out == NULL || out_len == NULL || fn == NULL) return 500;
    *out = NULL;
    *out_len = 0;

    if (platinum_valid_image_ref(ref) != 0 || platinum_parse_image_params(w, depth, &width, &colours) != 0) {
        return 400;
    }

    memset(&src, 0, sizeof src);
    r = fn(ctx, ref, &src);
    if (r == PLATINUM_IMAGE_TOO_LARGE) {
        free(src.pixels);
        return 413;
    }
    if (r != PLATINUM_IMAGE_OK || src.pixels == NULL) {
        free(src.pixels);
        return 502;
    }

    /* The JS flattens onto white and shrinks to at most width x 320. */
    rc = plim_encode(src.pixels, src.width, src.height, width, 320, colours, out, out_len);
    free(src.pixels);
    if (rc == PLIM_ERR_TOO_LARGE) return 413;
    if (rc != PLIM_OK) return 502;
    return 200;
}
