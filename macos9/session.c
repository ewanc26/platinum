#include "session.h"

#include <stdlib.h>
#include <string.h>

void platinum_session_init(platinum_session *session)
{
    if (session == NULL)
        return;

    memset(session, 0, sizeof(*session));
    platinum_config_init(&session->config);
}

void platinum_session_close(platinum_session *session)
{
    if (session == NULL)
        return;

    platinum_bridge_client_free(session->bridge);
    session->bridge = NULL;
    platinum_config_init(&session->config);
}

OSErr platinum_session_load(platinum_session *session)
{
    OSErr err;

    if (session == NULL)
        return paramErr;

    platinum_session_close(session);

    err = platinum_config_load(&session->config);
    if (err == fnfErr) {
        platinum_config_init(&session->config);
        return noErr;
    }
    if (err != noErr)
        return err;

    if (session->config.bridge_url[0] == '\0')
        return noErr;

    session->bridge = platinum_bridge_client_new(session->config.bridge_url);
    if (session->bridge == NULL) {
        platinum_config_init(&session->config);
        return memFullErr;
    }

    if (session->config.bridge_token[0] != '\0') {
        platinum_bridge_client_set_token(session->bridge,
                                         session->config.bridge_token);
    }

    return noErr;
}

OSErr platinum_session_set_bridge_url(platinum_session *session,
                                      const char *bridge_url)
{
    platinum_config updated;
    platinum_bridge_client *bridge;

    if (session == NULL || bridge_url == NULL)
        return paramErr;

    bridge = platinum_bridge_client_new(bridge_url);
    if (bridge == NULL)
        return memFullErr;

    updated = session->config;
    if (platinum_config_set_bridge_url(&updated, bridge_url) != noErr) {
        platinum_bridge_client_free(bridge);
        return paramErr;
    }

    if (platinum_config_save(&updated) != noErr) {
        platinum_bridge_client_free(bridge);
        return ioErr;
    }

    platinum_bridge_client_free(session->bridge);
    session->bridge = bridge;
    session->config = updated;
    return noErr;
}

wf_status platinum_session_pair(platinum_session *session,
                                const char *code)
{
    platinum_bridge_pairing pairing;
    platinum_config updated;
    wf_status status;

    if (session == NULL || session->bridge == NULL || code == NULL)
        return WF_ERR_INVALID_ARG;

    memset(&pairing, 0, sizeof(pairing));
    status = platinum_bridge_pair(session->bridge, code, &pairing);
    if (status != WF_OK)
        return status;

    updated = session->config;
    if (platinum_config_set_token(&updated, pairing.token) != noErr ||
        platinum_config_set_did(&updated, pairing.did) != noErr ||
        platinum_config_set_installation_id(&updated,
                                             pairing.installation_id) != noErr) {
        platinum_bridge_pairing_free(&pairing);
        return WF_ERR_VALIDATION;
    }

    if (platinum_config_save(&updated) != noErr) {
        platinum_bridge_pairing_free(&pairing);
        return WF_ERR_NETWORK;
    }

    status = platinum_bridge_client_set_token(session->bridge, pairing.token);
    platinum_bridge_pairing_free(&pairing);
    if (status != WF_OK)
        return status;

    session->config = updated;
    return WF_OK;
}

wf_status platinum_session_sign_out(platinum_session *session)
{
    wf_status revoke_status;
    OSErr clear_status;
    wf_response response;

    if (session == NULL)
        return WF_ERR_INVALID_ARG;

    revoke_status = WF_OK;
    if (session->bridge != NULL && session->config.bridge_token[0] != '\0')
        revoke_status = platinum_bridge_revoke(session->bridge, NULL);

    clear_status = platinum_config_clear();

    platinum_bridge_client_free(session->bridge);
    session->bridge = NULL;
    platinum_config_init(&session->config);

    if (clear_status != noErr)
        return WF_ERR_NETWORK;

    return revoke_status;
}

int platinum_session_is_paired(const platinum_session *session)
{
    if (session == NULL)
        return 0;

    return platinum_config_is_paired(&session->config);
}

const platinum_config *platinum_session_config(const platinum_session *session)
{
    if (session == NULL)
        return NULL;

    return &session->config;
}

platinum_bridge_client *platinum_session_bridge(platinum_session *session)
{
    if (session == NULL)
        return NULL;

    return session->bridge;
}
