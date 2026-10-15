/*
 * Profile route support for the native bridge (Part 8, slice 2).
 *
 * Covers what /v1/profile does without a network call: checking the actor
 * parameter, and encoding a profile the way the TypeScript route does (keys in
 * the same order, absent fields omitted, not null). The Wolfram calls that fill
 * the struct are a later step, so this module takes the data as input.
 *
 * Not covered yet: the pinned post (it needs a timeline post encoder) and the
 * description clip, which the handler applies before it fills the struct.
 */
#ifndef PLATINUM_NATIVE_PROFILE_H
#define PLATINUM_NATIVE_PROFILE_H

#include <stddef.h>

/* Matches the TypeScript validActor rule: a DID or a handle, at most 253 bytes. */
int platinum_valid_actor(const char *in, char *out, size_t cap);

typedef struct platinum_profile {
    char did[256];
    char handle[256];
    char display_name[512];
    char description[2048];
    char avatar[512];
    char banner[512];
    int has_display_name;
    int has_description;
    int has_avatar;
    int has_banner;
    int has_handle;
    long followers_count;
    long follows_count;
    long posts_count;
    int has_followers_count;
    int has_follows_count;
    int has_posts_count;
    /* -1 absent (your own profile), 0 false, 1 true. */
    int following;
    int followed_by;
    int muted;
    int blocking;
} platinum_profile;

/* Writes the profile as a JSON object into buf. Returns the length written,
 * or -1 if it does not fit (buf is then empty). */
int platinum_profile_json(const platinum_profile *p, char *buf, size_t cap);

#endif
