/* Configuration and health for the native bridge. C89. */

/* snprintf is C99; under -std=c89 it is only declared when POSIX 2001 is requested. */
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define PLATINUM_BRIDGE_DEFAULT_PORT 8787L
#define PLATINUM_BRIDGE_DEFAULT_HOST "127.0.0.1"
#define PLATINUM_BRIDGE_DEFAULT_DATA_DIR ".platinum-bridge"
#define PLATINUM_BRIDGE_DEFAULT_OAUTH_NODE "https://oauth.croft.click"
#define PLATINUM_BRIDGE_MAX_BODY_BYTES 65536L
#define PLATINUM_BRIDGE_SERVICE "platinum-bridge"

/* Returns the value for name, or NULL if it is unset. */
static const char *env_get(const char *const *env_names, const char *const *env_values, size_t env_count, const char *name)
{
    size_t i;

    for (i = 0; i < env_count; i++) {
        if (env_names[i] != NULL && strcmp(env_names[i], name) == 0) {
            return env_values[i];
        }
    }
    return NULL;
}

/* Copies src into dst. Returns -1 if src plus its NUL does not fit. */
static int copy_bounded(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    len = strlen(src);
    if (len >= dst_size) {
        return -1;
    }
    memcpy(dst, src, len + 1);
    return 0;
}

/* Parses a decimal TCP port in 1..65535 with the whole string consumed. */
static int parse_port(const char *s, long *out)
{
    char *end;
    long value;

    if (s[0] == '\0') {
        return -1;
    }
    value = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return -1;
    }
    if (value < 1 || value > 65535L) {
        return -1;
    }
    *out = value;
    return 0;
}

int platinum_config_load(platinum_bridge_config *out, const char *const *env_names, const char *const *env_values, size_t env_count)
{
    platinum_bridge_config cfg;
    const char *value;
    long port;
    int written;

    if (out == NULL) {
        return -1;
    }
    if (env_count > 0 && (env_names == NULL || env_values == NULL)) {
        return -1;
    }

    memset(&cfg, 0, sizeof cfg);

    value = env_get(env_names, env_values, env_count, "PLATINUM_BRIDGE_PORT");
    if (value == NULL) {
        port = PLATINUM_BRIDGE_DEFAULT_PORT;
    } else if (parse_port(value, &port) != 0) {
        return -1;
    }
    cfg.port = port;

    value = env_get(env_names, env_values, env_count, "PLATINUM_BRIDGE_HOST");
    if (copy_bounded(cfg.host, sizeof cfg.host, value != NULL ? value : PLATINUM_BRIDGE_DEFAULT_HOST) != 0) {
        return -1;
    }

    value = env_get(env_names, env_values, env_count, "PLATINUM_BRIDGE_URL");
    if (value != NULL) {
        if (copy_bounded(cfg.base_url, sizeof cfg.base_url, value) != 0) {
            return -1;
        }
    } else {
        written = snprintf(cfg.base_url, sizeof cfg.base_url, "http://%s:%ld", cfg.host, cfg.port);
        if (written < 0 || (size_t)written >= sizeof cfg.base_url) {
            return -1;
        }
    }

    value = env_get(env_names, env_values, env_count, "PLATINUM_BRIDGE_PUBLIC_URL");
    if (copy_bounded(cfg.public_url, sizeof cfg.public_url, value != NULL ? value : cfg.base_url) != 0) {
        return -1;
    }

    value = env_get(env_names, env_values, env_count, "PLATINUM_BRIDGE_DATA_DIR");
    if (copy_bounded(cfg.data_dir, sizeof cfg.data_dir, value != NULL ? value : PLATINUM_BRIDGE_DEFAULT_DATA_DIR) != 0) {
        return -1;
    }

    value = env_get(env_names, env_values, env_count, "PLATINUM_BRIDGE_OAUTH_NODE");
    if (copy_bounded(cfg.oauth_node_url, sizeof cfg.oauth_node_url,
                     value != NULL ? value : PLATINUM_BRIDGE_DEFAULT_OAUTH_NODE) != 0) {
        return -1;
    }

    if (copy_bounded(cfg.version, sizeof cfg.version, PLATINUM_BRIDGE_VERSION) != 0) {
        return -1;
    }

    cfg.max_body_bytes = PLATINUM_BRIDGE_MAX_BODY_BYTES;

    value = env_get(env_names, env_values, env_count, "PLATINUM_BRIDGE_ALLOW_APP_PASSWORD");
    cfg.allow_app_password = (value != NULL && strcmp(value, "1") == 0) ? 1 : 0;

    *out = cfg;
    return 0;
}

int platinum_config_health_json(const platinum_bridge_config *cfg, char *buf, size_t buf_size)
{
    int written;

    if (cfg == NULL || buf == NULL || buf_size == 0) {
        return -1;
    }

    written = snprintf(buf, buf_size, "{\"ok\":true,\"service\":\"%s\",\"version\":\"%s\"}", PLATINUM_BRIDGE_SERVICE, cfg->version);
    if (written < 0 || (size_t)written >= buf_size) {
        return -1;
    }
    return 0;
}
