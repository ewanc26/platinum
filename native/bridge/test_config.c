/* Host test for config.c. C89. Exits 1 if any check fails. */

#include <stdio.h>
#include <string.h>

#include "config.h"

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond) \
    do { \
        checks_run++; \
        if (!(cond)) { \
            checks_failed++; \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define ENV_COUNT 6

static const char *const env_keys[ENV_COUNT] = {
    "PLATINUM_BRIDGE_PORT",
    "PLATINUM_BRIDGE_HOST",
    "PLATINUM_BRIDGE_URL",
    "PLATINUM_BRIDGE_PUBLIC_URL",
    "PLATINUM_BRIDGE_DATA_DIR",
    "PLATINUM_BRIDGE_ALLOW_APP_PASSWORD"
};

/* Loads config from the six known variables. Pass NULL for unset. */
static int load_env(platinum_bridge_config *cfg, const char *port, const char *host, const char *url, const char *public_url, const char *data_dir, const char *allow)
{
    const char *values[ENV_COUNT];

    values[0] = port;
    values[1] = host;
    values[2] = url;
    values[3] = public_url;
    values[4] = data_dir;
    values[5] = allow;
    return platinum_config_load(cfg, env_keys, values, ENV_COUNT);
}

static char long_buf[600];

/* Returns a NUL-terminated string of n copies of c, using long_buf. */
static const char *repeat(char c, size_t n)
{
    memset(long_buf, c, n);
    long_buf[n] = '\0';
    return long_buf;
}

static void test_defaults(void)
{
    platinum_bridge_config cfg;
    int rc;

    rc = load_env(&cfg, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK(rc == 0);
    CHECK(strcmp(cfg.host, "127.0.0.1") == 0);
    CHECK(cfg.port == 8787L);
    CHECK(strcmp(cfg.base_url, "http://127.0.0.1:8787") == 0);
    CHECK(strcmp(cfg.public_url, "http://127.0.0.1:8787") == 0);
    CHECK(strcmp(cfg.data_dir, ".platinum-bridge") == 0);
    CHECK(strcmp(cfg.version, "0.3.1") == 0);
    CHECK(cfg.max_body_bytes == 65536L);
    CHECK(cfg.allow_app_password == 0);
}

static void test_custom_port_and_host(void)
{
    platinum_bridge_config cfg;
    int rc;

    rc = load_env(&cfg, "9000", "0.0.0.0", NULL, NULL, NULL, NULL);
    CHECK(rc == 0);
    CHECK(cfg.port == 9000L);
    CHECK(strcmp(cfg.host, "0.0.0.0") == 0);
    CHECK(strcmp(cfg.base_url, "http://0.0.0.0:9000") == 0);
    CHECK(strcmp(cfg.public_url, "http://0.0.0.0:9000") == 0);
}

static void test_port_bounds(void)
{
    platinum_bridge_config cfg;

    CHECK(load_env(&cfg, "1", NULL, NULL, NULL, NULL, NULL) == 0);
    CHECK(cfg.port == 1L);
    CHECK(load_env(&cfg, "65535", NULL, NULL, NULL, NULL, NULL) == 0);
    CHECK(cfg.port == 65535L);
    CHECK(strcmp(cfg.base_url, "http://127.0.0.1:65535") == 0);
}

static void test_bad_port(void)
{
    platinum_bridge_config cfg;

    CHECK(load_env(&cfg, "0", NULL, NULL, NULL, NULL, NULL) == -1);
    CHECK(load_env(&cfg, "65536", NULL, NULL, NULL, NULL, NULL) == -1);
    CHECK(load_env(&cfg, "-1", NULL, NULL, NULL, NULL, NULL) == -1);
    CHECK(load_env(&cfg, "12x", NULL, NULL, NULL, NULL, NULL) == -1);
    CHECK(load_env(&cfg, "", NULL, NULL, NULL, NULL, NULL) == -1);
    CHECK(load_env(&cfg, "abc", NULL, NULL, NULL, NULL, NULL) == -1);
    CHECK(load_env(&cfg, "80 80", NULL, NULL, NULL, NULL, NULL) == -1);
}

static void test_failed_load_leaves_out_unchanged(void)
{
    platinum_bridge_config cfg;

    cfg.port = -7L;
    CHECK(load_env(&cfg, "12x", NULL, NULL, NULL, NULL, NULL) == -1);
    CHECK(cfg.port == -7L);
}

static void test_url_overrides(void)
{
    platinum_bridge_config cfg;

    CHECK(load_env(&cfg, NULL, NULL, "https://bridge.example", NULL, NULL, NULL) == 0);
    CHECK(strcmp(cfg.base_url, "https://bridge.example") == 0);
    CHECK(strcmp(cfg.public_url, "https://bridge.example") == 0);

    CHECK(load_env(&cfg, NULL, NULL, "http://10.0.0.2:8787", "https://public.example", NULL, NULL) == 0);
    CHECK(strcmp(cfg.base_url, "http://10.0.0.2:8787") == 0);
    CHECK(strcmp(cfg.public_url, "https://public.example") == 0);
}

static void test_data_dir_override(void)
{
    platinum_bridge_config cfg;

    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, "/var/lib/platinum", NULL) == 0);
    CHECK(strcmp(cfg.data_dir, "/var/lib/platinum") == 0);
}

static void test_allow_app_password_flag(void)
{
    platinum_bridge_config cfg;

    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, NULL, "1") == 0);
    CHECK(cfg.allow_app_password == 1);
    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, NULL, "true") == 0);
    CHECK(cfg.allow_app_password == 0);
    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, NULL, "0") == 0);
    CHECK(cfg.allow_app_password == 0);
    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, NULL, "") == 0);
    CHECK(cfg.allow_app_password == 0);
    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, NULL, "11") == 0);
    CHECK(cfg.allow_app_password == 0);
}

