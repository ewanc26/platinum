/* Configuration and health for the native bridge. C89. */

#ifndef PLATINUM_BRIDGE_CONFIG_H
#define PLATINUM_BRIDGE_CONFIG_H

#include <stddef.h>

#define PLATINUM_BRIDGE_VERSION "0.3.1"

typedef struct platinum_bridge_config {
    char host[64];
    long port;
    char base_url[256];
    char public_url[256];
    char data_dir[512];
    char version[32];
    long max_body_bytes;      /* 65536 */
    int allow_app_password;   /* 1 only when env value is exactly "1" */
} platinum_bridge_config;

/*
 * Loads the bridge configuration from parallel arrays of environment names and
 * values. A NULL value means the variable is unset. Returns 0 on success and -1
 * on an invalid port, a value that does not fit its buffer, or bad arguments.
 * On failure *out is left unchanged.
 */
int platinum_config_load(platinum_bridge_config *out, const char *const *env_names, const char *const *env_values, size_t env_count);

/*
 * Writes the /health body, {"ok":true,"service":"platinum-bridge","version":...},
 * into buf as a NUL-terminated string. Returns 0 on success and -1 if it does
 * not fit (including buf_size 0 or a NULL argument).
 */
int platinum_config_health_json(const platinum_bridge_config *cfg, char *buf, size_t buf_size);

#endif /* PLATINUM_BRIDGE_CONFIG_H */
