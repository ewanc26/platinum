/*
 * diag.c -- the model behind the Connection Status window. No QuickDraw, so it
 * links on a host for tests; the window is diagwin.c.
 */
#include "diag.h"
#include "json_min.h"

#include <stdio.h>
#include <string.h>

static void diag_copy(char *out, long capacity, const char *value)
{
    long length;

    out[0] = '\0';
    if (value == NULL)
        return;
    length = (long)strlen(value);
    if (length >= capacity)
        length = capacity - 1;
    memcpy(out, value, (size_t)length);
    out[length] = '\0';
}

void platinum_diag_init(platinum_diag *diag)
{
    if (diag == NULL)
        return;
    memset(diag, 0, sizeof(*diag));
    diag->free_heap = -1;
}

const char *platinum_status_text(wf_status status)
{
    switch (status) {
        case WF_OK: return "no error";
        case WF_ERR_INVALID_ARG: return "a request was refused before it was sent";
        case WF_ERR_ALLOC: return "out of memory";
        case WF_ERR_NETWORK: return "no network answer";
        case WF_ERR_HTTP: return "the bridge answered with an error";
        case WF_ERR_PARSE: return "an answer could not be understood";
        case WF_ERR_NOT_FOUND: return "not found";
        case WF_ERR_WOULD_BLOCK: return "the network was not ready";
        case WF_ERR_CRYPTO: return "a security (TLS) step failed";
        case WF_ERR_VALIDATION: return "something did not validate";
        case WF_ERR_STATE: return "settings could not be saved";
        case WF_ERR_CONFIG: return "a setting is missing or wrong";
        case WF_ERR_TIMEOUT: return "the request timed out";
        case WF_ERR_UNSUPPORTED: return "the bridge speaks a version I do not";
        case WF_ERR_PERMISSION: return "not permitted";
        case WF_ERR_RATE_LIMIT: return "too many requests, try again later";
        case WF_ERR_AUTH: return "the sign-in was refused";
        default: return "an error I have no description for";
    }
}

static wf_status diag_check_bridge(platinum_diag *diag,
                                   platinum_bridge_client *bridge)
{
    wf_response response;
    platinum_json root;
    int ok;
    wf_status status;

    memset(&response, 0, sizeof(response));
    status = platinum_bridge_get(bridge, "/health", &response);
    diag->health_status = status;
    diag->health_http = response.status;
    if (status == WF_OK && response.body != NULL &&
        platinum_json_open(&root, response.body) == WF_OK &&
        platinum_json_bool(root, "ok", &ok) == WF_OK && ok) {
        diag->health_ok = 1;
        if (platinum_json_string_truncating(root, "service", diag->service,
                                            sizeof(diag->service)) != WF_OK)
            diag->service[0] = '\0';
        if (platinum_json_string_truncating(root, "version", diag->version,
                                            sizeof(diag->version)) != WF_OK)
            diag->version[0] = '\0';
    }
    wf_response_free(&response);
    return status;
}

/* The native session's own service. A well-formed answer names the service's
 * DID; anything else (an HTML page, a JSON object without one) is reported as
 * an answer that was not an AT Protocol service's. */
static wf_status diag_check_server(platinum_diag *diag,
                                   platinum_atproto_client *client)
{
    const platinum_atproto_session *session;
    wf_response response;
    platinum_json root;
    wf_status status;

    diag->has_session = 1;
    session = platinum_atproto_client_session(client);
    if (session != NULL) {
        diag_copy(diag->session_url, sizeof(diag->session_url), session->pds_url);
        diag_copy(diag->session_handle, sizeof(diag->session_handle),
                  session->handle);
    }

    memset(&response, 0, sizeof(response));
    status = platinum_atproto_client_describe_server(client, &response);
    diag->server_status = status;
    diag->server_http = response.status;
    if (status == WF_OK && response.body != NULL &&
        platinum_json_open(&root, response.body) == WF_OK &&
        platinum_json_string(root, "did", diag->server_did,
                             sizeof(diag->server_did)) == WF_OK &&
        strncmp(diag->server_did, "did:", 4) == 0)
        diag->server_ok = 1;
    else
        diag->server_did[0] = '\0';
    wf_response_free(&response);
    return status;
}

wf_status platinum_diag_check(platinum_diag *diag,
                              platinum_bridge_client *bridge,
                              const char *url, int has_token,
                              platinum_atproto_client *client,
                              wf_status last_error, long free_heap)
{
    wf_status result;
    wf_status status;

    if (diag == NULL)
        return WF_ERR_INVALID_ARG;
    platinum_diag_init(diag);
    diag->checked = 1;
    diag_copy(diag->url, sizeof(diag->url), url);
    diag->has_token = has_token != 0;
    diag->last_error = last_error;
    diag->free_heap = free_heap;
    diag->health_status = WF_ERR_CONFIG;
    diag->server_status = WF_ERR_CONFIG;

    if (bridge == NULL && client == NULL)
        return WF_ERR_CONFIG;

    result = WF_OK;
    if (bridge != NULL)
        result = diag_check_bridge(diag, bridge);
    if (client != NULL) {
        status = diag_check_server(diag, client);
        if (result == WF_OK)
            result = status;
    }
    return result;
}

