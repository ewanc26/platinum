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
    if (client->xrpc == NULL) {
        free(client);
        return NULL;
    }
    wf_xrpc_client_set_max_response_bytes(client->xrpc, 262144);

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

/*
 * Every member is read into a local buffer before any of it is stored in `out`,
 * so a response missing one field leaves the caller with nothing rather than a
 * half-populated pairing that still looks usable. (An earlier version assigned
 * each field as it parsed and then inspected the freed struct, so a malformed
 * response was always reported as WF_ERR_ALLOC.)
 */
static wf_status bridge_read_pairing(const char *body,
                                     platinum_bridge_pairing *out)
{
    char token[BRIDGE_TOKEN_MAX];
    char did[BRIDGE_DID_MAX];
    char installation_id[BRIDGE_INSTALLATION_ID_MAX];
    long protocol;
    wf_status status;

    status = platinum_json_get_int(body, "protocol", &protocol);
    if (status != WF_OK)
        return status;
    status = platinum_json_get_string(body, "token", token, sizeof(token));
    if (status != WF_OK)
        return status;
    status = platinum_json_get_string(body, "did", did, sizeof(did));
    if (status != WF_OK)
        return status;
    status = platinum_json_get_string(body, "installationId", installation_id,
                                      sizeof(installation_id));
    if (status != WF_OK)
        return status;
    if (protocol != BRIDGE_PROTOCOL_VERSION)
        return WF_ERR_UNSUPPORTED;

    out->protocol = (int)protocol;
    out->token = bridge_strdup(token);
    out->did = bridge_strdup(did);
    out->installation_id = bridge_strdup(installation_id);
    if (out->token == NULL || out->did == NULL ||
        out->installation_id == NULL) {
        platinum_bridge_pairing_free(out);
        return WF_ERR_ALLOC;
    }
    return WF_OK;
}

wf_status platinum_bridge_pair(platinum_bridge_client *client,
                               const char *code,
                               platinum_bridge_pairing *out)
{
    char *url;
    char *body;
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

    status = bridge_read_pairing(raw.body, out);
    wf_response_free(&raw);
    return status;
}

void platinum_bridge_wipe(void *data, size_t size)
{
    volatile unsigned char *p = (volatile unsigned char *)data;

    while (data != NULL && size-- > 0)
        *p++ = 0;
}

#define BRIDGE_LOGIN_FIELD_MAX 256
/* Each escaped field may grow six-fold; plus the JSON around them. */
static char login_identifier[BRIDGE_LOGIN_FIELD_MAX * 6 + 1];
static char login_password[BRIDGE_LOGIN_FIELD_MAX * 6 + 1];
static char login_body[BRIDGE_LOGIN_FIELD_MAX * 12 + 96];

static void bridge_login_wipe(void)
{
    platinum_bridge_wipe(login_identifier, sizeof(login_identifier));
    platinum_bridge_wipe(login_password, sizeof(login_password));
    platinum_bridge_wipe(login_body, sizeof(login_body));
}

int platinum_bridge_login_scratch_clear(void)
{
    size_t i;

    for (i = 0; i < sizeof(login_identifier); ++i)
        if (login_identifier[i] != 0)
            return 0;
    for (i = 0; i < sizeof(login_password); ++i)
        if (login_password[i] != 0)
            return 0;
    for (i = 0; i < sizeof(login_body); ++i)
        if (login_body[i] != 0)
            return 0;
    return 1;
}

static int bridge_login_reason(const char *body)
{
    char code[48];

    if (body == NULL ||
        platinum_json_get_string(body, "error", code, sizeof(code)) != WF_OK)
        return PLATINUM_LOGIN_OTHER;
    if (strcmp(code, "app_password_disabled") == 0)
        return PLATINUM_LOGIN_DISABLED;
    if (strcmp(code, "invalid_credentials") == 0)
        return PLATINUM_LOGIN_INVALID;
    if (strcmp(code, "too_many_attempts") == 0)
        return PLATINUM_LOGIN_TOO_MANY;
    if (strcmp(code, "invalid_service") == 0)
        return PLATINUM_LOGIN_BAD_SERVICE;
    if (strcmp(code, "invalid_request") == 0)
        return PLATINUM_LOGIN_BAD_REQUEST;
    return PLATINUM_LOGIN_OTHER;
}

