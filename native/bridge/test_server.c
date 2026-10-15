/* The bridge server: dispatch logic, the private-bind rule, and real HTTP requests. */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <wolfram/agent.h>
#include <wolfram/xrpc.h>

#include "mock_pds.h"
#include "server.h"
#include "server_http.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define PROFILE_JSON \
    "{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\",\"displayName\":\"Bob\",\"followersCount\":3," \
    "\"followsCount\":4,\"postsCount\":5}"
#define RECORD_JSON "{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3kpost\",\"cid\":\"bafyreiabc\"}"

static wf_agent *good_agent;

/* Only the one token is valid, and it maps to the agent the mock PDS answers. */
static void *
lookup(void *user, const char *token)
{
    (void) user;
    if (token != NULL && strcmp(token, "good-token") == 0) return good_agent;
    return NULL;
}

static int
dispatch(platinum_server_ctx *ctx, const char *method, const char *target, const char *auth,
         const char *body, char *out, size_t cap, int *status)
{
    *status = platinum_server_dispatch(ctx, method, target, auth, body, body ? strlen(body) : 0, out, cap);
    return *status;
}

static void
test_private_bind(void)
{
    CHECK(platinum_bind_is_private("127.0.0.1") == 1);
    CHECK(platinum_bind_is_private("::1") == 1);
    CHECK(platinum_bind_is_private("10.1.2.3") == 1);
    CHECK(platinum_bind_is_private("172.16.0.1") == 1);
    CHECK(platinum_bind_is_private("172.31.255.255") == 1);
    CHECK(platinum_bind_is_private("192.168.0.5") == 1);
    CHECK(platinum_bind_is_private("169.254.1.1") == 1);
    CHECK(platinum_bind_is_private("172.32.0.1") == 0);   /* just outside 172.16/12 */
    CHECK(platinum_bind_is_private("8.8.8.8") == 0);
    CHECK(platinum_bind_is_private("0.0.0.0") == 0);      /* every interface: refused */
    CHECK(platinum_bind_is_private("example.com") == 0);
    CHECK(platinum_bind_is_private(NULL) == 0);
}

static void
test_dispatch(void)
{
    platinum_server_ctx ctx;
    char out[8192];
    int st;

    memset(&ctx, 0, sizeof ctx);
    ctx.lookup = lookup;
    ctx.version = "0.3.1";
    ctx.max_body_bytes = 64 * 1024;

    /* Health: no token, 200, with the version. */
    st = dispatch(&ctx, "GET", "/health", NULL, NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"version\":\"0.3.1\"") != NULL);

    /* An unknown path is 404 before any token check. */
    st = dispatch(&ctx, "GET", "/no/such/path", NULL, NULL, out, sizeof out, &st);
    CHECK(st == 404);
    CHECK(strstr(out, "\"error\":\"not_found\"") != NULL);

    /* A known path with the wrong method is 405. */
    st = dispatch(&ctx, "DELETE", "/v1/profile", NULL, NULL, out, sizeof out, &st);
    CHECK(st == 405);

    /* An authenticated route without a token: 401 missing_bearer_token. */
    st = dispatch(&ctx, "GET", "/v1/profile", NULL, NULL, out, sizeof out, &st);
    CHECK(st == 401);
    CHECK(strstr(out, "missing_bearer_token") != NULL);

    /* A token that is not valid: 401 invalid_token. */
    st = dispatch(&ctx, "GET", "/v1/profile", "Bearer nope", NULL, out, sizeof out, &st);
    CHECK(st == 401);
    CHECK(strstr(out, "invalid_token") != NULL);

    /* A valid token, live: the profile comes from the PDS. */
    st = dispatch(&ctx, "GET", "/v1/profile?actor=bob.test", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"handle\":\"bob.test\"") != NULL);

    /* A route in the table but not wired yet: 501, not a made-up answer. */
    st = dispatch(&ctx, "GET", "/v1/timeline", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 501);
    CHECK(strstr(out, "not_implemented") != NULL);

    /* The image route needs a token; with one, a bad ref is 400 and no CDN is 502. */
    st = dispatch(&ctx, "GET", "/v1/image?ref=avatar/plain/did:plc:abc/x@png", NULL, NULL, out, sizeof out, &st);
    CHECK(st == 401);
    st = dispatch(&ctx, "GET", "/v1/image?ref=banner/x", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 502);
    CHECK(strstr(out, "image_unavailable") != NULL);

    /* Post: a valid body publishes live. */
    st = dispatch(&ctx, "POST", "/v1/post", "Bearer good-token",
                  "{\"text\":\"from the native bridge\"}", out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"cid\":\"bafyreiabc\"") != NULL);

    /* A body that is not JSON: 400. A body with no text: 400 missing_text. */
    st = dispatch(&ctx, "POST", "/v1/post", "Bearer good-token", "{not json", out, sizeof out, &st);
    CHECK(st == 400);
    st = dispatch(&ctx, "POST", "/v1/post", "Bearer good-token", "{\"text\":\"  \"}", out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "missing_text") != NULL);

    /* A body over the limit is refused before it is read. */
    {
        platinum_server_ctx small = ctx;
        small.max_body_bytes = 8;
        st = dispatch(&small, "POST", "/v1/post", "Bearer good-token",
                      "{\"text\":\"this body is longer than eight bytes\"}", out, sizeof out, &st);
        CHECK(st == 413);
    }
}

