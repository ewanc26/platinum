#ifndef PLATINUM_CONFIG_H
#define PLATINUM_CONFIG_H

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_CONFIG_BRIDGE_URL_MAX 255
#define PLATINUM_CONFIG_TOKEN_MAX 255
#define PLATINUM_CONFIG_DID_MAX 255
#define PLATINUM_CONFIG_INSTALLATION_ID_MAX 127

typedef struct platinum_config {
    char bridge_url[PLATINUM_CONFIG_BRIDGE_URL_MAX + 1];
    char bridge_token[PLATINUM_CONFIG_TOKEN_MAX + 1];
    char did[PLATINUM_CONFIG_DID_MAX + 1];
    char installation_id[PLATINUM_CONFIG_INSTALLATION_ID_MAX + 1];
} platinum_config;

void platinum_config_init(platinum_config *config);
int platinum_config_is_paired(const platinum_config *config);
OSErr platinum_config_load(platinum_config *config);
OSErr platinum_config_save(const platinum_config *config);
OSErr platinum_config_clear(void);

OSErr platinum_config_set_bridge_url(platinum_config *config,
                                     const char *value);
OSErr platinum_config_set_token(platinum_config *config,
                                const char *value);
OSErr platinum_config_set_did(platinum_config *config,
                              const char *value);
OSErr platinum_config_set_installation_id(platinum_config *config,
                                           const char *value);

#ifdef __cplusplus
}
#endif

#endif
