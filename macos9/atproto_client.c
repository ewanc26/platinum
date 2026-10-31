#include "atproto_client.h"

#include <stdlib.h>
#include <string.h>

#include "atproto_bootstrap.h"
#include "atproto_url.h"
#include "json_min.h"

#define PLATINUM_ATPROTO_CLIENT_SCRATCH 2048

struct platinum_atproto_client {
    wf_xrpc_client *xrpc;
    platinum_atproto_session session;
    char base[PLATINUM_ATPROTO_URL_MAX];
    platinum_atproto_save_fn save;
    void *save_userdata;
    int refreshing;
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

static wf_status client_refresh_once(platinum_atproto_client *client)
{
    static platinum_atproto_session previous;
    wf_response response;
    wf_status status;
    wf_status refresh_status;

    if (client->refreshing || client->session.refresh_jwt[0] == '\0')
        return WF_ERR_AUTH;
    client->refreshing = 1;

    previous = client->session;

    memset(&response, 0, sizeof(response));
    wf_xrpc_client_set_auth(client->xrpc, client->session.refresh_jwt);
    status = wf_xrpc_procedure(client->xrpc,
                               "com.atproto.server.refreshSession", NULL,
                               &response);

    refresh_status = platinum_atproto_session_parse_refresh(
        response.body, &previous, &client->session);
    wf_response_free(&response);

    if (status != WF_OK || refresh_status != WF_OK) {
        platinum_atproto_session_clear(&client->session);
        platinum_atproto_session_clear(&previous);
        wf_xrpc_client_set_auth(client->xrpc, NULL);
        client->refreshing = 0;
        return WF_ERR_AUTH;
    }

    platinum_atproto_session_clear(&previous);
    client->refreshing = 0;
    wf_xrpc_client_set_auth(client->xrpc, client->session.access_jwt);
    if (client->save != NULL)
        (void)client->save(&client->session, client->save_userdata);
    return WF_OK;
}

/* Wolfram calls this after an authenticated request is rejected as expired and
 * retries the original request itself. The transport guards re-entrancy, so the
 * refresh request below cannot trigger a second refresh. */
static wf_status client_refresh_handler(void *userdata)
{
    return client_refresh_once((platinum_atproto_client *)userdata);
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
    wf_xrpc_client_set_refresh_handler(client->xrpc, client_refresh_handler,
                                       client);
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
    if (wf_xrpc_client_set_base_url(client->xrpc, client->base) != WF_OK)
        return WF_ERR_ALLOC;

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

/*
 * Map the transport's status to what the caller needs. Wolfram has already
 * refreshed and retried once by the time a 401 reaches here; a body naming an
 * expired or invalid token means the session is gone, not merely that this
 * request failed. The upstream message is never copied.
 */
static wf_status client_map(wf_status status, const wf_response *response)
{
    if (status != WF_ERR_HTTP || response == NULL)
        return status;
    if (response->status == 401 || response->status == 403)
        return WF_ERR_AUTH;
    return platinum_atproto_error_status(response->body);
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

wf_status platinum_atproto_client_get(platinum_atproto_client *client,
                                      const char *nsid,
                                      const char *const *names,
                                      const char *const *values, size_t count,
                                      wf_response *out)
{
    wf_xrpc_param params[PLATINUM_ATPROTO_PARAMS_MAX];
    size_t index;

    if (client == NULL || nsid == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;
    if (client->session.access_jwt[0] == '\0')
        return WF_ERR_AUTH;
    if (count > PLATINUM_ATPROTO_PARAMS_MAX ||
        (count > 0 && (names == NULL || values == NULL)))
        return WF_ERR_INVALID_ARG;

    for (index = 0; index < count; index++) {
        if (names[index] == NULL || values[index] == NULL)
            return WF_ERR_INVALID_ARG;
        params[index].name = names[index];
        params[index].value = values[index];
    }

    return client_map(wf_xrpc_query_params(client->xrpc, nsid, params, count,
                                           out),
                      out);
}

wf_status platinum_atproto_client_post(platinum_atproto_client *client,
                                       const char *nsid, const char *json_body,
                                       wf_response *out)
{
    if (client == NULL || nsid == NULL || out == NULL)
        return WF_ERR_INVALID_ARG;
    if (client->session.access_jwt[0] == '\0')
        return WF_ERR_AUTH;
    if (json_body == NULL || platinum_json_valid(json_body) != WF_OK)
        return WF_ERR_PARSE;

    return client_map(wf_xrpc_procedure(client->xrpc, nsid, json_body, out),
                      out);
}

wf_status platinum_atproto_client_sign_in(platinum_atproto_client *client,
                                          const char *identifier,
                                          const char *password)
{
    char escaped_identifier[512];
    char escaped_password[512];
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

    memset(&response, 0, sizeof(response));
    wf_xrpc_client_set_auth(client->xrpc, NULL);
    status = wf_xrpc_procedure(client->xrpc,
                               "com.atproto.server.createSession",
                               sign_in_scratch, &response);
    client_wipe(sign_in_scratch, sizeof(sign_in_scratch));

    if (status != WF_OK) {
        status = client_map(status, &response);
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

/*
 * Bootstrap fetches public documents from several hosts. The caller's XRPC
 * client is only a transport here: wf_http_get_limited takes an absolute URL,
 * never carries the bearer token it may hold, and caps the body at the size
 * the caller asked for.
 */
static wf_status client_bootstrap_fetch(void *userdata, const char *url,
                                        size_t max_bytes, wf_response *out)
{
    return wf_http_get_limited((wf_xrpc_client *)userdata, url, max_bytes, out);
}

wf_status platinum_atproto_client_connect(
    const char *handle, const char *password, const char *fallback_service,
    platinum_atproto_save_fn save, void *userdata,
    platinum_atproto_client **out)
{
    char origin[PLATINUM_ATPROTO_URL_MAX];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_SESSION_PDS_MAX + 1];
    wf_xrpc_client *fetch;
    platinum_atproto_client *client;
    wf_status status;

    if (out == NULL)
        return WF_ERR_INVALID_ARG;
    *out = NULL;

    if (handle == NULL || password == NULL)
        return WF_ERR_INVALID_ARG;
    if (!platinum_atproto_valid_handle(handle))
        return WF_ERR_INVALID_ARG;

    /* The fetch client needs a base URL before the PDS is known; the handle's
     * own https origin is the host the first request goes to anyway. Every
     * bootstrap request carries its own absolute URL. */
    strcpy(origin, "https://");
    strcat(origin, handle);
    fetch = wf_xrpc_client_new(origin);
    if (fetch == NULL)
        return WF_ERR_ALLOC;

    status = platinum_atproto_bootstrap(client_bootstrap_fetch, fetch, handle,
                                        fallback_service, did, sizeof(did),
                                        pds, sizeof(pds));
    wf_xrpc_client_free(fetch);
    if (status != WF_OK)
        return status;

    client = platinum_atproto_client_new(pds);
    if (client == NULL)
        return WF_ERR_ALLOC;
    client->save = save;
    client->save_userdata = userdata;

    status = platinum_atproto_client_sign_in(client, handle, password);
    if (status != WF_OK) {
        platinum_atproto_client_free(client);
        return status;
    }

    *out = client;
    return WF_OK;
}
