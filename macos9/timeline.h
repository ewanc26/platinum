#ifndef PLATINUM_TIMELINE_H
#define PLATINUM_TIMELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_TIMELINE_POSTS 5

typedef struct platinum_post_preview {
    char author[48];
    char handle[48];
    char time[24];
    char line1[112];
    char line2[112];
    char line3[112];
} platinum_post_preview;

const platinum_post_preview *platinum_timeline_posts(void);
unsigned short platinum_timeline_post_count(void);

#ifdef __cplusplus
}
#endif

#endif
