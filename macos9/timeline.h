#ifndef PLATINUM_TIMELINE_H
#define PLATINUM_TIMELINE_H

#include "bridge_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_TIMELINE_MAX_POSTS 20
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
} platinum_post_preview;

typedef struct platinum_timeline {
    platinum_post_preview posts[PLATINUM_TIMELINE_MAX_POSTS];
    unsigned short count;
    char cursor[PLATINUM_TIMELINE_CURSOR_MAX + 1];
    char status[PLATINUM_TIMELINE_STATUS_MAX + 1];
    int loading;
} platinum_timeline;

void platinum_timeline_init(platinum_timeline *timeline);
wf_status platinum_timeline_refresh(platinum_timeline *timeline,
                                    platinum_bridge_client *bridge);
const platinum_post_preview *platinum_timeline_posts(
    const platinum_timeline *timeline);
unsigned short platinum_timeline_post_count(
    const platinum_timeline *timeline);
const char *platinum_timeline_status(const platinum_timeline *timeline);

#ifdef __cplusplus
}
#endif

#endif
