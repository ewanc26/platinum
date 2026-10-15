/*
 * The live profile fetch: Wolfram's getProfile, adapted to the route (Part 8,
 * live fetch). Built as C2x against Wolfram's headers and linked to libwolfram;
 * the route itself does not change.
 */
#ifndef PLATINUM_NATIVE_PROFILE_LIVE_H
#define PLATINUM_NATIVE_PROFILE_LIVE_H

#include "profile_route.h"

/* A platinum_profile_fetch_fn whose ctx is a wf_agent *. actor NULL means the
 * signed-in account (its DID comes from the agent). WF_ERR_NOT_FOUND is NOT_FOUND;
 * any other failure is ERROR. */
platinum_fetch_result platinum_profile_fetch_live(void *ctx, const char *actor, platinum_profile *out);

#endif
