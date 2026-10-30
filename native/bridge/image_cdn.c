#include "image_cdn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_source_wolfram.h"

platinum_image_result
platinum_image_source_cdn(void *ctx, const char *ref, platinum_rgba *out)
{
    platinum_cdn *cdn = (platinum_cdn *) ctx;
    char url[512];
    wf_response res;
    wf_status st;
    platinum_image_result r;

    if (cdn == NULL || cdn->client == NULL || cdn->base == NULL || out == NULL) return PLATINUM_IMAGE_ERROR;
    memset(out, 0, sizeof *out);
    if (platinum_valid_image_ref(ref) != 0) return PLATINUM_IMAGE_ERROR;
    if (strlen(cdn->base) + strlen(ref) + 8 >= sizeof url) return PLATINUM_IMAGE_ERROR;
    sprintf(url, "%s/img/%s", cdn->base, ref);

    memset(&res, 0, sizeof res);
    st = wf_http_get_public(cdn->client, url, PLATINUM_CDN_MAX_BYTES, &res);
    if (st != WF_OK || res.body == NULL || res.body_len == 0) {
        wf_response_free(&res);
        return PLATINUM_IMAGE_ERROR;
    }
    r = platinum_image_from_bytes(res.body, res.body_len, out);
    wf_response_free(&res);
    return r;
}
