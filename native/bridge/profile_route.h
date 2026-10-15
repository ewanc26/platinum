/*
 * The /v1/profile handler, with the fetch injected (Part 8, slice 4).
 *
 * It decides the status and body the way server.ts does: a bad actor is 400,
 * an unknown account is 404, any other failure is 500 bridge_error, and a found
 * profile is 200 with the encoded profile. The fetch is a callback, so this file
 * does not depend on Wolfram or the network. The Wolfram-backed fetch lives in
 * profile_wolfram.c, and the HTTP server that calls this is Part 10 (blocked).
 */
#ifndef PLATINUM_NATIVE_PROFILE_ROUTE_H
#define PLATINUM_NATIVE_PROFILE_ROUTE_H

#include <stddef.h>
#include "profile.h"

typedef enum platinum_fetch_result {
    PLATINUM_FETCH_OK = 0,
    PLATINUM_FETCH_NOT_FOUND,
    PLATINUM_FETCH_ERROR
} platinum_fetch_result;

/* Fetches the profile for actor (a normalised handle or DID), or the signed-in
 * account's own profile when actor is NULL. Fills out and returns a result. */
typedef platinum_fetch_result (*platinum_profile_fetch_fn)(void *ctx, const char *actor,
                                                           platinum_profile *out);

/* Handles GET /v1/profile. actor_param is the actor query value, or NULL when the
 * parameter is absent. Writes the JSON body into body and returns the HTTP status.
 * If the body does not fit, the status is 500 and the body is the generic error. */
int platinum_route_profile(void *ctx, platinum_profile_fetch_fn fetch,
                           const char *actor_param, char *body, size_t cap);

#endif
