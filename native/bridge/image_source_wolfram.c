#include "image_source_wolfram.h"

#include <stdlib.h>
#include <string.h>

#include <wolfram/image.h>
#include <wolfram/xrpc.h>

platinum_image_result
platinum_image_from_bytes(const void *bytes, size_t len, platinum_rgba *out)
{
    wf_image_rgba img;
    wf_status st;

    if (out == NULL) return PLATINUM_IMAGE_ERROR;
    memset(out, 0, sizeof *out);
    memset(&img, 0, sizeof img);

    /* max_dim 0: no downscale here; the PLIM encoder shrinks after flattening. */
    st = wf_image_decode_rgba(bytes, len, 0, &img);
    if (st == WF_ERR_UNSUPPORTED) {
        wf_image_rgba_free(&img);
        return PLATINUM_IMAGE_TOO_LARGE;
    }
    if (st != WF_OK || img.pixels == NULL) {
        wf_image_rgba_free(&img);
        return PLATINUM_IMAGE_ERROR;
    }
    out->pixels = img.pixels;
    out->width = img.width;
    out->height = img.height;
    /* The pixels now belong to the route, which frees them with free(). Wolfram's free
     * releases its own allocation, so the pixels are copied into the caller's allocator. */
    {
        size_t n = (size_t) img.width * (size_t) img.height * 4;
        unsigned char *copy = malloc(n);
        if (copy == NULL) {
            wf_image_rgba_free(&img);
            memset(out, 0, sizeof *out);
            return PLATINUM_IMAGE_ERROR;
        }
        memcpy(copy, img.pixels, n);
        wf_image_rgba_free(&img);
        out->pixels = copy;
    }
    return PLATINUM_IMAGE_OK;
}
