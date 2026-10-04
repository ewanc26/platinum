#ifndef PLATINUM_BRIDGE_CLIENT_H
#define PLATINUM_BRIDGE_CLIENT_H

#include "wolfram/xrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platinum_bridge_client platinum_bridge_client;

typedef struct platinum_bridge_pairing {
    int protocol;
    char *token;
    char *did;
} platinum_bridge_pairing;

platinum_bridge_client *platinum_bridge_client_new(const char *base_url);
void platinum_bridge_client_free(platinum_bridge_client *client);

wf_status platinum_bridge_client_set_token(platinum_bridge_client *client,
                                           const char *token);

wf_status platinum_bridge_pair(platinum_bridge_client *client,
                               const char *code,
                               platinum_bridge_pairing *out);
void platinum_bridge_pairing_free(platinum_bridge_pairing *pairing);

wf_status platinum_bridge_get(platinum_bridge_client *client,
                              const char *path,
                              wf_response *out);

wf_status platinum_bridge_post(platinum_bridge_client *client,
                               const char *path,
                               const char *json_body,
                               wf_response *out);

wf_status platinum_bridge_revoke(platinum_bridge_client *client,
                                 wf_response *out);

#ifdef __cplusplus
}
#endif

#endif
