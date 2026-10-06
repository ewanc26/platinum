#ifndef PLATINUM_DIAG_H
#define PLATINUM_DIAG_H

#include "bridge_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_DIAG_URL_MAX 255
#define PLATINUM_DIAG_LINES 6
#define PLATINUM_DIAG_LINE_MAX 200

/*
 * What the Connection Status window shows. It is a snapshot taken by
 * platinum_diag_check. Nothing secret is ever put in it: whether a token is
 * held, never the token.
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
    wf_status last_error;    /* the most recent failure of any bridge call */
    long free_heap;          /* bytes, or -1 if unknown */
} platinum_diag;

void platinum_diag_init(platinum_diag *diag);

/*
 * Ask the bridge for /health (no sign-in needed) and record the answer along
 * with what the caller knows. `bridge` may be NULL (no bridge configured), in
 * which case the health line says so. Returns the transport status.
 */
wf_status platinum_diag_check(platinum_diag *diag,
                              platinum_bridge_client *bridge,
                              const char *url, int has_token,
                              wf_status last_error, long free_heap);

/* A short plain description of a status ("no network answer", "refused by the
 * bridge", ...), never an empty string. */
const char *platinum_status_text(wf_status status);

/* The window's lines, one fixed sentence each: bridge, signed in, health, last
 * error, memory, and whether it has been checked. Returns how many were written
 * (PLATINUM_DIAG_LINES). */
int platinum_diag_lines(const platinum_diag *diag,
                        char lines[PLATINUM_DIAG_LINES][PLATINUM_DIAG_LINE_MAX]);

#ifdef __cplusplus
}
#endif

#endif
