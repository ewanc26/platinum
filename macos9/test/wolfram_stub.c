#include <stdlib.h>
#include <string.h>

#include "wolfram/xrpc.h"
#include "wolfram/macos9_tls.h"
#include "wolfram_stub.h"

/* ------------------------------------------------------------------ */
/* Recorded calls                                                      */
/* ------------------------------------------------------------------ */

char last_url[512];
char last_body[512];
char last_auth[256];
char last_nsid[128];
size_t last_param_count;
char last_param_names[8][64];
char last_param_values[8][512];
int auth_set;
size_t last_max_response_bytes;
int last_method; /* 0 = none, 1 = GET, 2 = POST */

/* What the next transport call should do. */
int fake_status;      /* wf_status to return */
long fake_http_status;/* HTTP status to put in the response */
const char *fake_body;/* response body to hand back */
size_t fake_body_len; /* see wolfram_stub.h */
int fake_call_count;
fake_script_fn fake_script;

static void run_script(int method, const char *url, const char *body)
{
    fake_call_count++;
    if (fake_script != NULL)
        fake_script(fake_call_count, method, url, body);
}

/* ------------------------------------------------------------------ */
/* Wolfram transport stubs                                             */
/* ------------------------------------------------------------------ */

static char *stub_strdup(const char *value)
{
    size_t len;
    char *copy;

    if (value == NULL)
        return NULL;
    len = strlen(value);
    copy = (char *)malloc(len + 1);
    if (copy != NULL)
        memcpy(copy, value, len + 1);
    return copy;
}

/*
 * The real client is opaque, so a one-byte stand-in is enough: the stubs
 * never dereference it.
 */
wf_xrpc_client *wf_xrpc_client_new(const char *service_base_url)
{
    static char handle;

    if (service_base_url == NULL || service_base_url[0] == '\0')
        return NULL;
    return (wf_xrpc_client *)&handle;
}

void wf_xrpc_client_free(wf_xrpc_client *client)
{
    (void)client;
}

void wf_xrpc_client_set_auth(wf_xrpc_client *client, const char *access_jwt)
{
    (void)client;
    auth_set = 1;
    last_auth[0] = '\0';
    if (access_jwt != NULL) {
        strncpy(last_auth, access_jwt, sizeof(last_auth) - 1);
        last_auth[sizeof(last_auth) - 1] = '\0';
    }
}

/*
 * The client caps every response at 256 KiB. The stub records the cap so a
 * test can assert the client still bounds its own reads.
 */
void wf_xrpc_client_set_max_response_bytes(wf_xrpc_client *client,
                                           size_t max_bytes)
{
    (void)client;
    last_max_response_bytes = max_bytes;
}

/*
 * The yield hook the Mac OS 9 transport pumps while blocking. The stub keeps
 * the last installed hook so a test can install and remove it; the CI
 * application build links this stub because Open Transport and macTLS are not
 * available there (issue 41).
 */
static wf_macos9_yield_fn last_yield_fn = NULL;
static void *last_yield_userdata = NULL;

static wf_xrpc_refresh_fn installed_refresh = NULL;
static void *installed_refresh_userdata = NULL;
static int refreshing;

wf_status wf_xrpc_client_set_base_url(wf_xrpc_client *client,
                                      const char *service_base_url)
{
    (void)client;
    (void)service_base_url;
    return WF_OK;
}

void wf_xrpc_client_set_refresh_handler(wf_xrpc_client *client,
                                        wf_xrpc_refresh_fn fn, void *userdata)
{
    (void)client;
    installed_refresh = fn;
    installed_refresh_userdata = userdata;
}

void wf_macos9_set_yield_callback(wf_macos9_yield_fn fn, void *userdata)
{
    last_yield_fn = fn;
    last_yield_userdata = userdata;
}

void wf_response_free(wf_response *res)
{
    if (res == NULL)
        return;
    free(res->body);
    free(res->dpop_nonce);
    free(res->set_cookie);
    free(res->location);
    memset(res, 0, sizeof(*res));
}

static void record_response(wf_response *out)
{
    /* The real transport populates every field, and wf_response_free is only
     * documented as safe on a zeroed struct. Leaving the optional header
     * pointers untouched here would make this stub free stack garbage rather
     * than expose a real defect. */
    memset(out, 0, sizeof(*out));
    out->status = fake_http_status;
    if (fake_body != NULL && fake_body_len > 0) {
        out->body = (char *)malloc(fake_body_len + 1);
        if (out->body != NULL) {
            memcpy(out->body, fake_body, fake_body_len);
            out->body[fake_body_len] = '\0';
            out->body_len = fake_body_len;
        }
    } else if (fake_body != NULL) {
        out->body = stub_strdup(fake_body);
        if (out->body != NULL)
            out->body_len = strlen(out->body);
    }
}

