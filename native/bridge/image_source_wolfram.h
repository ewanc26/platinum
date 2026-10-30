/*
 * Decodes source bytes with Wolfram's decoder into the route's RGBA (Part 9, C2x).
 * Wolfram refuses a source over WF_IMAGE_MAX_SOURCE_DIM pixels a side; that maps to
 * PLATINUM_IMAGE_TOO_LARGE, and everything else that goes wrong to ERROR.
 */
#ifndef PLATINUM_NATIVE_IMAGE_SOURCE_WOLFRAM_H
#define PLATINUM_NATIVE_IMAGE_SOURCE_WOLFRAM_H

#include "image_route.h"

platinum_image_result platinum_image_from_bytes(const void *bytes, size_t len, platinum_rgba *out);

#endif
