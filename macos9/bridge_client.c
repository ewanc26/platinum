#include "bridge_client.h"

#include <stdlib.h>
#include <string.h>

#include "json_min.h"

/*
 * The Mac OS 9 bridge client.
 *
 * Every request is synchronous and bounded. Cooperation with the host event
 * loop comes from Wolfram's yield callback, which the transport invokes while
 * it pumps the connection, so this file must never spin on the network itself
 * and must not assume a request completes immediately.
 *
 * Response bodies are bounded by the transport's response limit. The protocol
 * objects parsed here are small and flat; anything larger is handed back to the
 * caller as an opaque wf_response for the UI layer to deal with.
 */

/* Longest pairing code the bridge issues or accepts. */
#define BRIDGE_PAIRING_CODE_MAX 6

/* Bridge protocol version this client speaks. */
#define BRIDGE_PROTOCOL_VERSION 1

/*
 * Upper bounds for the values read out of a pairing response.
 *
 * The bridge issues tokens as base64url of 32 random bytes (43 characters) and
 * an installation id as a UUID (36 characters); both buffers leave headroom.
 * Bounding them keeps a hostile or corrupt response from being copied into
 * whatever buffer the caller happened to provide.
 */
#define BRIDGE_TOKEN_MAX 64
#define BRIDGE_DID_MAX 128
#define BRIDGE_INSTALLATION_ID_MAX 48

/*
 * Enough for {"code":"..."} plus escaping headroom. A bridge pairing code is six
 * characters and escaping can at most double each one, so this has a wide
 * margin rather than being sized exactly.
 */
#define BRIDGE_PAIR_BODY_MAX 64

static char *bridge_strdup(const char *value)
{
    size_t len;
    char *copy;

    if (value == NULL)
        return NULL;
    len = strlen(value);
    copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, value, len + 1);
    return copy;
}

struct platinum_bridge_client {
    wf_xrpc_client *xrpc;
    char *base_url;
};

static char *bridge_join(const char *base, const char *path)
{
    size_t base_len;
    size_t path_len;
    int slash;
    char *url;

    if (base == NULL || path == NULL)
        return NULL;

    /*
     * Collapse the join. A configured base URL that already ends in '/' and a
     * path that starts with one must not produce '//': a doubled slash is not
     * the same URL to every router or proxy in front of the bridge, and the
     * user supplies the base URL by hand in the setup dialog.
     */
    base_len = strlen(base);
    while (base_len > 0 && base[base_len - 1] == '/')
        base_len--;
    while (*path == '/')
        path++;

    path_len = strlen(path);
    slash = (base_len != 0 && path_len != 0);

    url = (char *)malloc(base_len + path_len + (slash ? 1 : 0) + 1);
    if (url == NULL)
        return NULL;

    memcpy(url, base, base_len);
    if (slash)
        url[base_len++] = '/';
    memcpy(url + base_len, path, path_len);
    url[base_len + path_len] = '\0';
    return url;
}

/*
 * Build {"code":"<code>"} with the code escaped, so a code typed by hand cannot
 * break out of the JSON string and inject fields.
 *
 * The length is checked before anything is built. A bridge pairing code is a
 * fixed six characters, so a longer one is a typo worth reporting to the user
 * rather than a request worth sending. The bridge remains the authority on what
 * a code may contain; duplicating its character rules here would only create a
 * second definition that can drift.
 *
 * On success `*out` is a heap string the caller frees.
 */
static wf_status bridge_pair_body(const char *code, char **out)
{
    static const char prefix[] = "{\"code\":\"";
    static const char suffix[] = "\"}";
    char body[BRIDGE_PAIR_BODY_MAX];
    char escaped[BRIDGE_PAIR_BODY_MAX];
    size_t prefix_len;
    size_t suffix_len;
    size_t escaped_len;
    wf_status status;

    if (out == NULL || code == NULL)
        return WF_ERR_INVALID_ARG;

    *out = NULL;

    if (strlen(code) > BRIDGE_PAIRING_CODE_MAX)
        return WF_ERR_INVALID_ARG;

    status = platinum_json_escape(escaped, sizeof(escaped), code);
    if (status != WF_OK)
        return status;

    escaped_len = strlen(escaped);
    prefix_len = sizeof(prefix) - 1;
    suffix_len = sizeof(suffix) - 1;
    if (prefix_len + escaped_len + suffix_len + 1 > sizeof(body))
        return WF_ERR_ALLOC;

    memcpy(body, prefix, prefix_len);
    memcpy(body + prefix_len, escaped, escaped_len);
    /* The terminator comes with the suffix. */
    memcpy(body + prefix_len + escaped_len, suffix, suffix_len + 1);

    *out = bridge_strdup(body);
    return (*out == NULL) ? WF_ERR_ALLOC : WF_OK;
}

platinum_bridge_client *platinum_bridge_client_new(const char *base_url)
{
    platinum_bridge_client *client;

    if (base_url == NULL || base_url[0] == '\0')
        return NULL;

    client = (platinum_bridge_client *)calloc(1, sizeof(*client));
    if (client == NULL)
        return NULL;

    client->xrpc = wf_xrpc_client_new(base_url);
    wf_xrpc_client_set_max_response_bytes(client->xrpc, 262144);
    if (client->xrpc == NULL) {
        free(client);
        return NULL;
    }

    client->base_url = bridge_strdup(base_url);
    if (client->base_url == NULL) {
        wf_xrpc_client_free(client->xrpc);
        free(client);
        return NULL;
    }

    return client;
}

