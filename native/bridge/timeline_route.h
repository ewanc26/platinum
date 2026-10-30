/*
 * GET /v1/timeline (Part 8, the home feed slice).
 *
 * The status and body decisions of server.ts for the home timeline: the limit is
 * 1 to 50 (default 20), the cursor is passed through, and the response is the
 * bridge's timeline contract — posts with author, text, counts, the viewer's
 * liked/reposted flags, and any images or link card. Muted words are applied
 * before the response is built, so a page can come back short while the cursor
 * still moves on.
 *
 * The fetch is a callback, so this file needs neither the network nor Wolfram.
 * The Wolfram-backed fetch lives in timeline_wolfram.c, which uses the typed
 * feed parser and is built with -std=c2x.
 */
#ifndef PLATINUM_NATIVE_TIMELINE_ROUTE_H
#define PLATINUM_NATIVE_TIMELINE_ROUTE_H

#include <stddef.h>

#define PLATINUM_TIMELINE_LIMIT_MIN 1
#define PLATINUM_TIMELINE_LIMIT_MAX 50
#define PLATINUM_TIMELINE_LIMIT_DEFAULT 20
/* One page holds at most the largest limit the route accepts. */
#define PLATINUM_TIMELINE_POSTS_MAX 50
#define PLATINUM_TIMELINE_IMAGES_MAX 4

/* One post as the bridge serves it. Every field is present; optional author
 * fields are empty strings when the source had none. */
typedef struct platinum_timeline_author {
    char did[256];
    char handle[256];
    char display_name[65];
    char avatar[301];
} platinum_timeline_author;

typedef struct platinum_timeline_image {
    char ref[301];
    char alt[301];
} platinum_timeline_image;

typedef struct platinum_timeline_card {
    char uri[301];
    char title[101];
    char domain[101];
} platinum_timeline_card;

typedef struct platinum_timeline_post {
    char uri[513];
    char cid[256];
    platinum_timeline_author author;
    char text[301];
    char created_at[65];
    long like_count;
    long repost_count;
    long reply_count;
    long quote_count;
    int liked;
    int reposted;
    int has_images;
    platinum_timeline_image images[4];
    int has_card;
    platinum_timeline_card card;
} platinum_timeline_post;

typedef struct platinum_timeline {
    platinum_timeline_post *posts;   /* malloc'd, post_count entries; may be NULL */
    size_t post_count;
    char cursor[256];
    int has_cursor;
} platinum_timeline;

typedef enum platinum_timeline_result {
    PLATINUM_TIMELINE_OK = 0,
    PLATINUM_TIMELINE_ERROR
} platinum_timeline_result;

/* Fetches one page. limit is 1 to 50 and cursor is the previous page's cursor, or
 * NULL for the first page. Fills out and returns a result; the caller frees with
 * platinum_timeline_free. */
typedef platinum_timeline_result (*platinum_timeline_fetch_fn)(void *ctx, int limit,
                                                                const char *cursor,
                                                                platinum_timeline *out);

/* Releases the posts and resets the page. NULL-safe. */
void platinum_timeline_free(platinum_timeline *out);

/* Writes the timeline shape — {"posts":[...],"cursor"} — from an already built
 * page. cursor may be NULL. Returns 0, or -1 when the body does not fit `cap`;
 * `body` is then empty. Shared with the author feed, which has the same shape. */
int platinum_write_posts(const platinum_timeline_post *posts, size_t count,
                         const char *cursor, char *body, size_t cap);

/* Writes the thread shape — {"posts":[...],"truncated":...}, each post with
 * its `depth` — from an already flattened list. depths has count entries.
 * Returns 0, or -1 when the body does not fit; `body` is then empty. */
int platinum_write_thread_posts(const platinum_timeline_post *posts, const int *depths,
                                size_t count, int truncated, char *body, size_t cap);

/* GET /v1/timeline. limit_param and cursor_param are the raw query values, or
 * NULL when absent. Writes the JSON body and returns the HTTP status. */
int platinum_route_timeline(void *ctx, platinum_timeline_fetch_fn fetch,
                           const char *limit_param, const char *cursor_param,
                           char *body, size_t cap);

#endif