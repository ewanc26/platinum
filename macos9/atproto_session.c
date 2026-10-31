#include "atproto_session.h"

#include <string.h>

#include "atproto_url.h"
#include "json_min.h"

static int session_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int session_token_ok(const char *token)
{
    const char *scan;
    int dots;

    if (token == NULL || token[0] == '\0')
        return 0;

    dots = 0;
    for (scan = token; *scan != '\0'; scan++) {
        char value;

        value = *scan;
        if (value == '.') {
            dots++;
            continue;
        }
        if (!((value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z') || session_is_digit(value) ||
              value == '-' || value == '_'))
            return 0;
    }

    return dots == 2;
}

void platinum_atproto_session_clear(platinum_atproto_session *session)
{
    volatile unsigned char *scan;
    size_t length;

    if (session == NULL)
        return;

    scan = (volatile unsigned char *)session;
    length = sizeof(*session);
    while (length > 0) {
        *scan = 0;
        scan++;
        length--;
    }
}

void platinum_atproto_session_init(platinum_atproto_session *session)
{
    if (session == NULL)
        return;
    platinum_atproto_session_clear(session);
}

static wf_status session_read_string(platinum_json object, const char *name,
                                     char *destination, size_t capacity)
{
    wf_status status;

    status = platinum_json_string(object, name, destination, capacity);
    if (status == WF_ERR_ALLOC)
        return status;
    if (status != WF_OK)
        return WF_ERR_PARSE;
    return WF_OK;
}

wf_status platinum_atproto_session_parse_create(const char *body,
                                                const char *pds_url,
                                                platinum_atproto_session *out)
{
    platinum_json root;
    wf_status status;

    status = WF_ERR_PARSE;
    if (out == NULL)
        return WF_ERR_INVALID_ARG;

    platinum_atproto_session_clear(out);
    if (body == NULL || pds_url == NULL)
        return WF_ERR_INVALID_ARG;

    status = platinum_atproto_service_base(pds_url, out->pds_url,
                                           sizeof(out->pds_url));
    if (status != WF_OK) {
        platinum_atproto_session_clear(out);
        return status;
    }

    if (platinum_json_open(&root, body) != WF_OK)
        goto parse_failed;

    status = session_read_string(root, "accessJwt", out->access_jwt,
                                 sizeof(out->access_jwt));
    if (status != WF_OK)
        goto parse_failed;
    status = session_read_string(root, "refreshJwt", out->refresh_jwt,
                                 sizeof(out->refresh_jwt));
    if (status != WF_OK)
        goto parse_failed;
    status = session_read_string(root, "did", out->did, sizeof(out->did));
    if (status != WF_OK)
        goto parse_failed;
    status = session_read_string(root, "handle", out->handle,
                                 sizeof(out->handle));
    if (status != WF_OK)
        goto parse_failed;

    if (!session_token_ok(out->access_jwt) ||
        !session_token_ok(out->refresh_jwt) ||
        !platinum_atproto_valid_did(out->did) ||
        !platinum_atproto_valid_handle(out->handle))
        goto parse_failed;

    return WF_OK;

parse_failed:
    platinum_atproto_session_clear(out);
    return status == WF_ERR_ALLOC ? WF_ERR_ALLOC : WF_ERR_PARSE;
}

wf_status platinum_atproto_session_parse_refresh(
    const char *body, const platinum_atproto_session *previous,
    platinum_atproto_session *out)
{
    platinum_json root;
    platinum_json member;
    char saved_did[PLATINUM_ATPROTO_SESSION_DID_MAX + 1];
    char saved_handle[PLATINUM_ATPROTO_SESSION_HANDLE_MAX + 1];
    char saved_pds[PLATINUM_ATPROTO_SESSION_PDS_MAX + 1];
    wf_status status;

    status = WF_ERR_PARSE;
    if (out == NULL || previous == NULL)
        return WF_ERR_INVALID_ARG;

    memcpy(saved_did, previous->did, sizeof(saved_did));
    memcpy(saved_handle, previous->handle, sizeof(saved_handle));
    memcpy(saved_pds, previous->pds_url, sizeof(saved_pds));

    platinum_atproto_session_clear(out);
    if (body == NULL)
        return WF_ERR_INVALID_ARG;

    memcpy(out->did, saved_did, sizeof(out->did));
    memcpy(out->handle, saved_handle, sizeof(out->handle));
    memcpy(out->pds_url, saved_pds, sizeof(out->pds_url));

    if (platinum_json_open(&root, body) != WF_OK)
        goto refresh_failed;

    status = session_read_string(root, "accessJwt", out->access_jwt,
                                 sizeof(out->access_jwt));
    if (status != WF_OK)
        goto refresh_failed;
    status = session_read_string(root, "refreshJwt", out->refresh_jwt,
                                 sizeof(out->refresh_jwt));
    if (status != WF_OK)
        goto refresh_failed;

    if (platinum_json_member(root, "did", &member) == WF_OK) {
        status = session_read_string(root, "did", out->did, sizeof(out->did));
        if (status != WF_OK)
            goto refresh_failed;
    }
    if (platinum_json_member(root, "handle", &member) == WF_OK) {
        status = session_read_string(root, "handle", out->handle,
                                     sizeof(out->handle));
        if (status != WF_OK)
            goto refresh_failed;
    }

    if (!session_token_ok(out->access_jwt) ||
        !session_token_ok(out->refresh_jwt) ||
        !platinum_atproto_valid_did(out->did) ||
        !platinum_atproto_valid_handle(out->handle))
        goto refresh_failed;

    return WF_OK;

refresh_failed:
    platinum_atproto_session_clear(out);
    return status == WF_ERR_ALLOC ? WF_ERR_ALLOC : WF_ERR_PARSE;
}

static int session_base64_value(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (session_is_digit(c))
        return c - '0' + 52;
    if (c == '-')
        return 62;
    if (c == '_')
        return 63;
    return -1;
}

static long session_base64url_decode(const char *source, size_t length,
                                     char *out, size_t capacity)
{
    unsigned long accumulator;
    size_t index;
    size_t used;
    int bits;

    accumulator = 0;
    index = 0;
    used = 0;
    bits = 0;

    while (index < length && source[index] != '=') {
        int value;

        value = session_base64_value(source[index]);
        if (value < 0)
            return -1;

        accumulator = (accumulator << 6) | (unsigned long)value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (used >= capacity)
                return -1;
            out[used] = (char)((accumulator >> bits) & 0xFF);
            used++;
        }
        if (bits == 0)
            accumulator = 0;
        else
            accumulator &= (1UL << bits) - 1UL;
        index++;
    }

    while (index < length) {
        if (source[index] != '=')
            return -1;
        index++;
    }

    return (long)used;
}

