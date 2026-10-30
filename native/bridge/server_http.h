/*
 * The HTTP transport for the native bridge: libmicrohttpd around platinum_server_dispatch.
 * It binds after platinum_bind_is_private has passed, so a public address is refused.
 */
#ifndef PLATINUM_NATIVE_SERVER_HTTP_H
#define PLATINUM_NATIVE_SERVER_HTTP_H

#include "server.h"

typedef struct platinum_http_server platinum_http_server;

/* Starts listening on host:port (port 0 for an ephemeral port). Returns 0 and sets
 * *out, or -1 when host is not private or the socket cannot be bound. */
int platinum_http_start(const char *host, int port, const platinum_server_ctx *ctx,
                        platinum_http_server **out);

/* The port actually bound, for port 0. */
int platinum_http_port(const platinum_http_server *srv);

void platinum_http_stop(platinum_http_server *srv);

#endif
