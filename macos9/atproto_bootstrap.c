#include "atproto_bootstrap.h"

#include <string.h>

#include "atproto_identity.h"
#include "json_min.h"

static wf_status bootstrap_copy(const char *value, char *out, size_t cap)
{
    size_t length;

    if (value == NULL || out == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;

    length = strlen(value);
    if (length >= cap) {
        out[0] = '\0';
        return WF_ERR_ALLOC;
    }

    memcpy(out, value, length + 1);
    return WF_OK;
}

static wf_status bootstrap_fetch(platinum_atproto_fetch_fn fetch,
                                 void *userdata, const char *url,
                                 size_t max_bytes, wf_response *response,
                                 int *http_status)
{
    wf_status status;

    memset(response, 0, sizeof(*response));
    *http_status = 0;

    status = fetch(userdata, url, max_bytes, response);
    if (response->status != 0)
        *http_status = (int)response->status;
    return status;
}

/*
 * The handle's own document. Any failure falls through to resolveHandle: the
 * well-known document is a convenience, and a handle that does not serve one
 * is resolved the protocol's other way. `found` is set only for a valid DID.
 */
static wf_status bootstrap_wellknown(platinum_atproto_fetch_fn fetch,
                                     void *userdata, const char *handle,
                                     char *out, size_t cap, int *found)
{
    char url[PLATINUM_ATPROTO_URL_MAX];
    wf_response response;
    wf_status status;
    int http_status;

    *found = 0;

    status = platinum_atproto_handle_wellknown_url(handle, url, sizeof(url));
    if (status != WF_OK)
        return status;

    status = bootstrap_fetch(fetch, userdata, url,
                             PLATINUM_ATPROTO_WELLKNOWN_MAX, &response,
                             &http_status);
    if (status == WF_OK && http_status >= 200 && http_status < 300 &&
        platinum_atproto_handle_wellknown_did(response.body, out, cap) == WF_OK)
        *found = 1;
    wf_response_free(&response);

    return WF_OK;
}

static wf_status bootstrap_resolve_handle(platinum_atproto_fetch_fn fetch,
                                          void *userdata, const char *handle,
                                          const char *service, char *out,
                                          size_t cap)
{
    char url[PLATINUM_ATPROTO_URL_MAX];
    wf_response response;
    wf_status status;
    int http_status;

    status = platinum_atproto_resolve_handle_url(service, handle, url,
                                                 sizeof(url));
    if (status != WF_OK)
        return status;

    status = bootstrap_fetch(fetch, userdata, url,
                             PLATINUM_ATPROTO_WELLKNOWN_MAX, &response,
                             &http_status);
    if (status != WF_OK) {
        wf_status result;

        result = (status == WF_ERR_HTTP && http_status == 404)
                     ? WF_ERR_HANDLE_DOCUMENT_NOT_FOUND
                     : status;
        wf_response_free(&response);
        return result;
    }

    if (http_status < 200 || http_status >= 300) {
        wf_response_free(&response);
        return WF_ERR_HANDLE_DOCUMENT_NOT_FOUND;
    }

    if (response.body == NULL ||
        platinum_json_get_string(response.body, "did", out, cap) != WF_OK ||
        !platinum_atproto_valid_did(out)) {
        out[0] = '\0';
        wf_response_free(&response);
        return WF_ERR_HANDLE_DOCUMENT_NOT_FOUND;
    }

    wf_response_free(&response);
    return WF_OK;
}

wf_status platinum_atproto_bootstrap(platinum_atproto_fetch_fn fetch,
                                     void *userdata, const char *handle,
                                     const char *fallback_service,
                                     char *did_out, size_t did_cap,
                                     char *pds_out, size_t pds_cap)
{
    char url[PLATINUM_ATPROTO_URL_MAX];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_response response;
    wf_status status;
    int http_status;
    int found;

    if (fetch == NULL || handle == NULL || did_out == NULL || pds_out == NULL ||
        did_cap == 0 || pds_cap == 0)
        return WF_ERR_INVALID_ARG;

    did_out[0] = '\0';
    pds_out[0] = '\0';

    if (!platinum_atproto_valid_handle(handle))
        return WF_ERR_INVALID_ARG;

    status = bootstrap_wellknown(fetch, userdata, handle, did, sizeof(did),
                                 &found);
    if (status != WF_OK)
        return status;

    if (!found) {
        if (fallback_service == NULL || fallback_service[0] == '\0')
            return WF_ERR_HANDLE_DOCUMENT_NOT_FOUND;

        status = bootstrap_resolve_handle(fetch, userdata, handle,
                                          fallback_service, did, sizeof(did));
        if (status != WF_OK)
            return status;
    }

    status = platinum_atproto_did_document_url(did, url, sizeof(url));
    if (status != WF_OK)
        return status;

    status = bootstrap_fetch(fetch, userdata, url, PLATINUM_ATPROTO_DOCUMENT_MAX,
                             &response, &http_status);
    if (status != WF_OK) {
        wf_status result;

        result = (status == WF_ERR_HTTP && http_status == 404)
                     ? WF_ERR_DID_DOCUMENT_NOT_FOUND
                     : status;
        wf_response_free(&response);
        return result;
    }

    if (http_status < 200 || http_status >= 300) {
        wf_response_free(&response);
        return WF_ERR_DID_DOCUMENT_NOT_FOUND;
    }

    status = platinum_atproto_did_pds(response.body, did, pds, sizeof(pds));
    if (status == WF_OK &&
        !platinum_atproto_did_claims_handle(response.body, handle))
        status = WF_ERR_HANDLE_RESOLVE;
    wf_response_free(&response);

    if (status != WF_OK)
        return status;

    status = bootstrap_copy(did, did_out, did_cap);
    if (status != WF_OK)
        return status;
    return bootstrap_copy(pds, pds_out, pds_cap);
}
