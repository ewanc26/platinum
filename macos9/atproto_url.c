#include "atproto_url.h"

#include <string.h>

static int atproto_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int atproto_is_lower(char c)
{
    return c >= 'a' && c <= 'z';
}

static int atproto_is_upper(char c)
{
    return c >= 'A' && c <= 'Z';
}

static int atproto_is_alpha(char c)
{
    return atproto_is_lower(c) || atproto_is_upper(c);
}

static int atproto_is_alnum(char c)
{
    return atproto_is_alpha(c) || atproto_is_digit(c);
}

static char atproto_lower(char c)
{
    if (atproto_is_upper(c))
        return (char)(c - 'A' + 'a');
    return c;
}

static wf_status atproto_append(char *out, size_t cap, size_t *used,
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

static wf_status atproto_append_char(char *out, size_t cap, size_t *used,
                                     char value)
{
    if (out == NULL || used == NULL || *used + 1 >= cap)
        return WF_ERR_ALLOC;

    out[*used] = value;
    (*used)++;
    out[*used] = '\0';
    return WF_OK;
}

static int atproto_host_ok(const char *host, size_t length)
{
    size_t index;
    size_t label_length;
    char last;

    if (host == NULL || length == 0 || length > PLATINUM_ATPROTO_HANDLE_MAX)
        return 0;
    if (host[0] == '.' || host[length - 1] == '.')
        return 0;

    label_length = 0;
    last = '\0';
    for (index = 0; index < length; index++) {
        char value;

        value = host[index];
        if (value == '.') {
            if (label_length == 0 || last == '-')
                return 0;
            label_length = 0;
            last = value;
            continue;
        }
        if (!atproto_is_alnum(value) && value != '-')
            return 0;
        if (value == '-' && label_length == 0)
            return 0;
        label_length++;
        if (label_length > 63)
            return 0;
        last = value;
    }

    return label_length > 0 && last != '-';
}

wf_status platinum_atproto_service_base(const char *input, char *out,
                                        size_t cap)
{
    const char *authority;
    const char *slash;
    const char *colon;
    const char *scan;
    size_t authority_length;
    size_t host_length;
    size_t used;
    size_t index;
    long port;
    int port_digits;
    wf_status status;

    if (input == NULL || out == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;
    out[0] = '\0';

    if (strlen(input) < 9)
        return WF_ERR_INVALID_ARG;
    if (atproto_lower(input[0]) != 'h' || atproto_lower(input[1]) != 't' ||
        atproto_lower(input[2]) != 't' || atproto_lower(input[3]) != 'p' ||
        atproto_lower(input[4]) != 's' || input[5] != ':' || input[6] != '/' ||
        input[7] != '/')
        return WF_ERR_INVALID_ARG;

    authority = input + 8;
    slash = strchr(authority, '/');
    if (slash != NULL) {
        if (slash[1] != '\0')
            return WF_ERR_INVALID_ARG;
        authority_length = (size_t)(slash - authority);
    } else {
        authority_length = strlen(authority);
    }

    if (authority_length == 0 || authority_length > 300)
        return WF_ERR_INVALID_ARG;

    colon = NULL;
    for (index = 0; index < authority_length; index++) {
        char value;

        value = authority[index];
        if (value == '@' || value == '?' || value == '#' || value == '/')
            return WF_ERR_INVALID_ARG;
        if (value == ':') {
            if (colon != NULL)
                return WF_ERR_INVALID_ARG;
            colon = authority + index;
        }
    }

    host_length = colon != NULL ? (size_t)(colon - authority) : authority_length;
    if (!atproto_host_ok(authority, host_length))
        return WF_ERR_INVALID_ARG;

    port = 0;
    port_digits = 0;
    if (colon != NULL) {
        scan = colon + 1;
        if (*scan == '\0')
            return WF_ERR_INVALID_ARG;
        while (*scan != '\0') {
            if (!atproto_is_digit(*scan))
                return WF_ERR_INVALID_ARG;
            if (port > 6553 || (port == 6553 && *scan > '5'))
                return WF_ERR_INVALID_ARG;
            port = port * 10 + (*scan - '0');
            port_digits++;
            if (port_digits > 5)
                return WF_ERR_INVALID_ARG;
            scan++;
        }
        if (port < 1)
            return WF_ERR_INVALID_ARG;
    }

    used = 0;
    status = atproto_append(out, cap, &used, "https://");
    if (status != WF_OK)
        return status;

    for (index = 0; index < host_length; index++) {
        status = atproto_append_char(out, cap, &used,
                                     atproto_lower(authority[index]));
        if (status != WF_OK)
            return status;
    }

    if (colon != NULL) {
        status = atproto_append_char(out, cap, &used, ':');
        if (status != WF_OK)
            return status;
        for (scan = colon + 1; *scan != '\0'; scan++) {
            status = atproto_append_char(out, cap, &used, *scan);
            if (status != WF_OK)
                return status;
        }
    }

    return WF_OK;
}

wf_status platinum_atproto_escape(const char *value, char *out, size_t cap)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t used;

    if (out == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;
    out[0] = '\0';
    if (value == NULL)
        return WF_ERR_INVALID_ARG;

    used = 0;
    while (*value != '\0') {
        unsigned char byte;

        byte = (unsigned char)*value;
        if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '.' ||
            byte == '_' || byte == '~') {
            if (used + 1 >= cap) {
                out[0] = '\0';
                return WF_ERR_ALLOC;
            }
            out[used++] = (char)byte;
        } else {
            if (used + 3 >= cap) {
                out[0] = '\0';
                return WF_ERR_ALLOC;
            }
            out[used++] = '%';
            out[used++] = hex[(byte >> 4) & 0x0F];
            out[used++] = hex[byte & 0x0F];
        }
        value++;
    }

    out[used] = '\0';
    return WF_OK;
}

static int atproto_param_name_ok(const char *name)
{
    const char *scan;

    if (name == NULL || name[0] == '\0')
        return 0;
    if (!atproto_is_alpha(name[0]) && name[0] != '_')
        return 0;

    for (scan = name; *scan != '\0'; scan++) {
        if (!atproto_is_alnum(*scan) && *scan != '.' && *scan != '_' &&
            *scan != '-')
            return 0;
    }
    return 1;
}

int platinum_atproto_valid_nsid(const char *nsid)
{
    const char *start;
    const char *end;
    size_t length;
    size_t segment_length;
    size_t index;
    int segments;

    if (nsid == NULL)
        return 0;
    length = strlen(nsid);
    if (length < 5 || length > PLATINUM_ATPROTO_NSID_MAX)
        return 0;

    /* The authority segments are lowercase DNS names; the name (the last
     * segment) is lowerCamelCase, which is why getTimeline and createSession
     * are valid and a blanket lowercase rule is not. */
    segments = 0;
    start = nsid;
    for (;;) {
        end = strchr(start, '.');
        segment_length = end != NULL ? (size_t)(end - start) : strlen(start);
        if (segment_length == 0 || segment_length > 63)
            return 0;

        if (end == NULL) {
            if (!atproto_is_alpha(start[0]))
                return 0;
            for (index = 0; index < segment_length; index++) {
                if (!atproto_is_alnum(start[index]))
                    return 0;
            }
        } else {
            if (!atproto_is_lower(start[0]) || start[segment_length - 1] == '-')
                return 0;
            for (index = 0; index < segment_length; index++) {
                char value;

                value = start[index];
                if (!atproto_is_lower(value) && !atproto_is_digit(value) &&
                    value != '-')
                    return 0;
            }
        }

        segments++;
        if (end == NULL)
            break;
        start = end + 1;
    }

    return segments >= 3;
}

int platinum_atproto_valid_did(const char *did)
{
    const char *scan;
    size_t method_length;
    size_t id_length;
    size_t total;

    if (did == NULL)
        return 0;
    if (strncmp(did, "did:", 4) != 0)
        return 0;
    total = strlen(did);
    if (total > PLATINUM_ATPROTO_DID_MAX)
        return 0;

    scan = did + 4;
    method_length = 0;
    while (*scan != '\0' && *scan != ':') {
        if (!atproto_is_lower(*scan))
            return 0;
        method_length++;
        if (method_length > 20)
            return 0;
        scan++;
    }
    if (method_length == 0 || *scan != ':')
        return 0;

    scan++;
    id_length = 0;
    while (*scan != '\0') {
        char value;

        value = *scan;
        if (!atproto_is_alnum(value) && value != '.' && value != '_' &&
            value != ':' && value != '%' && value != '-')
            return 0;
        id_length++;
        scan++;
    }

    return id_length > 0;
}

int platinum_atproto_valid_handle(const char *handle)
{
    const char *scan;
    size_t length;
    size_t label_length;
    int dots;
    char last;

    if (handle == NULL)
        return 0;
    length = strlen(handle);
    if (length == 0 || length > PLATINUM_ATPROTO_HANDLE_MAX)
        return 0;
    if (handle[0] == '.' || handle[length - 1] == '.')
        return 0;

    dots = 0;
    label_length = 0;
    last = '\0';
    for (scan = handle; *scan != '\0'; scan++) {
        if (*scan == '.') {
            if (label_length == 0 || last == '-')
                return 0;
            dots++;
            label_length = 0;
            last = *scan;
            continue;
        }
        if (!atproto_is_alnum(*scan) && *scan != '-')
            return 0;
        if (*scan == '-' && label_length == 0)
            return 0;
        label_length++;
        if (label_length > 63)
            return 0;
        last = *scan;
    }

    return dots > 0 && label_length > 0 && last != '-';
}

int platinum_atproto_valid_at_uri(const char *uri)
{
    const char *authority;
    const char *collection;
    const char *key;
    size_t authority_length;
    size_t collection_length;
    size_t key_length;
    size_t index;
    char buffer[PLATINUM_ATPROTO_NSID_MAX + 1];
    char authority_buffer[PLATINUM_ATPROTO_DID_MAX + 1];

    if (uri == NULL)
        return 0;
    if (strlen(uri) > PLATINUM_ATPROTO_AT_URI_MAX)
        return 0;
    if (strncmp(uri, "at://", 5) != 0)
        return 0;

    authority = uri + 5;
    collection = strchr(authority, '/');
    if (collection == NULL)
        return 0;
    authority_length = (size_t)(collection - authority);
    if (authority_length == 0)
        return 0;

    collection++;
    key = strchr(collection, '/');
    if (key == NULL)
        return 0;
    collection_length = (size_t)(key - collection);
    if (collection_length == 0 || collection_length > PLATINUM_ATPROTO_NSID_MAX)
        return 0;

    key++;
    key_length = strlen(key);
    if (key_length == 0 || key_length > 512)
        return 0;

    for (index = 0; index < key_length; index++) {
        char value;

        value = key[index];
        if (!atproto_is_alnum(value) && value != '.' && value != '_' &&
            value != '~' && value != ':' && value != '-')
            return 0;
    }
    if (strchr(key, '/') != NULL || strchr(key, '?') != NULL ||
        strchr(key, '#') != NULL)
        return 0;

    memcpy(buffer, collection, collection_length);
    buffer[collection_length] = '\0';
    if (!platinum_atproto_valid_nsid(buffer))
        return 0;

    memcpy(authority_buffer, authority, authority_length);
    authority_buffer[authority_length] = '\0';
    if (!platinum_atproto_valid_did(authority_buffer) &&
        !platinum_atproto_valid_handle(authority_buffer))
        return 0;

    return 1;
}

wf_status platinum_atproto_xrpc_url(const char *service_base, const char *nsid,
                                    const char *const *names,
                                    const char *const *values, size_t count,
                                    char *out, size_t cap)
{
    char base[PLATINUM_ATPROTO_URL_MAX];
    char encoded[PLATINUM_ATPROTO_URL_MAX];
    size_t used;
    size_t index;
    wf_status status;

    if (out == NULL || cap == 0)
        return WF_ERR_INVALID_ARG;
    out[0] = '\0';

    if (count > PLATINUM_ATPROTO_PARAMS_MAX)
        return WF_ERR_INVALID_ARG;
    if (count > 0 && (names == NULL || values == NULL))
        return WF_ERR_INVALID_ARG;

    status = platinum_atproto_service_base(service_base, base, sizeof(base));
    if (status != WF_OK)
        return status;
    if (!platinum_atproto_valid_nsid(nsid))
        return WF_ERR_INVALID_ARG;

    used = 0;
    status = atproto_append(out, cap, &used, base);
    if (status != WF_OK)
        return status;
    status = atproto_append(out, cap, &used, "/xrpc/");
    if (status != WF_OK)
        return status;
    status = atproto_append(out, cap, &used, nsid);
    if (status != WF_OK)
        return status;

    for (index = 0; index < count; index++) {
        if (!atproto_param_name_ok(names[index]) || values[index] == NULL ||
            values[index][0] == '\0')
            return WF_ERR_INVALID_ARG;

        status = atproto_append_char(out, cap, &used, index == 0 ? '?' : '&');
        if (status != WF_OK)
            return status;
        status = atproto_append(out, cap, &used, names[index]);
        if (status != WF_OK)
            return status;
        status = atproto_append_char(out, cap, &used, '=');
        if (status != WF_OK)
            return status;

        status = platinum_atproto_escape(values[index], encoded,
                                         sizeof(encoded));
        if (status != WF_OK)
            return status;
        status = atproto_append(out, cap, &used, encoded);
        if (status != WF_OK)
            return status;
    }

    return WF_OK;
}
