#include "atproto_identity.h"

#include <string.h>

#include "atproto_url.h"
#include "json_min.h"

#define PLATINUM_ATPROTO_PATH_SEGMENTS 16

static int identity_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int identity_is_lower(char c)
{
    return c >= 'a' && c <= 'z';
}

static int identity_is_upper(char c)
{
    return c >= 'A' && c <= 'Z';
}

static int identity_is_alnum(char c)
{
    return identity_is_digit(c) || identity_is_lower(c) || identity_is_upper(c);
}

static int identity_hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static wf_status identity_append(char *out, size_t cap, size_t *used,
                                 const char *text)
{
    size_t length;

    if (out == NULL || used == NULL || text == NULL)
        return WF_ERR_INVALID_ARG;

    length = strlen(text);
    if (*used >= cap || length >= cap - *used)
        return WF_ERR_ALLOC;

    memcpy(out + *used, text, length + 1);
    *used += length;
    return WF_OK;
}

static wf_status identity_append_char(char *out, size_t cap, size_t *used,
                                      char value)
{
    if (out == NULL || used == NULL || *used + 1 >= cap)
        return WF_ERR_ALLOC;

    out[*used] = value;
    (*used)++;
    out[*used] = '\0';
    return WF_OK;
}

static int identity_path_segment_ok(const char *value, size_t length)
{
    size_t index;

    if (length == 0 || length > 64)
        return 0;
    if ((length == 1 && value[0] == '.') ||
        (length == 2 && value[0] == '.' && value[1] == '.'))
        return 0;

    for (index = 0; index < length; index++) {
        if (!identity_is_alnum(value[index]) && value[index] != '.' &&
            value[index] != '_' && value[index] != '-')
            return 0;
    }
    return 1;
}

/*
 * did:web:<host>[:<path>...], where a colon inside the host is percent-encoded
 * as %3A (the method's way of carrying a port). Only %3A is decoded: decoding
 * any other escape would let a DID introduce a path separator the validation
 * below never sees.
 */