wf_status platinum_bridge_login_app_password(platinum_bridge_client *client,
                                             const char *identifier,
                                             const char *password,
                                             platinum_bridge_pairing *out,
                                             int *reason)
{
    char *url;
    wf_response raw;
    wf_status status;
    size_t i;

    if (reason != NULL)
        *reason = PLATINUM_LOGIN_OTHER;
    if (client == NULL || client->xrpc == NULL || identifier == NULL ||
        password == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    memset(&raw, 0, sizeof(raw));

    if (identifier[0] == '\0' || password[0] == '\0' ||
        strlen(identifier) > BRIDGE_LOGIN_FIELD_MAX ||
        strlen(password) > BRIDGE_LOGIN_FIELD_MAX)
        return WF_ERR_INVALID_ARG;
    for (i = 0; password[i] != '\0'; ++i)
        if ((unsigned char)password[i] < 32 || password[i] == 127)
            return WF_ERR_INVALID_ARG;

    if (platinum_json_escape(login_identifier, sizeof(login_identifier),
                             identifier) != WF_OK ||
        platinum_json_escape(login_password, sizeof(login_password),
                             password) != WF_OK) {
        bridge_login_wipe();
        return WF_ERR_INVALID_ARG;
    }
    strcpy(login_body, "{\"identifier\":\"");
    strcat(login_body, login_identifier);
    strcat(login_body, "\",\"password\":\"");
    strcat(login_body, login_password);
    strcat(login_body, "\"}");

    url = bridge_join(client->base_url, "/v1/login/app-password");
    if (url == NULL) {
        bridge_login_wipe();
        return WF_ERR_ALLOC;
    }
    status = wf_http_post(client->xrpc, url, "application/json", login_body,
                          NULL, 0, &raw);
    free(url);
    bridge_login_wipe();

    if (status != WF_OK || raw.status != 200) {
        if (reason != NULL)
            *reason = bridge_login_reason(raw.body);
        wf_response_free(&raw);
        return status != WF_OK ? status : WF_ERR_HTTP;
    }
    if (raw.body == NULL) {
        wf_response_free(&raw);
        return WF_ERR_PARSE;
    }
    status = bridge_read_pairing(raw.body, out);
    wf_response_free(&raw);
    return status;
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

long platinum_bridge_query_escape(const char *value, char *out, long capacity)
{
    static const char hex[] = "0123456789ABCDEF";
    long length = 0;
    unsigned char c;

    if (out == NULL || capacity <= 0)
        return -1;
    out[0] = '\0';
    if (value == NULL)
        return 0;

    for (; *value != '\0'; ++value) {
        c = (unsigned char)*value;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
            c == '~') {
            if (length + 1 >= capacity) {
                out[0] = '\0';
                return -1;
            }
            out[length++] = (char)c;
        } else {
            if (length + 3 >= capacity) {
                out[0] = '\0';
                return -1;
            }
            out[length++] = '%';
            out[length++] = hex[c >> 4];
            out[length++] = hex[c & 0x0F];
        }
    }
    out[length] = '\0';
    return length;
}

wf_status platinum_bridge_mark_seen(platinum_bridge_client *client,
                                    const char *seen_at)
{
    char escaped[160];
    char body[192];
    wf_response response;
    wf_status status;

    if (client == NULL || seen_at == NULL || seen_at[0] == '\0' ||
        strlen(seen_at) > 64)
        return WF_ERR_INVALID_ARG;
    if (platinum_json_escape(escaped, sizeof(escaped), seen_at) != WF_OK)
        return WF_ERR_INVALID_ARG;

    strcpy(body, "{\"seenAt\":\"");
    strcat(body, escaped);
    strcat(body, "\"}");

    memset(&response, 0, sizeof(response));
    status = platinum_bridge_post(client, "/v1/notifications/seen", body,
                                  &response);
    wf_response_free(&response);
    return status;
}

wf_status platinum_bridge_delete_post(platinum_bridge_client *client,
                                      const char *uri)
{
    char escaped[520 * 6 + 1];
    char body[520 * 6 + 32];
    wf_response response;
    wf_status status;

    if (client == NULL || uri == NULL || uri[0] == '\0' || strlen(uri) > 512)
        return WF_ERR_INVALID_ARG;
    if (platinum_json_escape(escaped, sizeof(escaped), uri) != WF_OK)
        return WF_ERR_INVALID_ARG;
    strcpy(body, "{\"uri\":\"");
    strcat(body, escaped);
    strcat(body, "\"}");

    memset(&response, 0, sizeof(response));
    status = platinum_bridge_post(client, "/v1/post/delete", body, &response);
    wf_response_free(&response);
    return status;
}

int platinum_post_uri_is_in_repo(const char *uri, const char *did)
{
    size_t length;

    if (uri == NULL || did == NULL || did[0] == '\0' ||
        strncmp(uri, "at://", 5) != 0)
        return 0;
    length = strlen(did);
    return strncmp(uri + 5, did, length) == 0 && uri[5 + length] == '/';
}

wf_status platinum_bridge_set_muted_word(platinum_bridge_client *client,
                                         const char *utf8_word, int on)
{
    char escaped[PLATINUM_MUTED_WORD_BYTES * 2 + 8];
    char body[PLATINUM_MUTED_WORD_BYTES * 2 + 48];
    wf_response response;
    wf_status status;

    if (client == NULL || utf8_word == NULL || utf8_word[0] == '\0' ||
        strlen(utf8_word) > PLATINUM_MUTED_WORD_BYTES)
        return WF_ERR_INVALID_ARG;
    if (platinum_json_escape(escaped, sizeof(escaped), utf8_word) != WF_OK)
        return WF_ERR_INVALID_ARG;

    strcpy(body, "{\"value\":\"");
    strcat(body, escaped);
    strcat(body, on ? "\",\"on\":true}" : "\",\"on\":false}");

    memset(&response, 0, sizeof(response));
    status = platinum_bridge_post(client, "/v1/muted-words", body, &response);
    wf_response_free(&response);
    return status;
}

int platinum_bridge_reply_gate_failed(const char *response_body)
{
    platinum_json root;
    int applied;

    if (response_body == NULL ||
        platinum_json_open(&root, response_body) != WF_OK ||
        platinum_json_bool(root, "replyGateApplied", &applied) != WF_OK)
        return 0;
    return applied == 0;
}

static const char *const kGateNames[PLATINUM_REPLY_GATE_COUNT] = {
    "everyone", "nobody", "mentioned", "following", "followers"
};
static const char *const kGateLabels[PLATINUM_REPLY_GATE_COUNT] = {
    "Everyone", "Nobody", "People you mention", "People you follow",
    "Your followers"
};

const char *platinum_bridge_reply_gate_name(int index)
{
    return (index >= 0 && index < PLATINUM_REPLY_GATE_COUNT) ? kGateNames[index]
                                                              : NULL;
}

const char *platinum_bridge_reply_gate_label(int index)
{
    return (index >= 0 && index < PLATINUM_REPLY_GATE_COUNT) ? kGateLabels[index]
                                                              : NULL;
}

/* Append ,"key":{"uri":"...","cid":"..."}; 0 on failure. */
static int post_body_ref(char *body, char *scratch, size_t scratch_cap,
                         const char *key, const char *uri, const char *cid)
{
    strcat(body, ",\"");
    strcat(body, key);
    strcat(body, "\":{\"uri\":\"");
    if (platinum_json_escape(scratch, scratch_cap, uri) != WF_OK)
        return 0;
    strcat(body, scratch);
    strcat(body, "\",\"cid\":\"");
    if (platinum_json_escape(scratch, scratch_cap, cid) != WF_OK)
        return 0;
    strcat(body, scratch);
    strcat(body, "\"}");
    return 1;
}

char *platinum_bridge_post_body_ex(const char *utf8_text,
                                   const char *reply_uri,
                                   const char *reply_cid,
                                   const char *quote_uri,
                                   const char *quote_cid,
                                   int reply_gate)
{
    size_t text_cap;
    size_t id_cap;
    size_t body_cap;
    char *text_esc;
    char *scratch;
    char *body;
    int has_reply;
    int has_quote;

    if (utf8_text == NULL)
        return NULL;
    has_reply = reply_uri != NULL && reply_uri[0] != '\0';
    if (has_reply != (reply_cid != NULL && reply_cid[0] != '\0'))
        return NULL;
    has_quote = quote_uri != NULL && quote_uri[0] != '\0';
    if (has_quote != (quote_cid != NULL && quote_cid[0] != '\0'))
        return NULL;
    if (reply_gate < 0 || reply_gate >= PLATINUM_REPLY_GATE_COUNT ||
        (has_reply && reply_gate != 0))
        return NULL;

    id_cap = 1;
    if (has_reply && (strlen(reply_uri) + strlen(reply_cid)) * 6 + 2 > id_cap)
        id_cap = (strlen(reply_uri) + strlen(reply_cid)) * 6 + 2;
    if (has_quote && (strlen(quote_uri) + strlen(quote_cid)) * 6 + 2 > id_cap)
        id_cap = (strlen(quote_uri) + strlen(quote_cid)) * 6 + 2;
    text_cap = strlen(utf8_text) * 6 + 1;
    body_cap = text_cap + 2 * id_cap + 160;
    text_esc = (char *)malloc(text_cap);
    scratch = (char *)malloc(id_cap);
    body = (char *)malloc(body_cap);
    if (text_esc == NULL || scratch == NULL || body == NULL)
        goto fail;

    if (platinum_json_escape(text_esc, text_cap, utf8_text) != WF_OK)
        goto fail;
    strcpy(body, "{\"text\":\"");
    strcat(body, text_esc);
    strcat(body, "\"");
    if (has_reply &&
        !post_body_ref(body, scratch, id_cap, "replyTo", reply_uri, reply_cid))
        goto fail;
    if (has_quote &&
        !post_body_ref(body, scratch, id_cap, "quote", quote_uri, quote_cid))
        goto fail;
    if (reply_gate != 0) {
        strcat(body, ",\"replyGate\":\"");
        strcat(body, kGateNames[reply_gate]);
        strcat(body, "\"");
    }
    strcat(body, "}");
    free(text_esc);
    free(scratch);
    return body;

fail:
    free(text_esc);
    free(scratch);
    free(body);
    return NULL;
}

char *platinum_bridge_post_body(const char *utf8_text,
                                const char *reply_uri,
                                const char *reply_cid)
{
    return platinum_bridge_post_body_ex(utf8_text, reply_uri, reply_cid, NULL,
                                        NULL, 0);
}
