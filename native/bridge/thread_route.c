#include "thread_route.h"

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

int
platinum_route_thread_view(void *ctx, platinum_thread_fn fn, const char *uri,
                           char *body, size_t cap)
{
    static platinum_thread_entry entries[PLATINUM_THREAD_POSTS_MAX];
    static platinum_timeline_post posts[PLATINUM_THREAD_POSTS_MAX];
    size_t count = 0;
    int depths[PLATINUM_THREAD_POSTS_MAX];
    int truncated = 0;
    size_t i;
    platinum_thread_result result;

    if (body == NULL || cap == 0 || fn == NULL) return 500;
    body[0] = '\0';

    if (platinum_valid_post_ref(uri, "x") != 0) {
        return write_error(body, cap, 400, "invalid_post_ref",
                           "uri must be an app.bsky.feed.post AT URI.");
    }

    result = fn(ctx, uri, entries, PLATINUM_THREAD_POSTS_MAX, &count, &truncated);
    if (result == PLATINUM_THREAD_NOT_FOUND) {
        return write_error(body, cap, 404, "post_not_found", "The post no longer exists.");
    }
    if (result != PLATINUM_THREAD_OK || count > PLATINUM_THREAD_POSTS_MAX) {
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }

    /* The writer takes a plain post array; entries interleave each post with its
     * depth, so copy the posts into a contiguous one first. */
    for (i = 0; i < count; i++) {
        posts[i] = entries[i].post;
        depths[i] = entries[i].depth;
    }
    if (platinum_write_thread_posts(posts, depths, count, truncated, body, cap) != 0) {
        body[0] = '\0';
        return write_error(body, cap, 500, "bridge_error",
                           "The backend could not complete the request.");
    }
    return 200;
}