void platinum_bridge_client_free(platinum_bridge_client *client)
{
    if (client == NULL)
        return;
    wf_xrpc_client_free(client->xrpc);
    free(client->base_url);
    free(client);
}

wf_status platinum_bridge_client_set_token(platinum_bridge_client *client,
                                           const char *token)
{
    if (client == NULL || client->xrpc == NULL)
        return WF_ERR_INVALID_ARG;
    /* Wolfram prefixes "Bearer " itself, so the raw bridge token is what the
     * caller passes here. */
    wf_xrpc_client_set_auth(client->xrpc, token);
    return WF_OK;
}

void platinum_bridge_pairing_free(platinum_bridge_pairing *pairing)
{
    if (pairing == NULL)
        return;
    free(pairing->token);
    free(pairing->did);
    free(pairing->installation_id);
    memset(pairing, 0, sizeof(*pairing));
}

wf_status platinum_bridge_pair(platinum_bridge_client *client,
                               const char *code,
                               platinum_bridge_pairing *out)
{
    char *url;
    char *body;
    char token[BRIDGE_TOKEN_MAX];
    char did[BRIDGE_DID_MAX];
    char installation_id[BRIDGE_INSTALLATION_ID_MAX];
    long protocol;
    wf_response raw;
    wf_status status;

    if (client == NULL || client->xrpc == NULL || code == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;

    /* Reset before anything else: the caller may pass an unpopulated struct,
     * and freeing its fields first would free whatever was on the stack. */
    memset(out, 0, sizeof(*out));
    memset(&raw, 0, sizeof(raw));

    status = bridge_pair_body(code, &body);
    if (status != WF_OK)
        return status;

    url = bridge_join(client->base_url, "/v1/pair");
    if (url == NULL) {
        free(body);
        return WF_ERR_ALLOC;
    }

    status = wf_http_post(client->xrpc, url, "application/json", body,
                          NULL, 0, &raw);
    free(url);
    free(body);
    if (status != WF_OK) {
        wf_response_free(&raw);
        return status;
    }

    /* A 4xx from the bridge is a normal outcome here: an expired or mistyped
     * pairing code. wf_http_post already reports that as WF_ERR_HTTP, and the
     * body carries the reason, so the UI layer can explain it. Only a
     * well-formed 200 is turned into a pairing. */
    if (raw.status != 200) {
        wf_response_free(&raw);
        return WF_ERR_HTTP;
    }

    if (raw.body == NULL) {
        wf_response_free(&raw);
        return WF_ERR_PARSE;
    }

    /*
     * Every member is read into a local buffer before any of it is stored in
     * `out`. The previous implementation assigned each field as it parsed and
     * then inspected the same field after platinum_bridge_pairing_free() had
     * zeroed the struct, so a malformed response was always reported as
     * WF_ERR_ALLOC no matter what had actually gone wrong.
     *
     * Reading everything first also means a response missing one field leaves
     * the caller with nothing rather than a half-populated pairing that still
     * looks usable.
     */
    status = platinum_json_get_int(raw.body, "protocol", &protocol);
    if (status != WF_OK) {
        wf_response_free(&raw);
        return status;
    }

    status = platinum_json_get_string(raw.body, "token", token, sizeof(token));
    if (status != WF_OK) {
        wf_response_free(&raw);
        return status;
    }

    status = platinum_json_get_string(raw.body, "did", did, sizeof(did));
    if (status != WF_OK) {
        wf_response_free(&raw);
        return status;
    }

    status = platinum_json_get_string(raw.body, "installationId",
                                      installation_id,
                                      sizeof(installation_id));
    if (status != WF_OK) {
        wf_response_free(&raw);
        return status;
    }

    if (protocol != BRIDGE_PROTOCOL_VERSION) {
        wf_response_free(&raw);
        return WF_ERR_UNSUPPORTED;
    }

    out->protocol = (int)protocol;
    out->token = bridge_strdup(token);
    out->did = bridge_strdup(did);
    out->installation_id = bridge_strdup(installation_id);
    if (out->token == NULL || out->did == NULL ||
        out->installation_id == NULL) {
        platinum_bridge_pairing_free(out);
        wf_response_free(&raw);
        return WF_ERR_ALLOC;
    }

    wf_response_free(&raw);
    return WF_OK;
}

wf_status platinum_bridge_get(platinum_bridge_client *client,
                              const char *path,
                              wf_response *out)
{
    char *url;
    wf_status status;

    if (client == NULL || client->xrpc == NULL || path == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;

    url = bridge_join(client->base_url, path);
    if (url == NULL)
        return WF_ERR_ALLOC;

    status = wf_http_get(client->xrpc, url, out);
    free(url);
    return status;
}

wf_status platinum_bridge_post(platinum_bridge_client *client,
                               const char *path,
                               const char *json_body,
                               wf_response *out)
{
    char *url;
    wf_status status;

    if (client == NULL || client->xrpc == NULL || path == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;

    /* Refuse to send a body that is not well-formed JSON. The bridge would
     * reject it anyway, and failing here keeps a malformed request from
     * reaching the network at all. */
    if (json_body != NULL) {
        status = platinum_json_valid(json_body);
        if (status != WF_OK)
            return status;
    }

    url = bridge_join(client->base_url, path);
    if (url == NULL)
        return WF_ERR_ALLOC;

    status = wf_http_post(client->xrpc, url, "application/json", json_body,
                          NULL, 0, out);
    free(url);
    return status;
}

wf_status platinum_bridge_revoke(platinum_bridge_client *client,
                                 wf_response *out)
{
    return platinum_bridge_post(client, "/v1/revoke", NULL, out);
}