/* A raw HTTP request to the server; returns the status line's code, or -1. */
static int
http_request(int port, const char *request, char *reply, size_t cap)
{
    struct sockaddr_in sa;
    int fd;
    size_t got = 0;
    ssize_t n;
    int code = -1;

    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short) port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    if (connect(fd, (struct sockaddr *) &sa, sizeof sa) != 0) {
        close(fd);
        return -1;
    }
    if (write(fd, request, strlen(request)) < 0) {
        close(fd);
        return -1;
    }
    reply[0] = '\0';
    while (got + 1 < cap && (n = read(fd, reply + got, cap - got - 1)) > 0) {
        got += (size_t) n;
    }
    reply[got] = '\0';
    close(fd);
    if (strncmp(reply, "HTTP/1.1 ", 9) == 0) code = atoi(reply + 9);
    return code;
}

static void
test_http(void)
{
    platinum_server_ctx ctx;
    platinum_http_server *srv = NULL;
    char reply[16384];
    char req[2048];
    int port;
    int code;
    const char *body = "{\"text\":\"over http\"}";

    memset(&ctx, 0, sizeof ctx);
    ctx.lookup = lookup;
    ctx.version = "0.3.1";
    ctx.max_body_bytes = 64 * 1024;

    /* A public address is refused. */
    CHECK(platinum_http_start("8.8.8.8", 0, &ctx, &srv) == -1 && srv == NULL);

    CHECK(platinum_http_start("127.0.0.1", 0, &ctx, &srv) == 0);
    if (srv == NULL) return;
    port = platinum_http_port(srv);
    CHECK(port > 0);

    snprintf(req, sizeof req, "GET /health HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
    code = http_request(port, req, reply, sizeof reply);
    CHECK(code == 200);
    CHECK(strstr(reply, "\"service\":\"platinum-bridge\"") != NULL);

    /* A live route over HTTP, with the token and the query string. */
    snprintf(req, sizeof req,
             "GET /v1/profile?actor=bob.test HTTP/1.1\r\nHost: 127.0.0.1\r\n"
             "Authorization: Bearer good-token\r\nConnection: close\r\n\r\n");
    code = http_request(port, req, reply, sizeof reply);
    CHECK(code == 200);
    CHECK(strstr(reply, "\"handle\":\"bob.test\"") != NULL);

    /* No token over HTTP: 401. */
    snprintf(req, sizeof req, "GET /v1/profile HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
    code = http_request(port, req, reply, sizeof reply);
    CHECK(code == 401);

    /* A POST body over HTTP, with a length. */
    snprintf(req, sizeof req,
             "POST /v1/post HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Bearer good-token\r\n"
             "Content-Type: application/json\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n%s",
             (unsigned long) strlen(body), body);
    code = http_request(port, req, reply, sizeof reply);
    CHECK(code == 200);
    CHECK(strstr(reply, "bafyreiabc") != NULL);

    platinum_http_stop(srv);
}

int
main(void)
{
    wf_mock_pds *pds = NULL;
    int pds_port = 0;
    char svc[64];

    if (wf_mock_pds_start(&pds, &pds_port) != WF_OK) {
        fprintf(stderr, "could not start the mock PDS\n");
        return 1;
    }
    wf_mock_pds_register(pds, "app.bsky.actor.getProfile", PROFILE_JSON);
    wf_mock_pds_register(pds, "com.atproto.repo.createRecord", RECORD_JSON);
    snprintf(svc, sizeof svc, "http://127.0.0.1:%d", pds_port);
    good_agent = wf_agent_new(svc);
    CHECK(good_agent != NULL);
    if (good_agent != NULL) {
        CHECK(wf_agent_set_bearer(good_agent, "tok", "bob.test", "did:plc:bob") == WF_OK);
    }

    test_private_bind();
    test_dispatch();
    test_http();

    if (good_agent != NULL) wf_agent_free(good_agent);
    wf_mock_pds_free(pds);
    if (failures) {
        fprintf(stderr, "test_server: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_server: %d checks passed\n", checks);
    return 0;
}
