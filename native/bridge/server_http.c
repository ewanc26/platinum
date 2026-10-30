#include "server_http.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

#include <microhttpd.h>

struct platinum_http_server {
    struct MHD_Daemon *daemon;
    platinum_server_ctx ctx;
};

typedef struct platinum_conn {
    char *body;
    size_t len;
    size_t cap;
    int too_big;
} platinum_conn;

static void
completed(void *cls, struct MHD_Connection *conn, void **con_cls, enum MHD_RequestTerminationCode code)
{
    platinum_conn *c = (platinum_conn *) *con_cls;
    (void) cls; (void) conn; (void) code;
    if (c != NULL) {
        free(c->body);
        free(c);
    }
    *con_cls = NULL;
}

typedef struct platinum_args {
    char *buf;
    size_t cap;
    size_t used;
    int first;
    int overflow;
} platinum_args;

/* Appends key=value to the target, joined by &. MHD gives the values already decoded. */
static enum MHD_Result
arg_cb(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
    platinum_args *a = (platinum_args *) cls;
    size_t need;
    (void) kind;
    if (value == NULL) value = "";
    need = strlen(key) + 1 + strlen(value) + (a->first ? 1 : 2);
    if (a->overflow || a->used + need >= a->cap) {
        a->overflow = 1;
        return MHD_NO;
    }
    a->buf[a->used++] = a->first ? '?' : '&';
    a->first = 0;
    memcpy(a->buf + a->used, key, strlen(key));
    a->used += strlen(key);
    a->buf[a->used++] = '=';
    memcpy(a->buf + a->used, value, strlen(value));
    a->used += strlen(value);
    a->buf[a->used] = '\0';
    return MHD_YES;
}

/* The target the dispatcher reads: the path, then the query arguments. Returns -1 when
 * the target does not fit. */
static int
build_target(struct MHD_Connection *conn, const char *url, char *out, size_t cap)
{
    platinum_args a;
    if (strlen(url) + 1 >= cap) return -1;
    memcpy(out, url, strlen(url) + 1);
    a.buf = out;
    a.cap = cap;
    a.used = strlen(url);
    a.first = 1;
    a.overflow = 0;
    MHD_get_connection_values(conn, MHD_GET_ARGUMENT_KIND, &arg_cb, &a);
    return a.overflow ? -1 : 0;
}

/* The client's numeric address, or NULL. The app-password limiter keys on this. */
static const char *
peer_address(struct MHD_Connection *conn, char *buf, size_t cap)
{
    const union MHD_ConnectionInfo *info;
    const struct sockaddr *sa;

    if (conn == NULL || buf == NULL || cap == 0) return NULL;
    info = MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    if (info == NULL || info->client_addr == NULL) return NULL;
    sa = info->client_addr;
    if (sa->sa_family == AF_INET) {
        return inet_ntop(AF_INET, &((const struct sockaddr_in *) sa)->sin_addr,
                         buf, (socklen_t) cap);
    }
    if (sa->sa_family == AF_INET6) {
        return inet_ntop(AF_INET6, &((const struct sockaddr_in6 *) sa)->sin6_addr,
                         buf, (socklen_t) cap);
    }
    return NULL;
}

static enum MHD_Result
handle(void *cls, struct MHD_Connection *conn, const char *url, const char *method,
       const char *version, const char *upload_data, size_t *upload_size, void **con_cls)
{
    platinum_http_server *srv = (platinum_http_server *) cls;
    platinum_conn *c = (platinum_conn *) *con_cls;
    char target[2048];
    /* A page of twenty posts with images and cards runs past 16 KB, so the
     * response buffer is sized for the route's largest page rather than the
     * smallest one. A route that still does not fit answers 500, never a cut
     * body. */
    char out[65536];
    char peer_buf[64];
    const char *auth;
    const char *peer;
    const char *content_type = "application/json; charset=utf-8";
    size_t body_len = 0;
    int status;
    struct MHD_Response *resp;
    enum MHD_Result ok;

    (void) version;
    if (c == NULL) {
        c = calloc(1, sizeof *c);
        if (c == NULL) return MHD_NO;
        *con_cls = c;
        return MHD_YES;
    }
    if (*upload_size > 0) {
        if (!c->too_big && c->len + *upload_size <= srv->ctx.max_body_bytes) {
            if (c->len + *upload_size + 1 > c->cap) {
                size_t ncap = c->len + *upload_size + 1024;
                char *nb = realloc(c->body, ncap);
                if (nb == NULL) return MHD_NO;
                c->body = nb;
                c->cap = ncap;
            }
            memcpy(c->body + c->len, upload_data, *upload_size);
            c->len += *upload_size;
            c->body[c->len] = '\0';
        } else {
            c->too_big = 1;
        }
        *upload_size = 0;
        return MHD_YES;
    }

    if (build_target(conn, url, target, sizeof target) != 0) {
        status = 500;
        snprintf(out, sizeof out, "%s", "{\"error\":\"bridge_error\",\"message\":\"The backend could not complete the request.\"}");
    } else {
        auth = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "Authorization");
        peer = peer_address(conn, peer_buf, sizeof peer_buf);
        if (c->too_big) {
            status = 413;
            snprintf(out, sizeof out, "%s", "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}");
        } else {
            status = platinum_server_dispatch_ex(&srv->ctx, method, target, auth, peer,
                                                 c->body != NULL ? c->body : "", c->len,
                                                 out, sizeof out, &content_type, &body_len);
        }
    }

    /* The body length is the dispatcher's, not strlen(out): an image is binary and
     * a zero byte in it does not end the body. */
    resp = MHD_create_response_from_buffer(body_len, out, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(resp, "content-type", content_type);
    MHD_add_response_header(resp, "cache-control", "no-store");
    ok = MHD_queue_response(conn, (unsigned int) status, resp);
    MHD_destroy_response(resp);
    return ok;
}

int
platinum_http_start(const char *host, int port, const platinum_server_ctx *ctx,
                    platinum_http_server **out)
{
    platinum_http_server *srv;
    struct sockaddr_in sa;

    if (out == NULL || ctx == NULL || !platinum_bind_is_private(host)) return -1;
    if (port < 0 || port > 65535) return -1;

    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short) port);
    if (inet_pton(AF_INET, strcmp(host, "localhost") == 0 ? "127.0.0.1" : host, &sa.sin_addr) != 1) {
        return -1;
    }

    srv = calloc(1, sizeof *srv);
    if (srv == NULL) return -1;
    srv->ctx = *ctx;
    srv->daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, (unsigned short) port, NULL, NULL,
                                   &handle, srv,
                                   MHD_OPTION_SOCK_ADDR, (struct sockaddr *) &sa,
                                   MHD_OPTION_NOTIFY_COMPLETED, &completed, NULL,
                                   MHD_OPTION_END);
    if (srv->daemon == NULL) {
        free(srv);
        return -1;
    }
    *out = srv;
    return 0;
}

int
platinum_http_port(const platinum_http_server *srv)
{
    const union MHD_DaemonInfo *info;
    if (srv == NULL || srv->daemon == NULL) return -1;
    info = MHD_get_daemon_info(srv->daemon, MHD_DAEMON_INFO_BIND_PORT);
    return info != NULL ? (int) info->port : -1;
}

void
platinum_http_stop(platinum_http_server *srv)
{
    if (srv == NULL) return;
    if (srv->daemon != NULL) MHD_stop_daemon(srv->daemon);
    free(srv);
}
