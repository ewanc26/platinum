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

wf_status platinum_diag_check(platinum_diag *diag,
                              platinum_bridge_client *bridge,
                              const char *url, int has_token,
                              wf_status last_error, long free_heap)
{
    wf_response response;
    platinum_json root;
    int ok;
    wf_status status;

    if (diag == NULL)
        return WF_ERR_INVALID_ARG;
    platinum_diag_init(diag);
    diag->checked = 1;
    diag_copy(diag->url, sizeof(diag->url), url);
    diag->has_token = has_token != 0;
    diag->last_error = last_error;
    diag->free_heap = free_heap;

    if (bridge == NULL) {
        diag->health_status = WF_ERR_CONFIG;
        return WF_ERR_CONFIG;
    }

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

    diag_copy(lines[1], PLATINUM_DIAG_LINE_MAX,
              diag->has_token ? "Signed in: yes (a token is held; it is never shown)"
                              : "Signed in: no");

    if (!diag->checked) {
        diag_copy(lines[2], PLATINUM_DIAG_LINE_MAX, "Bridge health: not checked yet");
    } else if (diag->health_ok) {
        sprintf(text, "Bridge health: OK, %.40s %.30s",
                diag->service[0] != '\0' ? diag->service : "bridge",
                diag->version[0] != '\0' ? diag->version : "(no version)");
        diag_copy(lines[2], PLATINUM_DIAG_LINE_MAX, text);
    } else if (diag->health_status == WF_ERR_CONFIG) {
        diag_copy(lines[2], PLATINUM_DIAG_LINE_MAX, "Bridge health: no bridge is configured");
    } else if (diag->health_status == WF_OK) {
        diag_copy(lines[2], PLATINUM_DIAG_LINE_MAX,
                  "Bridge health: it answered, but not like a Platinum bridge");
    } else if (diag->health_status == WF_ERR_HTTP) {
        sprintf(text, "Bridge health: HTTP %ld", diag->health_http);
        diag_copy(lines[2], PLATINUM_DIAG_LINE_MAX, text);
    } else {
        sprintf(text, "Bridge health: failed (%.150s)",
                platinum_status_text(diag->health_status));
        diag_copy(lines[2], PLATINUM_DIAG_LINE_MAX, text);
    }

    sprintf(text, "Last error: %.170s",
            diag->last_error == WF_OK ? "none since I started"
                                      : platinum_status_text(diag->last_error));
    diag_copy(lines[3], PLATINUM_DIAG_LINE_MAX, text);

    if (diag->free_heap >= 0)
        sprintf(text, "Free memory: %ld KB", diag->free_heap / 1024);
    else
        strcpy(text, "Free memory: unknown");
    diag_copy(lines[4], PLATINUM_DIAG_LINE_MAX, text);

    diag_copy(lines[5], PLATINUM_DIAG_LINE_MAX,
              diag->checked ? "Checked when this window opened or you chose Check Again."
                            : "Choose Check Again to ask the bridge.");
    return PLATINUM_DIAG_LINES;
}
