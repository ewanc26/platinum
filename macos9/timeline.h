#ifndef PLATINUM_TIMELINE_H
#define PLATINUM_TIMELINE_H

#include "bridge_client.h"
#include "json_min.h"
#include "post_media.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Rows kept in memory, and rows asked for per request. Loading an older page
 * past the cap drops the newest rows from the front, so memory stays bounded at
 * MAX_POSTS previews (about 130KB with their media references) however far back the reader goes. */
#define PLATINUM_TIMELINE_MAX_POSTS 40
#define PLATINUM_TIMELINE_PAGE 20
#define PLATINUM_TIMELINE_AUTHOR_MAX 64
#define PLATINUM_TIMELINE_HANDLE_MAX 64
#define PLATINUM_TIMELINE_TIME_MAX 32
#define PLATINUM_TIMELINE_LINE_MAX 112
#define PLATINUM_TIMELINE_CURSOR_MAX 255
#define PLATINUM_TIMELINE_STATUS_MAX 127

typedef struct platinum_post_preview {
    char uri[513];
    char cid[256];
    char author[PLATINUM_TIMELINE_AUTHOR_MAX];
    char handle[PLATINUM_TIMELINE_HANDLE_MAX];
    char time[PLATINUM_TIMELINE_TIME_MAX];
    char line1[PLATINUM_TIMELINE_LINE_MAX];
    char line2[PLATINUM_TIMELINE_LINE_MAX];
    char line3[PLATINUM_TIMELINE_LINE_MAX];
    long like_count;
    long repost_count;
    long reply_count;
    long quote_count;
    int liked;
    int reposted;
    /* Avatar, pictures and link card references, for the windows that draw
     * them. Empty for a post that has none. */
    platinum_post_media media;
} platinum_post_preview;

typedef struct platinum_timeline {
    platinum_post_preview posts[PLATINUM_TIMELINE_MAX_POSTS];
    unsigned short count;
    char cursor[PLATINUM_TIMELINE_CURSOR_MAX + 1];
    char status[PLATINUM_TIMELINE_STATUS_MAX + 1];
    int loading;
} platinum_timeline;

/* Read one bridge post object (the timeline and thread item shape) into a
 * preview. Returns 0 for a post missing uri, cid or author.did. */
int platinum_timeline_parse_post(platinum_post_preview *post,
                                 platinum_json item);

void platinum_timeline_init(platinum_timeline *timeline);
wf_status platinum_timeline_refresh(platinum_timeline *timeline,
                                    platinum_bridge_client *bridge);
/*
 * Fetch the page after the current cursor and append it. On success `dropped`
 * is how many rows were removed from the front to stay within the cap, so the
 * caller can move its scroll position and selection by the same amount. On
 * failure the rows already loaded are kept and only the status changes.
 * Returns WF_ERR_INVALID_ARG when there is no older page.
 */
wf_status platinum_timeline_load_older(platinum_timeline *timeline,
                                       platinum_bridge_client *bridge,
                                       unsigned short *dropped);
int platinum_timeline_has_older(const platinum_timeline *timeline);

/*
 * Set the signed-in account's like (repost = 0) or repost (repost = 1) of the
 * post at `index` to `on`, through POST /v1/like or /v1/repost, and update the
 * row from the bridge's answer. The bridge is idempotent, so sending the state
 * the row already shows is harmless. On failure the row is left unchanged and
 * the status says why.
 */
wf_status platinum_timeline_set_engagement(platinum_timeline *timeline,
                                           platinum_bridge_client *bridge,
                                           unsigned short index,
                                           int repost,
                                           int on);
const platinum_post_preview *platinum_timeline_posts(
    const platinum_timeline *timeline);
unsigned short platinum_timeline_post_count(
    const platinum_timeline *timeline);
const char *platinum_timeline_status(const platinum_timeline *timeline);

#ifdef __cplusplus
}
#endif

#endif
