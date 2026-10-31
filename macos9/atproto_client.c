#include "atproto_client.h"

#include <stdlib.h>
#include <string.h>

#include "atproto_url.h"
#include "json_min.h"

#define PLATINUM_ATPROTO_CLIENT_SCRATCH 2048

struct platinum_atproto_client {
    wf_xrpc_client *xrpc;
    platinum_atproto_session session;
    char base[PLATINUM_ATPROTO_URL_MAX];
    platinum_atproto_save_fn save;
    void *save_userdata;
};

static char sign_in_scratch[PLATINUM_ATPROTO_CLIENT_SCRATCH];

static void client_wipe(void *data, size_t length)
{
    volatile unsigned char *scan;

    scan = (volatile unsigned char *)data;
    while (length > 0) {
        *scan = 0;
        scan++;
        length--;
    }
}

int platinum_atproto_client_scratch_clear(void)
{
    size_t index;

    for (index = 0; index < sizeof(sign_in_scratch); index++) {
        if (sign_in_scratch[index] != 0)
            return 0;
    }
    return 1;
}

platinum_atproto_client *platinum_atproto_client_new(const char *pds_url)
{
    platinum_atproto_client *client;
    wf_status status;

    client = (platinum_atproto_client *)malloc(sizeof(*client));
    if (client == NULL)
        return NULL;
    memset(client, 0, sizeof(*client));

    status = platinum_atproto_service_base(pds_url, client->base,
                                           sizeof(client->base));
    if (status != WF_OK) {
        free(client);
        return NULL;
    }

    client->xrpc = wf_xrpc_client_new(client->base);
    if (client->xrpc == NULL) {
        free(client);
        return NULL;
    }
    /* A page is bounded well below this; the cap is a second line of defence
     * against a hostile response filling a 1990s heap. */
    wf_xrpc_client_set_max_response_bytes(client->xrpc, 262144);
    return client;
}

void platinum_atproto_client_free(platinum_atproto_client *client)
{
    if (client == NULL)
        return;
    platinum_atproto_session_clear(&client->session);
    wf_xrpc_client_free(client->xrpc);
    client_wipe(client, sizeof(*client));
    free(client);
}

wf_status platinum_atproto_client_set_session(
    platinum_atproto_client *client, const platinum_atproto_session *session)
{
    if (client == NULL || session == NULL)
        return WF_ERR_INVALID_ARG;
    if (session->pds_url[0] == '\0' ||
        platinum_atproto_service_base(session->pds_url, client->base,
                                      sizeof(client->base)) != WF_OK)
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_did(session->did) ||
        !platinum_atproto_valid_handle(session->handle))
        return WF_ERR_INVALID_ARG;

    platinum_atproto_session_clear(&client->session);
    client->session = *session;
    wf_xrpc_client_set_auth(client->xrpc, client->session.access_jwt);
    return WF_OK;
}

const platinum_atproto_session *platinum_atproto_client_session(
    const platinum_atproto_client *client)
{
    if (client == NULL)
        return NULL;
    return &client->session;
}

void platinum_atproto_client_set_save(platinum_atproto_client *client,
                                      platinum_atproto_save_fn save,
                                      void *userdata)
{
    if (client == NULL)
        return;
    client->save = save;
    client->save_userdata = userdata;
}

static int client_auth_failure(wf_status status, const wf_response *response)
{
    if (status == WF_ERR_AUTH)
        return 1;
    if (status != WF_ERR_HTTP || response == NULL)
        return 0;
    if (response->status == 401 || response->status == 403)
        return 1;
    return platinum_atproto_error_status(response->body) == WF_ERR_AUTH;
}

static wf_status client_refresh_once(platinum_atproto_client *client)
{
    char url[PLATINUM_ATPROTO_URL_MAX];
    static platinum_atproto_session previous;
    wf_response response;
    wf_status status;
    wf_status refresh_status;

    if (client->session.refresh_jwt[0] == '\0')
        return WF_ERR_AUTH;

    status = platinum_atproto_xrpc_url(client->base,
                                       "com.atproto.server.refreshSession",
                                       NULL, NULL, 0, url, sizeof(url));
    if (status != WF_OK)
        return status;

    previous = client->session;

    memset(&response, 0, sizeof(response));
    wf_xrpc_client_set_auth(client->xrpc, client->session.refresh_jwt);
    status = wf_http_post(client->xrpc, url, "application/json", NULL, NULL, 0,
                          &response);

    refresh_status = platinum_atproto_session_parse_refresh(
        response.body, &previous, &client->session);
    wf_response_free(&response);

    if (status != WF_OK || refresh_status != WF_OK) {
        platinum_atproto_session_clear(&client->session);
        platinum_atproto_session_clear(&previous);
        wf_xrpc_client_set_auth(client->xrpc, NULL);
        return WF_ERR_AUTH;
    }

    platinum_atproto_session_clear(&previous);
    wf_xrpc_client_set_auth(client->xrpc, client->session.access_jwt);
    if (client->save != NULL)
        (void)client->save(&client->session, client->save_userdata);
    return WF_OK;
}

