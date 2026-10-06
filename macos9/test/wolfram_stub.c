#include <stdlib.h>
#include <string.h>

#include "wolfram/xrpc.h"
#include "wolfram_stub.h"

/* ------------------------------------------------------------------ */
/* Recorded calls                                                      */
/* ------------------------------------------------------------------ */

char last_url[512];
char last_body[512];
char last_auth[256];
int auth_set;
size_t last_max_response_bytes;
int last_method; /* 0 = none, 1 = GET, 2 = POST */

/* What the next transport call should do. */
int fake_status;      /* wf_status to return */
long fake_http_status;/* HTTP status to put in the response */
const char *fake_body;/* response body to hand back */
size_t fake_body_len; /* see wolfram_stub.h */

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

wf_status wf_http_get(wf_xrpc_client *client, const char *url,
                      wf_response *out)
{
    (void)client;
    last_method = 1;
    last_url[0] = '\0';
    if (url != NULL) {
        strncpy(last_url, url, sizeof(last_url) - 1);
        last_url[sizeof(last_url) - 1] = '\0';
    }
    if (out != NULL)
        record_response(out);
    return (wf_status)fake_status;
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