static void record_param(size_t index, const char *name, const char *value)
{
    if (index >= 8)
        return;
    last_param_names[index][0] = '\0';
    last_param_values[index][0] = '\0';
    if (name != NULL) {
        strncpy(last_param_names[index], name,
                sizeof(last_param_names[index]) - 1);
        last_param_names[index][sizeof(last_param_names[index]) - 1] = '\0';
    }
    if (value != NULL) {
        strncpy(last_param_values[index], value,
                sizeof(last_param_values[index]) - 1);
        last_param_values[index][sizeof(last_param_values[index]) - 1] = '\0';
    }
}

/*
 * Model the transport's one refresh-and-retry: on an authenticated 401, call
 * the installed handler once and, if it succeeds, issue the same request again.
 * The handler's own refresh procedure runs with `refreshing` set, so it cannot
 * recurse, exactly as macOS's xrpc_mac9.c guards it.
 */
static wf_status stub_request(int method, const char *nsid,
                              const wf_xrpc_param *params, size_t count,
                              const char *body, wf_response *out)
{
    size_t index;
    int attempt;
    wf_status status;

    last_method = method;
    last_nsid[0] = '\0';
    last_body[0] = '\0';
    last_param_count = count > 8 ? 8 : count;
    if (nsid != NULL) {
        strncpy(last_nsid, nsid, sizeof(last_nsid) - 1);
        last_nsid[sizeof(last_nsid) - 1] = '\0';
    }
    for (index = 0; index < last_param_count; index++)
        record_param(index, params[index].name, params[index].value);
    if (body != NULL) {
        strncpy(last_body, body, sizeof(last_body) - 1);
        last_body[sizeof(last_body) - 1] = '\0';
    }

    status = WF_OK;
    for (attempt = 0; attempt < 2; attempt++) {
        run_script(method, nsid, body);
        if (out != NULL)
            record_response(out);
        status = (wf_status)fake_status;

        if (attempt == 0 && status == WF_ERR_HTTP && fake_http_status == 401 &&
            auth_set && last_auth[0] != '\0' && installed_refresh != NULL &&
            !refreshing) {
            wf_status refreshed;

            refreshing = 1;
            refreshed = installed_refresh(installed_refresh_userdata);
            refreshing = 0;
            if (refreshed == WF_OK) {
                wf_response_free(out);
                continue;
            }
        }
        break;
    }
    return status;
}

wf_status wf_xrpc_query_params(wf_xrpc_client *client, const char *nsid,
                               const wf_xrpc_param *params, size_t count,
                               wf_response *out)
{
    (void)client;
    last_url[0] = '\0';
    return stub_request(1, nsid, params, count, NULL, out);
}

wf_status wf_xrpc_procedure(wf_xrpc_client *client, const char *nsid,
                            const char *body, wf_response *out)
{
    (void)client;
    last_url[0] = '\0';
    return stub_request(2, nsid, NULL, 0, body, out);
}

wf_status wf_http_get(wf_xrpc_client *client, const char *url,
                      wf_response *out)
{
    (void)client;
    last_method = 1;
    last_nsid[0] = '\0';
    last_param_count = 0;
    run_script(1, url, NULL);
    last_url[0] = '\0';
    if (url != NULL) {
        strncpy(last_url, url, sizeof(last_url) - 1);
        last_url[sizeof(last_url) - 1] = '\0';
    }
    if (out != NULL)
        record_response(out);
    return (wf_status)fake_status;
}

wf_status wf_http_get_limited(wf_xrpc_client *client, const char *url,
                              size_t max_bytes, wf_response *out)
{
    last_max_response_bytes = max_bytes;
    return wf_http_get(client, url, out);
}

wf_status wf_http_post(wf_xrpc_client *client, const char *url,
                       const char *content_type, const char *body,
                       const wf_http_header *headers, size_t header_count,
                       wf_response *out)
{
    (void)client;
    (void)content_type;
    (void)headers;
    (void)header_count;
    last_method = 2;
    last_nsid[0] = '\0';
    last_param_count = 0;
    run_script(2, url, body);
    last_url[0] = '\0';
    last_body[0] = '\0';
    if (url != NULL) {
        strncpy(last_url, url, sizeof(last_url) - 1);
        last_url[sizeof(last_url) - 1] = '\0';
    }
    if (body != NULL) {
        strncpy(last_body, body, sizeof(last_body) - 1);
        last_body[sizeof(last_body) - 1] = '\0';
    }
    if (out != NULL)
        record_response(out);
    return (wf_status)fake_status;
}