static void diag_bridge_line(const platinum_diag *diag, char *line)
{
    char text[PLATINUM_DIAG_LINE_MAX];

    if (!diag->checked) {
        diag_copy(line, PLATINUM_DIAG_LINE_MAX, "Bridge health: not checked yet");
    } else if (diag->health_ok) {
        sprintf(text, "Bridge health: OK, %.40s %.30s",
                diag->service[0] != '\0' ? diag->service : "bridge",
                diag->version[0] != '\0' ? diag->version : "(no version)");
        diag_copy(line, PLATINUM_DIAG_LINE_MAX, text);
    } else if (diag->health_status == WF_ERR_CONFIG) {
        diag_copy(line, PLATINUM_DIAG_LINE_MAX, "Bridge health: no bridge is configured");
    } else if (diag->health_status == WF_OK) {
        diag_copy(line, PLATINUM_DIAG_LINE_MAX,
                  "Bridge health: it answered, but not like a Platinum bridge");
    } else if (diag->health_status == WF_ERR_HTTP) {
        sprintf(text, "Bridge health: HTTP %ld", diag->health_http);
        diag_copy(line, PLATINUM_DIAG_LINE_MAX, text);
    } else {
        sprintf(text, "Bridge health: failed (%.150s)",
                platinum_status_text(diag->health_status));
        diag_copy(line, PLATINUM_DIAG_LINE_MAX, text);
    }
}

static void diag_server_lines(const platinum_diag *diag, char *where,
                              char *answer)
{
    char text[PLATINUM_DIAG_LINE_MAX];

    if (!diag->checked) {
        diag_copy(where, PLATINUM_DIAG_LINE_MAX, "Service: not checked yet");
        diag_copy(answer, PLATINUM_DIAG_LINE_MAX, "Service check: not checked yet");
        return;
    }
    if (!diag->has_session) {
        diag_copy(where, PLATINUM_DIAG_LINE_MAX, "Service: none (not signed in natively)");
        diag_copy(answer, PLATINUM_DIAG_LINE_MAX, "Service check: nothing to ask");
        return;
    }

    sprintf(text, "Service: %.180s",
            diag->session_url[0] != '\0' ? diag->session_url : "(no address)");
    diag_copy(where, PLATINUM_DIAG_LINE_MAX, text);

    if (diag->server_ok) {
        sprintf(text, "Service check: OK, %.170s", diag->server_did);
    } else if (diag->server_status == WF_OK) {
        strcpy(text, "Service check: it answered, but not like an AT Protocol service");
    } else if (diag->server_http >= 400) {
        sprintf(text, "Service check: HTTP %ld (%.150s)", diag->server_http,
                platinum_status_text(diag->server_status));
    } else {
        sprintf(text, "Service check: failed (%.150s)",
                platinum_status_text(diag->server_status));
    }
    diag_copy(answer, PLATINUM_DIAG_LINE_MAX, text);
}

int platinum_diag_lines(const platinum_diag *diag,
                        char lines[PLATINUM_DIAG_LINES][PLATINUM_DIAG_LINE_MAX])
{
    char text[PLATINUM_DIAG_LINE_MAX];
    int i;

    for (i = 0; i < PLATINUM_DIAG_LINES; ++i)
        lines[i][0] = '\0';
    if (diag == NULL)
        return PLATINUM_DIAG_LINES;

    sprintf(text, "Bridge: %.180s", diag->url[0] != '\0' ? diag->url : "none set");
    diag_copy(lines[0], PLATINUM_DIAG_LINE_MAX, text);

    if (diag->has_session && diag->session_handle[0] != '\0') {
        sprintf(text, "Signed in: yes, as %.64s (tokens are never shown)",
                diag->session_handle);
        diag_copy(lines[1], PLATINUM_DIAG_LINE_MAX, text);
    } else {
        diag_copy(lines[1], PLATINUM_DIAG_LINE_MAX,
                  diag->has_token || diag->has_session
                      ? "Signed in: yes (a token is held; it is never shown)"
                      : "Signed in: no");
    }

    diag_bridge_line(diag, lines[2]);
    diag_server_lines(diag, lines[3], lines[4]);

    sprintf(text, "Last error: %.170s",
            diag->last_error == WF_OK ? "none since I started"
                                      : platinum_status_text(diag->last_error));
    diag_copy(lines[5], PLATINUM_DIAG_LINE_MAX, text);

    if (diag->free_heap >= 0)
        sprintf(text, "Free memory: %ld KB", diag->free_heap / 1024);
    else
        strcpy(text, "Free memory: unknown");
    diag_copy(lines[6], PLATINUM_DIAG_LINE_MAX, text);

    if (!diag->checked)
        strcpy(text, "Choose Check Again to ask the bridge and your service.");
    else if (!diag->has_session && diag->health_status == WF_ERR_CONFIG)
        strcpy(text, "Nothing to ask: pair a bridge or sign in first.");
    else
        strcpy(text, "Checked when this window opened or you chose Check Again.");
    diag_copy(lines[7], PLATINUM_DIAG_LINE_MAX, text);
    return PLATINUM_DIAG_LINES;
}
