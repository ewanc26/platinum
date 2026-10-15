#include "avatar_cache.h"

#include <stdlib.h>
#include <string.h>

static void release_slot(platinum_avatar_slot *slot)
{
    free(slot->blob);
    slot->blob = NULL;
    memset(&slot->image, 0, sizeof(slot->image));
    slot->ref[0] = '\0';
    slot->state = PLATINUM_AVATAR_EMPTY;
    slot->used = 0;
}

static int usable_ref(const char *ref)
{
    return ref != NULL && ref[0] != '\0' && strlen(ref) < PLATINUM_MEDIA_REF_MAX;
}

static platinum_avatar_slot *find_slot(platinum_avatar_cache *cache, const char *ref)
{
    int i;

    for (i = 0; i < PLATINUM_AVATAR_SLOTS; i++) {
        platinum_avatar_slot *slot = &cache->slots[i];

        if (slot->state != PLATINUM_AVATAR_EMPTY && strcmp(slot->ref, ref) == 0) {
            return slot;
        }
    }
    return NULL;
}

/* A slot for `ref`: an empty one if there is one, else the least recently used
 * slot that is not fetching. NULL when every slot is fetching. */
static platinum_avatar_slot *claim_slot(platinum_avatar_cache *cache, const char *ref)
{
    platinum_avatar_slot *victim = NULL;
    int i;

    for (i = 0; i < PLATINUM_AVATAR_SLOTS; i++) {
        platinum_avatar_slot *slot = &cache->slots[i];

        if (slot->state == PLATINUM_AVATAR_EMPTY) {
            victim = slot;
            break;
        }
        if (slot->state != PLATINUM_AVATAR_PENDING &&
            (victim == NULL || slot->used < victim->used)) {
            victim = slot;
        }
    }
    if (victim == NULL) {
        return NULL;
    }

    release_slot(victim);
    strcpy(victim->ref, ref);
    victim->used = ++cache->tick;
    return victim;
}

void platinum_avatar_cache_init(platinum_avatar_cache *cache)
{
    memset(cache, 0, sizeof(*cache));
}

void platinum_avatar_cache_dispose(platinum_avatar_cache *cache)
{
    int i;

    for (i = 0; i < PLATINUM_AVATAR_SLOTS; i++) {
        release_slot(&cache->slots[i]);
    }
    cache->tick = 0;
}

const platinum_image *platinum_avatar_lookup(platinum_avatar_cache *cache, const char *ref)
{
    platinum_avatar_slot *slot;

    if (!usable_ref(ref)) {
        return NULL;
    }
    slot = find_slot(cache, ref);
    if (slot == NULL || slot->state != PLATINUM_AVATAR_READY) {
        return NULL;
    }
    slot->used = ++cache->tick;
    return &slot->image;
}

int platinum_avatar_want(platinum_avatar_cache *cache, const char *ref)
{
    platinum_avatar_slot *slot;

    if (!usable_ref(ref) || find_slot(cache, ref) != NULL) {
        return 0;
    }
    slot = claim_slot(cache, ref);
    if (slot == NULL) {
        return 0;
    }
    slot->state = PLATINUM_AVATAR_PENDING;
    return 1;
}

int platinum_avatar_fulfil(platinum_avatar_cache *cache, const char *ref,
                           unsigned char *blob, long length)
{
    platinum_avatar_slot *slot;
    platinum_image image;
    int status;

    slot = usable_ref(ref) ? find_slot(cache, ref) : NULL;
    if (slot == NULL || slot->state != PLATINUM_AVATAR_PENDING) {
        free(blob);
        return -1;
    }

    status = platinum_image_parse(blob, length, &image);
    if (status == PLATINUM_IMAGE_OK) {
        slot->blob = blob;
        slot->image = image;
        slot->state = PLATINUM_AVATAR_READY;
        slot->used = ++cache->tick;
    } else {
        free(blob);
        memset(&slot->image, 0, sizeof(slot->image));
        slot->state = PLATINUM_AVATAR_FAILED;
    }
    return status;
}

void platinum_avatar_fail(platinum_avatar_cache *cache, const char *ref)
{
    platinum_avatar_slot *slot;

    slot = usable_ref(ref) ? find_slot(cache, ref) : NULL;
    if (slot != NULL && slot->state == PLATINUM_AVATAR_PENDING) {
        slot->state = PLATINUM_AVATAR_FAILED;
    }
}
