#ifndef PLATINUM_AVATAR_CACHE_H
#define PLATINUM_AVATAR_CACHE_H

#include "image_blob.h"
#include "post_media.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Avatars for the timeline, kept by reference. A post names its author's avatar
 * (`author.avatar`, a reference for GET /v1/image); the cache says whether that
 * reference has been fetched, parsed and is ready to draw, and holds the parsed
 * image until something else needs the slot.
 *
 * This module does no network and no Toolbox calls. The caller asks for a
 * reference with platinum_avatar_want, fetches it through the bridge client when
 * want says so, and hands the answer back with platinum_avatar_fulfil or
 * platinum_avatar_fail. That keeps the state machine testable on a host.
 *
 * Slots are bounded. A slot that is fetching is never evicted, so an answer that
 * is on its way always finds its slot; among the others the least recently used
 * goes first.
 */

#define PLATINUM_AVATAR_SLOTS 24
#define PLATINUM_AVATAR_WIDTH 32
#define PLATINUM_AVATAR_DEPTH 8

enum {
    PLATINUM_AVATAR_EMPTY = 0,
    PLATINUM_AVATAR_PENDING,  /* asked for; the answer has not come back */
    PLATINUM_AVATAR_READY,    /* parsed, and drawable */
    PLATINUM_AVATAR_FAILED    /* the fetch failed or the answer did not parse */
};

typedef struct platinum_avatar_slot {
    char ref[PLATINUM_MEDIA_REF_MAX];
    int state;
    unsigned long used;           /* the tick of the last use, for eviction */
    unsigned char *blob;          /* owned; `image` points into it when READY */
    platinum_image image;
} platinum_avatar_slot;

typedef struct platinum_avatar_cache {
    platinum_avatar_slot slots[PLATINUM_AVATAR_SLOTS];
    unsigned long tick;
} platinum_avatar_cache;

void platinum_avatar_cache_init(platinum_avatar_cache *cache);
void platinum_avatar_cache_dispose(platinum_avatar_cache *cache);

/* The parsed image for `ref` if it is ready, and NULL otherwise. Counts as a use. */
const platinum_image *platinum_avatar_lookup(platinum_avatar_cache *cache,
                                             const char *ref);

/*
 * Note that `ref` is wanted. Returns 1 when the caller should fetch it now: the
 * reference now has a pending slot. Returns 0 when the reference is already
 * known in any state, is empty or too long, or no slot can be given to it.
 */
int platinum_avatar_want(platinum_avatar_cache *cache, const char *ref);

/*
 * The fetch for `ref` answered. `blob` is a malloc'd copy of the answer, and the
 * cache owns it from here on, whatever the outcome. Returns PLATINUM_IMAGE_OK
 * when it parsed and the slot is ready; otherwise the parse error, with the slot
 * failed. Returns -1, freeing `blob`, when `ref` was not pending.
 */
int platinum_avatar_fulfil(platinum_avatar_cache *cache, const char *ref,
                           unsigned char *blob, long length);

/* The fetch for `ref` failed. The slot is marked failed, so it is not asked for again. */
void platinum_avatar_fail(platinum_avatar_cache *cache, const char *ref);

#ifdef __cplusplus
}
#endif

#endif