static wf_status identity_did_web_url(const char *specific, char *out,
                                      size_t cap)
{
    char host[PLATINUM_ATPROTO_HANDLE_MAX + 8];
    char base[PLATINUM_ATPROTO_URL_MAX];
    char segment[65];
    const char *scan;
    const char *start;
    size_t host_used;
    size_t used;
    size_t index;
    int segment_count;
    int first;
    wf_status status;

    if (specific == NULL || specific[0] == '\0' || out == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;
    out[0] = '\0';

    /* Build the authority first: the first colon-separated part is the host
     * (with its optional encoded port), the rest is the document path. */
    start = specific;
    scan = strchr(start, ':');
    if (scan == NULL)
        scan = specific + strlen(specific);

    host_used = 0;
    for (index = 0; start + index < scan; index++) {
        char value;

        value = start[index];
        if (value == '%') {
            char high;
            char low;

            if (start + index + 2 >= scan)
                return WF_ERR_INVALID_ARG;
            high = start[index + 1];
            low = start[index + 2];
            if (identity_hex(high) < 0 || identity_hex(low) < 0)
                return WF_ERR_INVALID_ARG;
            if (high != '3' || (low != 'A' && low != 'a'))
                return WF_ERR_INVALID_ARG;
            value = ':';
            index += 2;
        }
        if (host_used + 1 >= sizeof(host))
            return WF_ERR_INVALID_ARG;
        host[host_used++] = value;
    }
    host[host_used] = '\0';
    if (host_used == 0)
        return WF_ERR_INVALID_ARG;

    strcpy(base, "https://");
    strcat(base, host);
    status = platinum_atproto_service_base(base, out, cap);
    if (status != WF_OK)
        return status;

    used = strlen(out);
    segment_count = 0;
    first = 1;
    while (*scan == ':') {
        scan++;
        start = scan;
        while (*scan != '\0' && *scan != ':')
            scan++;

        if ((size_t)(scan - start) >= sizeof(segment))
            return WF_ERR_INVALID_ARG;
        memcpy(segment, start, (size_t)(scan - start));
        segment[(size_t)(scan - start)] = '\0';
        if (!identity_path_segment_ok(segment, (size_t)(scan - start)))
            return WF_ERR_INVALID_ARG;

        status = identity_append_char(out, cap, &used, '/');
        if (status != WF_OK)
            return status;
        status = identity_append(out, cap, &used, segment);
        if (status != WF_OK)
            return status;

        segment_count++;
        if (segment_count >= PLATINUM_ATPROTO_PATH_SEGMENTS)
            return WF_ERR_INVALID_ARG;
        first = 0;
    }

    if (first) {
        status = identity_append(out, cap, &used, "/.well-known/did.json");
    } else {
        status = identity_append(out, cap, &used, "/did.json");
    }
    return status;
}

wf_status platinum_atproto_did_document_url(const char *did, char *out,
                                            size_t cap)
{
    const char *specific;
    size_t used;
    wf_status status;

    if (out == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;
    out[0] = '\0';

    if (did == NULL || !platinum_atproto_valid_did(did))
        return WF_ERR_INVALID_ARG;

    if (strncmp(did, "did:plc:", 8) == 0) {
        specific = did + 8;
        if (specific[0] == '\0')
            return WF_ERR_INVALID_ARG;
        /* A PLC identifier is base32: lowercase letters and digits. Refusing
         * anything else keeps a DID from injecting a path into the resolver
         * URL. */
        while (*specific != '\0') {
            if (!identity_is_lower(*specific) && !identity_is_digit(*specific))
                return WF_ERR_INVALID_ARG;
            specific++;
        }

        used = 0;
        status = identity_append(out, cap, &used, "https://plc.directory/");
        if (status != WF_OK)
            return status;
        status = identity_append(out, cap, &used, did);
        return status;
    }

    if (strncmp(did, "did:web:", 8) == 0)
        return identity_did_web_url(did + 8, out, cap);

    return WF_ERR_UNSUPPORTED;
}

wf_status platinum_atproto_resolve_handle_url(const char *service_base,
                                              const char *handle, char *out,
                                              size_t cap)
{
    const char *names[1];
    const char *values[1];

    if (!platinum_atproto_valid_handle(handle))
        return WF_ERR_INVALID_ARG;

    names[0] = "handle";
    values[0] = handle;
    return platinum_atproto_xrpc_url(service_base,
                                     "com.atproto.identity.resolveHandle",
                                     names, values, 1, out, cap);
}

static int identity_service_id_is_pds(const char *id)
{
    const char *hash;

    if (id == NULL)
        return 0;
    hash = strrchr(id, '#');
    if (hash == NULL)
        return 0;
    return strcmp(hash, "#atproto_pds") == 0;
}

wf_status platinum_atproto_did_pds(const char *document_json,
                                   const char *expected_did, char *out,
                                   size_t cap)
{
    platinum_json root;
    platinum_json services;
    platinum_json item;
    platinum_json member;
    char document_did[PLATINUM_ATPROTO_DID_MAX + 1];
    char service_id[PLATINUM_ATPROTO_DID_MAX + 32];
    char service_type[64];
    char endpoint[PLATINUM_ATPROTO_URL_MAX];
    long count;
    long index;

    if (document_json == NULL || out == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;
    out[0] = '\0';

    if (platinum_json_open(&root, document_json) != WF_OK)
        return WF_ERR_PARSE;
    if (platinum_json_string(root, "id", document_did,
                             sizeof(document_did)) != WF_OK)
        return WF_ERR_PARSE;
    if (expected_did != NULL && strcmp(document_did, expected_did) != 0)
        return WF_ERR_DID_DOCUMENT_NOT_FOUND;

    if (platinum_json_member(root, "service", &services) != WF_OK)
        return WF_ERR_NOT_FOUND;
    if (platinum_json_count(services, &count) != WF_OK)
        return WF_ERR_PARSE;
    if (count > 64)
        count = 64;

    for (index = 0; index < count; index++) {
        if (platinum_json_element(services, index, &item) != WF_OK)
            continue;
        if (platinum_json_member(item, "id", &member) != WF_OK ||
            platinum_json_string(item, "id", service_id,
                                 sizeof(service_id)) != WF_OK)
            continue;
        if (!identity_service_id_is_pds(service_id))
            continue;
        if (platinum_json_string(item, "type", service_type,
                                 sizeof(service_type)) != WF_OK)
            continue;
        if (strcmp(service_type, "AtprotoPersonalDataServer") != 0)
            continue;
        if (platinum_json_string(item, "serviceEndpoint", endpoint,
                                 sizeof(endpoint)) != WF_OK)
            continue;
        if (platinum_atproto_service_base(endpoint, out, cap) == WF_OK)
            return WF_OK;
    }

    return WF_ERR_NOT_FOUND;
}