static void test_buffer_limits(void)
{
    platinum_bridge_config cfg;

    /* host[64]: 63 characters fit, 64 do not. */
    CHECK(load_env(&cfg, NULL, repeat('h', 63), NULL, NULL, NULL, NULL) == 0);
    CHECK(load_env(&cfg, NULL, repeat('h', 64), NULL, NULL, NULL, NULL) == -1);

    /* base_url[256]: 255 characters fit, 256 do not. */
    CHECK(load_env(&cfg, NULL, NULL, repeat('u', 255), NULL, NULL, NULL) == 0);
    CHECK(load_env(&cfg, NULL, NULL, repeat('u', 256), NULL, NULL, NULL) == -1);

    /* public_url[256]: 255 characters fit, 256 do not. */
    CHECK(load_env(&cfg, NULL, NULL, NULL, repeat('p', 255), NULL, NULL) == 0);
    CHECK(load_env(&cfg, NULL, NULL, NULL, repeat('p', 256), NULL, NULL) == -1);

    /* data_dir[512]: 511 characters fit, 512 do not. */
    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, repeat('d', 511), NULL) == 0);
    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, repeat('d', 512), NULL) == -1);
}

static void test_null_arguments(void)
{
    platinum_bridge_config cfg;
    const char *values[ENV_COUNT];

    CHECK(platinum_config_load(NULL, env_keys, env_keys, ENV_COUNT) == -1);
    CHECK(platinum_config_load(&cfg, NULL, NULL, ENV_COUNT) == -1);
    values[0] = NULL;
    CHECK(platinum_config_load(&cfg, env_keys, values, 0) == 0);
}

static void test_health_json(void)
{
    platinum_bridge_config cfg;
    char buf[128];
    char small[64];
    const char *expected = "{\"ok\":true,\"service\":\"platinum-bridge\",\"version\":\"0.3.1\"}";
    size_t need;

    CHECK(load_env(&cfg, NULL, NULL, NULL, NULL, NULL, NULL) == 0);

    CHECK(platinum_config_health_json(&cfg, buf, sizeof buf) == 0);
    CHECK(strcmp(buf, expected) == 0);

    need = strlen(expected);
    CHECK(platinum_config_health_json(&cfg, small, need + 1) == 0);
    CHECK(strcmp(small, expected) == 0);
    CHECK(platinum_config_health_json(&cfg, small, need) == -1);
    CHECK(platinum_config_health_json(&cfg, small, 0) == -1);
    CHECK(platinum_config_health_json(NULL, buf, sizeof buf) == -1);
    CHECK(platinum_config_health_json(&cfg, NULL, sizeof buf) == -1);
}

int main(void)
{
    test_defaults();
    test_custom_port_and_host();
    test_port_bounds();
    test_bad_port();
    test_failed_load_leaves_out_unchanged();
    test_url_overrides();
    test_data_dir_override();
    test_allow_app_password_flag();
    test_buffer_limits();
    test_null_arguments();
    test_health_json();

    printf("test_config: %d checks, %d failed\n", checks_run, checks_failed);
    if (checks_failed != 0) {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
