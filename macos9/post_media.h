#ifndef PLATINUM_POST_MEDIA_H
#define PLATINUM_POST_MEDIA_H

#include "json_min.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * What a post carries besides its text: the author's avatar, up to four
 * pictures and a link card, as the bridge describes them (docs/BRIDGE_PROTOCOL.md).
 * The Mac keeps only references and short text. A reference is what
 * GET /v1/image takes; it is never a URL, and the Mac never fetches from an
 * image host. Alt text and the card title are MacRoman, clipped to what fits.
 */
#define PLATINUM_MEDIA_REF_MAX 192
#define PLATINUM_MEDIA_ALT_MAX 160
#define PLATINUM_MEDIA_IMAGES_MAX 4
#define PLATINUM_MEDIA_TITLE_MAX 101
#define PLATINUM_MEDIA_DOMAIN_MAX 64

typedef struct platinum_media_image {
    char ref[PLATINUM_MEDIA_REF_MAX];
    char alt[PLATINUM_MEDIA_ALT_MAX];
} platinum_media_image;

typedef struct platinum_post_media {
    char avatar[PLATINUM_MEDIA_REF_MAX];
    short image_count;
    platinum_media_image images[PLATINUM_MEDIA_IMAGES_MAX];
    char card_title[PLATINUM_MEDIA_TITLE_MAX];
    char card_domain[PLATINUM_MEDIA_DOMAIN_MAX];
} platinum_post_media;

/*
 * Fill `media` from a post object `item` and its `author` object. Absent fields
 * leave the matching parts empty: a post with none has image_count 0 and empty
 * strings. An image without a reference is skipped. Never fails: a post is
 * shown without pictures rather than not at all.
 */
void platinum_post_media_parse(platinum_post_media *media,
                               platinum_json author,
                               platinum_json item);

#ifdef __cplusplus
}
#endif

#endif
