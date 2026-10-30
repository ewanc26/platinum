/*
 * Maps Wolfram's wf_agent_profile (from app.bsky.actor.getProfile) onto the
 * bridge's profile struct, so /v1/profile can encode it.
 *
 * This file uses Wolfram's headers, which are C23, so it is built with
 * -std=c2x, not with the strict C89 flags the rest of native/bridge uses.
 *
 * Fields the route has but Wolfram's struct lacks are left out, not invented:
 * banner, followedBy and avatar (the bridge's avatar is a served reference, not
 * the CDN URL Wolfram gives). The description is clipped to 511 code points,
 * as the TypeScript does.
 */
#ifndef PLATINUM_NATIVE_PROFILE_WOLFRAM_H
#define PLATINUM_NATIVE_PROFILE_WOLFRAM_H

#include <wolfram/agent.h>
#include "profile.h"

/* mine is 1 when the profile is the signed-in account's own: the viewer fields
 * are then omitted, as the TypeScript does. Returns 0, or -1 if a required
 * field (did) is missing or does not fit. */
int platinum_profile_from_wolfram(const wf_agent_profile *src, int mine, platinum_profile *out);

/* Copies at most max_cp Unicode code points of src into dst, never splitting a
 * sequence. Returns the bytes written, or -1 if dst (cap bytes) is too small. */
long platinum_utf8_clip_codepoints(const char *src, long max_cp, char *dst, size_t cap);

#endif
