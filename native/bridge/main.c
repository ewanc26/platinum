/*
 * platinum-bridge — the native bridge process.
 *
 * Configuration comes from the same PLATINUM_BRIDGE_* variables as the Node
 * bridge. The listener is private-only (127.0.0.1 or a LAN address); put it
 * behind a VPN or a reverse proxy if it must be reached from elsewhere.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "auth.h"
#include "config.h"
#include "image_cdn.h"
#include "server.h"
#include "server_http.h"

#define ENV_COUNT 7

static volatile sig_atomic_t stop_requested;

static void
on_signal(int sig)
{
    (void) sig;
    stop_requested = 1;
}

static void
usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--check]\n"
            "  Runs the Platinum bridge on PLATINUM_BRIDGE_HOST:PLATINUM_BRIDGE_PORT.\n"
            "  --check loads the configuration, token store and data directory, then exits.\n",
            argv0);
}

int
main(int argc, char **argv)
{
    static const char *names[ENV_COUNT] = {
        "PLATINUM_BRIDGE_PORT", "PLATINUM_BRIDGE_HOST", "PLATINUM_BRIDGE_URL",
        "PLATINUM_BRIDGE_PUBLIC_URL", "PLATINUM_BRIDGE_DATA_DIR",
        "PLATINUM_BRIDGE_ALLOW_APP_PASSWORD", "PLATINUM_BRIDGE_OAUTH_NODE"
    };
    const char *values[ENV_COUNT];
    platinum_bridge_config cfg;
    platinum_bridge_auth *auth;
    platinum_http_server *server = NULL;
    platinum_cdn cdn;
    platinum_server_ctx ctx;
    int check_only = 0;
    int i;
    int rc;

    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[1], "--check") == 0) {
            check_only = 1;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    for (i = 0; i < ENV_COUNT; i++) values[i] = getenv(names[i]);
    if (platinum_config_load(&cfg, names, values, ENV_COUNT) != 0) {
        fprintf(stderr, "platinum-bridge: invalid configuration\n");
        return 1;
    }

    auth = platinum_auth_new(&cfg);
    if (auth == NULL) {
        fprintf(stderr, "platinum-bridge: out of memory\n");
        return 1;
    }
    rc = platinum_auth_load(auth);
    if (rc < 0) {
        fprintf(stderr, "platinum-bridge: cannot load the token store in %s\n",
                cfg.data_dir);
        platinum_auth_free(auth);
        return 1;
    }

    memset(&cdn, 0, sizeof cdn);
    cdn.client = wf_xrpc_client_new(PLATINUM_CDN_BASE);
    if (cdn.client == NULL) {
        fprintf(stderr, "platinum-bridge: cannot prepare the image CDN client\n");
        platinum_auth_free(auth);
        return 1;
    }
    cdn.base = PLATINUM_CDN_BASE;

    memset(&ctx, 0, sizeof ctx);
    ctx.lookup = platinum_auth_lookup;
    ctx.user = auth;
    ctx.version = cfg.version;
    ctx.max_body_bytes = (size_t) cfg.max_body_bytes;
    ctx.auth = auth;
    ctx.cdn = &cdn;

    if (!cfg.allow_app_password) {
        fprintf(stderr,
                "platinum-bridge: app-password sign-in is disabled "
                "(set PLATINUM_BRIDGE_ALLOW_APP_PASSWORD=1 to enable)\n");
    }

    if (check_only) {
        printf("platinum-bridge: configuration and store are readable\n");
        wf_xrpc_client_free(cdn.client);
        platinum_auth_free(auth);
        return 0;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    if (platinum_http_start(cfg.host, (int) cfg.port, &ctx, &server) != 0) {
        fprintf(stderr, "platinum-bridge: could not listen on %s:%ld\n",
                cfg.host, cfg.port);
        wf_xrpc_client_free(cdn.client);
        platinum_auth_free(auth);
        return 1;
    }

    fprintf(stderr, "platinum-bridge: listening on %s:%d (%s)\n", cfg.host,
            platinum_http_port(server), cfg.version);
    fflush(stderr);

    while (!stop_requested) pause();

    fprintf(stderr, "platinum-bridge: stopping\n");
    platinum_http_stop(server);
    wf_xrpc_client_free(cdn.client);
    platinum_auth_free(auth);
    return 0;
}