static long session_payload(const platinum_atproto_session *session,
                            char *out, size_t capacity)
{
    const char *token;
    const char *first;
    const char *second;

    if (session == NULL)
        return -1;

    token = session->access_jwt;
    first = strchr(token, '.');
    if (first == NULL)
        return -1;
    second = strchr(first + 1, '.');
    if (second == NULL)
        return -1;

    return session_base64url_decode(first + 1, (size_t)(second - first - 1),
                                    out, capacity);
}

long platinum_atproto_session_access_expiry(
    const platinum_atproto_session *session)
{
    char payload[768];
    const char *scan;
    long value;
    long length;

    length = session_payload(session, payload, sizeof(payload) - 1);
    if (length <= 0)
        return 0;
    payload[length] = '\0';

    scan = strstr(payload, "\"exp\"");
    if (scan == NULL)
        return 0;
    scan += 5;
    while (*scan == ' ' || *scan == '\t')
        scan++;
    if (*scan != ':')
        return 0;
    scan++;
    while (*scan == ' ' || *scan == '\t')
        scan++;
    if (!session_is_digit(*scan))
        return 0;

    value = 0;
    while (session_is_digit(*scan)) {
        int digit;

        digit = *scan - '0';
        if (value > 214748364L)
            return 2147483647L;
        value = value * 10 + digit;
        scan++;
    }

    return value;
}

int platinum_atproto_session_access_expired(
    const platinum_atproto_session *session, long now, long skew_seconds)
{
    long expiry;
    long boundary;

    expiry = platinum_atproto_session_access_expiry(session);
    if (expiry == 0)
        return 0;

    if (skew_seconds < 0)
        skew_seconds = 0;
    if (expiry < skew_seconds)
        boundary = 0;
    else
        boundary = expiry - skew_seconds;

    return now >= boundary;
}

wf_status platinum_atproto_error_status(const char *body)
{
    platinum_json root;
    char code[64];

    if (body == NULL)
        return WF_ERR_HTTP;
    if (platinum_json_open(&root, body) != WF_OK)
        return WF_ERR_HTTP;
    if (platinum_json_string(root, "error", code, sizeof(code)) != WF_OK)
        return WF_ERR_HTTP;

    if (strcmp(code, "AuthRequired") == 0 ||
        strcmp(code, "ExpiredToken") == 0 ||
        strcmp(code, "InvalidToken") == 0)
        return WF_ERR_AUTH;
    if (strcmp(code, "NotFound") == 0 ||
        strcmp(code, "AccountNotFound") == 0)
        return WF_ERR_NOT_FOUND;
    if (strcmp(code, "InvalidRequest") == 0)
        return WF_ERR_VALIDATION;
    if (strcmp(code, "RateLimitExceeded") == 0)
        return WF_ERR_RATE_LIMIT;

    return WF_ERR_HTTP;
}