wf_status platinum_atproto_client_refresh(platinum_atproto_client *client)
{
    wf_status status;

    if (client == NULL)
        return WF_ERR_INVALID_ARG;
    status = client_refresh_once(client);
    if (status != WF_OK)
        platinum_atproto_session_clear(&client->session);
    return status;
}

static wf_status client_request(platinum_atproto_client *client, int post,
                                const char *nsid, const char *const *names,
                                const char *const *values, size_t count,
                                const char *body, wf_response *out)
{
    char url[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;
    int attempt;

    if (client == NULL || nsid == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;
    if (client->session.access_jwt[0] == '\0')
        return WF_ERR_AUTH;

    status = platinum_atproto_xrpc_url(client->base, nsid, names, values, count,
                                       url, sizeof(url));
    if (status != WF_OK)
        return status;

    for (attempt = 0; attempt < 2; attempt++) {
        wf_xrpc_client_set_auth(client->xrpc, client->session.access_jwt);
        if (post)
            status = wf_http_post(client->xrpc, url, "application/json", body,
                                  NULL, 0, out);
        else
            status = wf_http_get(client->xrpc, url, out);

        if (status == WF_OK)
            return WF_OK;
        if (attempt == 1 || !client_auth_failure(status, out))
            return status;

        wf_response_free(out);
        status = client_refresh_once(client);
        if (status != WF_OK)
            return WF_ERR_AUTH;
    }

    return WF_ERR_AUTH;
}

wf_status platinum_atproto_client_get(platinum_atproto_client *client,
                                      const char *nsid,
                                      const char *const *names,
                                      const char *const *values, size_t count,
                                      wf_response *out)
{
    return client_request(client, 0, nsid, names, values, count, NULL, out);
}

wf_status platinum_atproto_client_post(platinum_atproto_client *client,
                                       const char *nsid, const char *json_body,
                                       wf_response *out)
{
    if (json_body == NULL || platinum_json_valid(json_body) != WF_OK)
        return WF_ERR_PARSE;
    return client_request(client, 1, nsid, NULL, NULL, 0, json_body, out);
}

wf_status platinum_atproto_client_sign_in(platinum_atproto_client *client,
                                          const char *identifier,
                                          const char *password)
{
    char escaped_identifier[512];
    char escaped_password[512];
    char url[PLATINUM_ATPROTO_URL_MAX];
    wf_response response;
    wf_status status;

    if (client == NULL || identifier == NULL || password == NULL)
        return WF_ERR_INVALID_ARG;
    if (identifier[0] == '\0' || password[0] == '\0')
        return WF_ERR_INVALID_ARG;

    status = platinum_json_escape(escaped_identifier,
                                  sizeof(escaped_identifier), identifier);
    if (status != WF_OK)
        return WF_ERR_INVALID_ARG;
    status = platinum_json_escape(escaped_password,
                                  sizeof(escaped_password), password);
    if (status != WF_OK)
        return WF_ERR_INVALID_ARG;

    if (strlen(escaped_identifier) + strlen(escaped_password) + 48 >=
        sizeof(sign_in_scratch)) {
        client_wipe(escaped_identifier, sizeof(escaped_identifier));
        client_wipe(escaped_password, sizeof(escaped_password));
        return WF_ERR_INVALID_ARG;
    }

    strcpy(sign_in_scratch, "{\"identifier\":\"");
    strcat(sign_in_scratch, escaped_identifier);
    strcat(sign_in_scratch, "\",\"password\":\"");
    strcat(sign_in_scratch, escaped_password);
    strcat(sign_in_scratch, "\"}");
    client_wipe(escaped_identifier, sizeof(escaped_identifier));
    client_wipe(escaped_password, sizeof(escaped_password));

    status = platinum_atproto_xrpc_url(client->base,
                                       "com.atproto.server.createSession", NULL,
                                       NULL, 0, url, sizeof(url));
    if (status != WF_OK) {
        client_wipe(sign_in_scratch, sizeof(sign_in_scratch));
        return status;
    }

    memset(&response, 0, sizeof(response));
    wf_xrpc_client_set_auth(client->xrpc, NULL);
    status = wf_http_post(client->xrpc, url, "application/json",
                          sign_in_scratch, NULL, 0, &response);
    client_wipe(sign_in_scratch, sizeof(sign_in_scratch));

    if (status != WF_OK) {
        if (client_auth_failure(status, &response))
            status = WF_ERR_AUTH;
        else if (status == WF_ERR_HTTP)
            status = platinum_atproto_error_status(response.body);
        wf_response_free(&response);
        platinum_atproto_session_clear(&client->session);
        return status;
    }

    status = platinum_atproto_session_parse_create(response.body, client->base,
                                                   &client->session);
    wf_response_free(&response);
    if (status != WF_OK) {
        platinum_atproto_session_clear(&client->session);
        return status;
    }

    wf_xrpc_client_set_auth(client->xrpc, client->session.access_jwt);
    if (client->save != NULL)
        (void)client->save(&client->session, client->save_userdata);
    return WF_OK;
}
