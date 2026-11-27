#ifndef PLATINUM_DIAG_H
#define PLATINUM_DIAG_H

#include "atproto_client.h"
#include "bridge_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_DIAG_URL_MAX 255
#define PLATINUM_DIAG_HANDLE_MAX 64
#define PLATINUM_DIAG_DID_MAX 255
#define PLATINUM_DIAG_LINES 8
#define PLATINUM_DIAG_LINE_MAX 200

/*
 * What the Connection Status window shows. It is a snapshot taken by
 * platinum_diag_check from up to two sources: the paired bridge's /health and
 * the native session's own service (describeServer). Nothing secret is ever
 * put in it: whether a token is held, never the token.
 */
typedef struct platinum_diag {
    int checked;
    char url[PLATINUM_DIAG_URL_MAX + 1];
    int has_token;
    wf_status health_status; /* the transport result for GET /health */
    long health_http;        /* its HTTP status, 0 if none */
    int health_ok;           /* a well-formed {"ok":true,...} */
    char service[48];
    char version[32];
    int has_session;         /* a native session was there to ask */
    char session_url[PLATINUM_DIAG_URL_MAX + 1];       /* its service */
    char session_handle[PLATINUM_DIAG_HANDLE_MAX + 1]; /* clipped for display */
    wf_status server_status; /* the result of describeServer */
    long server_http;        /* its HTTP status, 0 if none */
    int server_ok;           /* an answer carrying the service's own DID */
    char server_did[PLATINUM_DIAG_DID_MAX + 1];
    wf_status last_error;    /* the most recent failure of any service call */
    long free_heap;          /* bytes, or -1 if unknown */
} platinum_diag;

void platinum_diag_init(platinum_diag *diag);

/*
 * Ask each configured source and record the answers along with what the
 * caller knows. `bridge` may be NULL (no bridge paired): the bridge line says
 * so and /health is not asked. `client` may be NULL (no native session): the
 * service lines say so and describeServer is not asked. With both NULL nothing
 * is sent and the result is WF_ERR_CONFIG. Otherwise the result is the first
 * failure among the checks made, or WF_OK.
 */
wf_status platinum_diag_check(platinum_diag *diag,
                              platinum_bridge_client *bridge,
                              const char *url, int has_token,
                              platinum_atproto_client *client,
                              wf_status last_error, long free_heap);

/* A short plain description of a status ("no network answer", "refused by the
 * bridge", ...), never an empty string. */
const char *platinum_status_text(wf_status status);

/* The window's lines, one fixed sentence each: bridge, signed in, bridge
 * health, service, service check, last error, memory, and whether it has been
 * checked. Returns how many were written (PLATINUM_DIAG_LINES). */
int platinum_diag_lines(const platinum_diag *diag,
                        char lines[PLATINUM_DIAG_LINES][PLATINUM_DIAG_LINE_MAX]);

#ifdef __cplusplus
}
#endif

#endif
