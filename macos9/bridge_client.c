#include "bridge_client.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

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

    base_len = strlen(base);
    path_len = strlen(path);
    slash = (base_len != 0 && base[base_len - 1] != '/' &&
             path[0] != '/');

    url = (char *)malloc(base_len + path_len + (slash ? 2 : 1));
    if (url == NULL)
        return NULL;

    memcpy(url, base, base_len);
    if (slash)
        url[base_len++] = '/';
    memcpy(url + base_len, path, path_len);
    url[base_len + path_len] = '\0';
    return url;
}

static wf_status bridge_json_body(const char *json_body, wf_response *out)
{
    cJSON *json;
    if (json_body == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;

    json = cJSON_Parse(json_body);
    if (json == NULL)
        return WF_ERR_PARSE;
    cJSON_Delete(json);
    return WF_OK;
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

    client->base_url = wf_xrpc_get_base_url(client->xrpc);
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
    wf_xrpc_client_set_auth(client->xrpc, token);
    return WF_OK;
}

wf_status platinum_bridge_pair(platinum_bridge_client *client,
                               const char *code,
                               platinum_bridge_pairing *out)
{
    char *url;
    char *body;
    cJSON *request;
    cJSON *response;
    cJSON *item;
    wf_response raw;
    wf_status status;

    if (client == NULL || client->xrpc == NULL || code == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;

    memset(out, 0, sizeof(*out));
    memset(&raw, 0, sizeof(raw));

    request = cJSON_CreateObject();
    if (request == NULL)
        return WF_ERR_ALLOC;
    cJSON_AddStringToObject(request, "code", code);
    body = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    if (body == NULL)
        return WF_ERR_ALLOC;

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

    response = cJSON_Parse(raw.body ? raw.body : "");
    if (response == NULL) {
        wf_response_free(&raw);
        return WF_ERR_PARSE;
    }

    item = cJSON_GetObjectItemCaseSensitive(response, "protocol");
    if (!cJSON_IsNumber(item)) {
        cJSON_Delete(response);
        wf_response_free(&raw);
        return WF_ERR_PARSE;
    }
    out->protocol = item->valueint;

    item = cJSON_GetObjectItemCaseSensitive(response, "token");
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        cJSON_Delete(response);
        wf_response_free(&raw);
        platinum_bridge_pairing_free(out);
        return WF_ERR_PARSE;
    }
    out->token = bridge_strdup(item->valuestring);

    item = cJSON_GetObjectItemCaseSensitive(response, "did");
    if (!cJSON_IsString(item) || item->valuestring == NULL ||
        out->token == NULL) {
        cJSON_Delete(response);
        wf_response_free(&raw);
        platinum_bridge_pairing_free(out);
        return out->token == NULL ? WF_ERR_ALLOC : WF_ERR_PARSE;
    }
    out->did = bridge_strdup(item->valuestring);

    cJSON_Delete(response);
    wf_response_free(&raw);
    if (out->did == NULL) {
        platinum_bridge_pairing_free(out);
        return WF_ERR_ALLOC;
    }

    if (out->protocol != 1) {
        platinum_bridge_pairing_free(out);
        return WF_ERR_UNSUPPORTED;
    }

    return WF_OK;
}

void platinum_bridge_pairing_free(platinum_bridge_pairing *pairing)
{
    if (pairing == NULL)
        return;
    free(pairing->token);
    free(pairing->did);
    memset(pairing, 0, sizeof(*pairing));
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

    if (json_body != NULL) {
        status = bridge_json_body(json_body, out);
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
