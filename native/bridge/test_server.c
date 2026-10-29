/* The bridge server: dispatch logic, the private-bind rule, and real HTTP requests. */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
#define TIMELINE_JSON \
    "{\"feed\":[{\"post\":{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3ktl\"," \
    "\"cid\":\"bafytl\",\"author\":{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\"}," \
    "\"record\":{\"$type\":\"app.bsky.feed.post\",\"text\":\"from the timeline\"," \
    "\"createdAt\":\"2026-10-01T10:00:00.000Z\"},\"likeCount\":2}}],\"cursor\":\"tl1\"}"
/* The viewer is did:plc:bob (wf_agent_set_bearer below), so the other member is
 * the counterpart and the second message is not "mine". */
#define CONVOS_JSON \
    "{\"convos\":[{\"id\":\"convo-direct\",\"rev\":\"r1\",\"members\":[" \
    "{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\"}," \
    "{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\",\"displayName\":\"Alice\"}]," \
    "\"lastMessage\":{\"text\":\"hello there\"},\"unreadCount\":2}],\"cursor\":\"c1\"}"
#define MESSAGES_JSON \
    "{\"messages\":[" \
    "{\"id\":\"m2\",\"rev\":\"r2\",\"text\":\"mine\",\"sender\":{\"did\":\"did:plc:bob\"}," \
    "\"sentAt\":\"2026-10-02T10:00:00.000Z\"}," \
    "{\"id\":\"m1\",\"rev\":\"r1\",\"text\":\"theirs\",\"sender\":{\"did\":\"did:plc:alice\"}," \
    "\"sentAt\":\"2026-10-01T10:00:00.000Z\"}],\"cursor\":\"mc1\"}"
#define SENT_MESSAGE_JSON \
    "{\"id\":\"m3\",\"rev\":\"r3\",\"text\":\"hi there\"," \
    "\"sender\":{\"did\":\"did:plc:bob\"},\"sentAt\":\"2026-10-03T10:00:00.000Z\"}"
#define ACTORS_JSON \
    "{\"actors\":[{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\"," \
    "\"displayName\":\"Alice\"}],\"cursor\":\"a1\"}"
#define FOLLOWS_JSON \
    "{\"follows\":[{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\"," \
    "\"displayName\":\"Alice\"}],\"cursor\":\"f1\"}"
#define FOLLOWERS_JSON \
    "{\"followers\":[{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\"}]," \
    "\"cursor\":\"fo1\"}"
#define AUTHOR_FEED_JSON \
    "{\"feed\":[{\"post\":{\"uri\":\"at://did:plc:alice/app.bsky.feed.post/3kaf\"," \
    "\"cid\":\"bafyaf\",\"author\":{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\"}," \
    "\"record\":{\"$type\":\"app.bsky.feed.post\",\"text\":\"from the author\"," \
    "\"createdAt\":\"2026-10-01T10:00:00.000Z\"},\"likeCount\":1}}],\"cursor\":\"af1\"}"
#define LIKES_JSON \
    "{\"likes\":[{\"actor\":{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\"," \
    "\"displayName\":\"Alice\"},\"createdAt\":\"2026-10-01T10:00:00.000Z\"," \
    "\"indexedAt\":\"2026-10-01T10:00:00.000Z\"}],\"cursor\":\"l1\"}"
#define REPOSTED_JSON \
    "{\"repostedBy\":[{\"did\":\"did:plc:alice\",\"handle\":\"alice.test\"}]," \
    "\"cursor\":\"r1\"}"
#define PREFS_JSON \
    "{\"preferences\":[{\"$type\":\"app.bsky.actor.defs#savedFeedsPrefV2\",\"items\":[" \
    "{\"id\":\"1\",\"type\":\"feed\"," \
    "\"value\":\"at://did:plc:bob/app.bsky.feed.generator/whats-hot\",\"pinned\":true}," \
    "{\"id\":\"2\",\"type\":\"timeline\",\"value\":\"following\",\"pinned\":true}]}]}"
#define GENERATORS_JSON \
    "{\"feeds\":[{\"uri\":\"at://did:plc:bob/app.bsky.feed.generator/whats-hot\"," \
    "\"displayName\":\"What's Hot\",\"creator\":{\"did\":\"did:plc:bob\"," \
    "\"handle\":\"bob.test\"}}]}"
#define LISTS_JSON \
    "{\"lists\":[{\"uri\":\"at://did:plc:bob/app.bsky.graph.list/3kl\"," \
    "\"name\":\"Friends\",\"purpose\":\"app.bsky.graph.defs#curatelist\"}]}"
#define NOTIFICATIONS_JSON \
    "{\"notifications\":[{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3kn\"," \
    "\"cid\":\"bafyn\",\"author\":{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\"}," \
    "\"reason\":\"like\",\"indexedAt\":\"2026-10-01T10:00:00.000Z\"," \
    "\"isRead\":false}],\"cursor\":\"n1\"}"

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

    /* Timeline is live: the page comes from the PDS, with its cursor. */
    st = dispatch(&ctx, "GET", "/v1/timeline", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"text\":\"from the timeline\"") != NULL);
    CHECK(strstr(out, "\"cursor\":\"tl1\"") != NULL);

    /* A cursor that cannot be echoed back is refused before the fetch. */
    {
        char target[400];
        int i;
        strcpy(target, "/v1/timeline?cursor=");
        for (i = 0; i < 300; i++) strcat(target, "a");
        st = dispatch(&ctx, "GET", target, "Bearer good-token", NULL, out, sizeof out, &st);
        CHECK(st == 400);
        CHECK(strstr(out, "invalid_cursor") != NULL);
    }

    /* Notifications are live: the page comes from the PDS, with its cursor. */
    st = dispatch(&ctx, "GET", "/v1/notifications?limit=5", "Bearer good-token", NULL,
                  out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"reason\":\"like\"") != NULL);
    CHECK(strstr(out, "\"isRead\":false") != NULL);
    CHECK(strstr(out, "\"cursor\":\"n1\"") != NULL);

    /* Conversations are live: the list, one page of messages and a send all reach
     * the chat service through the PDS, and the viewer's own message is marked. */
    st = dispatch(&ctx, "GET", "/v1/conversations", "Bearer good-token", NULL,
                  out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"id\":\"convo-direct\"") != NULL);
    CHECK(strstr(out, "\"name\":\"Alice\"") != NULL);
    CHECK(strstr(out, "\"handle\":\"@alice.test\"") != NULL);
    CHECK(strstr(out, "\"group\":false") != NULL);
    CHECK(strstr(out, "\"unread\":2") != NULL);
    CHECK(strstr(out, "\"cursor\":\"c1\"") != NULL);

    st = dispatch(&ctx, "GET", "/v1/conversation?id=convo-direct", "Bearer good-token",
                  NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"mine\":true") != NULL);
    CHECK(strstr(out, "\"mine\":false") != NULL);
    CHECK(strstr(out, "\"cursor\":\"mc1\"") != NULL);

    st = dispatch(&ctx, "POST", "/v1/conversation/message", "Bearer good-token",
                  "{\"convoId\":\"convo-direct\",\"text\":\"hi there\"}",
                  out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"text\":\"hi there\"") != NULL);
    CHECK(strstr(out, "\"id\":\"m3\"") != NULL);

    /* Actor search is live too. */
    st = dispatch(&ctx, "GET", "/v1/search/actors?q=ali", "Bearer good-token", NULL,
                  out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"handle\":\"alice.test\"") != NULL);
    CHECK(strstr(out, "\"displayName\":\"Alice\"") != NULL);

    /* The actor graph and engagement lists are live, in the account-list shape. */
    st = dispatch(&ctx, "GET", "/v1/follows?actor=alice.test", "Bearer good-token",
                  NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"displayName\":\"Alice\"") != NULL);
    CHECK(strstr(out, "\"cursor\":\"f1\"") != NULL);

    st = dispatch(&ctx, "GET", "/v1/followers?actor=alice.test", "Bearer good-token",
                  NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"cursor\":\"fo1\"") != NULL);

    st = dispatch(&ctx, "GET",
                  "/v1/post/likes?uri=at://did:plc:bob/app.bsky.feed.post/3kpost",
                  "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"displayName\":\"Alice\"") != NULL);
    CHECK(strstr(out, "\"cursor\":\"l1\"") != NULL);

    st = dispatch(&ctx, "GET",
                  "/v1/post/reposts?uri=at://did:plc:bob/app.bsky.feed.post/3kpost",
                  "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"cursor\":\"r1\"") != NULL);

    /* The author feed is the timeline shape, cursor included. */
    st = dispatch(&ctx, "GET", "/v1/author-feed?actor=alice.test", "Bearer good-token",
                  NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"text\":\"from the author\"") != NULL);
    CHECK(strstr(out, "\"cursor\":\"af1\"") != NULL);

    /* A cursor that cannot be echoed back is refused before the fetch. */
    {
        char target[400];
        int i;
        strcpy(target, "/v1/notifications?cursor=");
        for (i = 0; i < 300; i++) strcat(target, "a");
        st = dispatch(&ctx, "GET", target, "Bearer good-token", NULL, out, sizeof out, &st);
        CHECK(st == 400);
        CHECK(strstr(out, "invalid_cursor") != NULL);
    }

    /* Seen is live: the timestamp is normalised and sent to the PDS. */
    st = dispatch(&ctx, "POST", "/v1/notifications/seen", "Bearer good-token",
                  "{\"seenAt\":\"2026-10-01T10:00:00.000Z\"}", out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"seenAt\":\"2026-10-01T10:00:00.000Z\"") != NULL);

    /* Routes with no handler at all (OAuth, pairing) still return 501. */
    st = dispatch(&ctx, "POST", "/v1/pair", NULL, "{\"code\":\"ABCDEF\"}", out, sizeof out, &st);
    CHECK(st == 501);
    CHECK(strstr(out, "not_implemented") != NULL);

    /* /v1/follows: missing actor → 400; a good actor is served live. */
    st = dispatch(&ctx, "GET", "/v1/follows", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_actor") != NULL);
    st = dispatch(&ctx, "GET", "/v1/follows?actor=bob.test", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 200);

    /* /v1/post/likes: missing uri → 400. */
    st = dispatch(&ctx, "GET", "/v1/post/likes", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_post_ref") != NULL);

    /* /v1/search/actors: missing q → 400. */
    st = dispatch(&ctx, "GET", "/v1/search/actors", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_query") != NULL);

    /* /v1/feeds: the saved custom feeds are named from the preferences; the
     * timeline entry in the same preference is not a custom feed. */
    st = dispatch(&ctx, "GET", "/v1/feeds", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"items\":[{\"uri\":\"at://did:plc:bob/app.bsky.feed.generator/whats-hot\"") != NULL);
    CHECK(strstr(out, "\"name\":\"What's Hot\"") != NULL);
    CHECK(strstr(out, "following") == NULL);

    /* /v1/lists: the account's own lists, named. */
    st = dispatch(&ctx, "GET", "/v1/lists", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 200);
    CHECK(strstr(out, "\"uri\":\"at://did:plc:bob/app.bsky.graph.list/3kl\"") != NULL);
    CHECK(strstr(out, "\"name\":\"Friends\"") != NULL);

    /* /v1/feed: missing uri → 400. */
    st = dispatch(&ctx, "GET", "/v1/feed", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_uri") != NULL);

    /* /v1/thread: missing uri → 400. */
    st = dispatch(&ctx, "GET", "/v1/thread", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_post_ref") != NULL);

    /* POST /v1/like: bad JSON → 400; missing on → 400. */
    st = dispatch(&ctx, "POST", "/v1/like", "Bearer good-token", "{not json", out, sizeof out, &st);
    CHECK(st == 400);
    st = dispatch(&ctx, "POST", "/v1/like", "Bearer good-token",
                  "{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/3k\",\"cid\":\"bafyabc\"}",
                  out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_post_ref") != NULL);

    /* POST /v1/follow: bad JSON → 400; missing did → 400. */
    st = dispatch(&ctx, "POST", "/v1/follow", "Bearer good-token", "{not json", out, sizeof out, &st);
    CHECK(st == 400);
    st = dispatch(&ctx, "POST", "/v1/follow", "Bearer good-token", "{\"on\":true}", out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_actor") != NULL);

    /* POST /v1/notifications/seen: bad JSON → 400; missing seenAt → 400. */
    st = dispatch(&ctx, "POST", "/v1/notifications/seen", "Bearer good-token",
                  "{\"seenAt\":\"not-a-timestamp\"}", out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_seen_at") != NULL);

    /* POST /v1/post/delete: missing uri → 400. */
    st = dispatch(&ctx, "POST", "/v1/post/delete", "Bearer good-token",
                  "{}", out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_post_ref") != NULL);

    /* POST /v1/muted-words: missing word or on → 400. */
    st = dispatch(&ctx, "POST", "/v1/muted-words", "Bearer good-token",
                  "{\"value\":\"bad\",\"on\":\"not-bool\"}", out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_word") != NULL);

    /* GET /v1/conversation: missing id → 400. */
    st = dispatch(&ctx, "GET", "/v1/conversation", "Bearer good-token", NULL, out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_conversation") != NULL);

    /* POST /v1/conversation/message: missing convoId → 400; missing text → 400. */
    st = dispatch(&ctx, "POST", "/v1/conversation/message", "Bearer good-token",
                  "{\"text\":\"hi\"}", out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "invalid_conversation") != NULL);
    st = dispatch(&ctx, "POST", "/v1/conversation/message", "Bearer good-token",
                  "{\"convoId\":\"abc123\"}", out, sizeof out, &st);
    CHECK(st == 400);
    CHECK(strstr(out, "missing_text") != NULL);


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

/* Case-insensitive header lookup in a raw reply. Stops at the header block, so a
 * binary body after it is never read. */
static const char *
find_header(const char *reply, const char *name)
{
    size_t n = strlen(name);
    const char *p = reply;

    while ((p = strchr(p, '\n')) != NULL) {
        p++;
        if (strncasecmp(p, name, n) == 0 && p[n] == ':') return p + n + 1;
    }
    return NULL;
}

/* A 2x2 image the route can encode without the CDN, so the HTTP transport's binary
 * framing is exercised offline. */
static platinum_image_result
fake_image(void *ctx, const char *ref, platinum_rgba *out)
{
    static const unsigned char pixels[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 0, 255
    };

    (void) ctx;
    if (ref == NULL || strncmp(ref, "avatar/plain/", 13) != 0) return PLATINUM_IMAGE_ERROR;
    out->pixels = malloc(sizeof pixels);
    if (out->pixels == NULL) return PLATINUM_IMAGE_ERROR;
    memcpy(out->pixels, pixels, sizeof pixels);
    out->width = 2;
    out->height = 2;
    return PLATINUM_IMAGE_OK;
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
    /* Set before the daemon copies the context: the image route runs offline through
     * the fake source, and the fake ignores the non-NULL cdn. */
    ctx.cdn = &ctx;
    ctx.image_source = fake_image;

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

    /* Notifications over the same HTTP path, with its query string. */
    snprintf(req, sizeof req,
             "GET /v1/notifications?limit=5 HTTP/1.1\r\nHost: 127.0.0.1\r\n"
             "Authorization: Bearer good-token\r\nConnection: close\r\n\r\n");
    code = http_request(port, req, reply, sizeof reply);
    CHECK(code == 200);
    CHECK(strstr(reply, "\"reason\":\"like\"") != NULL);
    CHECK(strstr(reply, "\"cursor\":\"n1\"") != NULL);

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

    /* A binary body over HTTP. The PLIM blob has zero bytes in it, so the transport
     * must frame it by the length the dispatcher reports. Before that length existed,
     * the body was cut at the first zero and the rest of the stack buffer could reach
     * the client. */
    {
        const char *ref = "avatar/plain/did:plc:abc/x@png";
        unsigned char *blob = NULL;
        long blob_len = 0;
        size_t json_len = 0;
        long sent_len;
        const char *body_at;
        const char *len_hdr;

        CHECK(platinum_route_image(NULL, fake_image, ref, "16", "4", &blob, &blob_len) == 200);
        CHECK(blob_len > 4 && memcmp(blob, "PLIM", 4) == 0);
        CHECK(blob != NULL && memchr(blob, 0, (size_t) blob_len) != NULL);

        snprintf(req, sizeof req,
                 "GET /v1/image?ref=%s&w=16&depth=4 HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                 "Authorization: Bearer good-token\r\nConnection: close\r\n\r\n", ref);
        code = http_request(port, req, reply, sizeof reply);
        CHECK(code == 200);
        CHECK(strstr(reply, "application/x-platinum-image") != NULL);

        body_at = strstr(reply, "\r\n\r\n");
        len_hdr = find_header(reply, "Content-Length");
        sent_len = len_hdr != NULL ? strtol(len_hdr, NULL, 10) : -1;
        CHECK(body_at != NULL);
        CHECK(sent_len == blob_len);
        if (body_at != NULL && blob != NULL && sent_len == blob_len) {
            CHECK(memcmp(body_at + 4, blob, (size_t) blob_len) == 0);
        }
        free(blob);

        /* Every other route still reports its text length, so a caller that frames
         * by out_len is correct for both shapes. */
        CHECK(platinum_server_dispatch_ex(&ctx, "GET", "/health", NULL, NULL, NULL, 0,
                                          reply, sizeof reply, NULL, &json_len) == 200);
        CHECK(json_len == strlen(reply));
    }

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
    wf_mock_pds_register(pds, "app.bsky.feed.getTimeline", TIMELINE_JSON);
    wf_mock_pds_register(pds, "app.bsky.notification.listNotifications", NOTIFICATIONS_JSON);
    wf_mock_pds_register(pds, "app.bsky.notification.updateSeen", "{}");
    wf_mock_pds_register(pds, "com.atproto.server.describeServer",
                         "{\"did\":\"did:web:mock\",\"chat\":\"did:web:api.bsky.chat\"}");
    wf_mock_pds_register(pds, "chat.bsky.convo.listConvos", CONVOS_JSON);
    wf_mock_pds_register(pds, "chat.bsky.convo.getMessages", MESSAGES_JSON);
    wf_mock_pds_register(pds, "chat.bsky.convo.sendMessage", SENT_MESSAGE_JSON);
    wf_mock_pds_register(pds, "app.bsky.actor.searchActors", ACTORS_JSON);
    wf_mock_pds_register(pds, "app.bsky.graph.getFollows", FOLLOWS_JSON);
    wf_mock_pds_register(pds, "app.bsky.graph.getFollowers", FOLLOWERS_JSON);
    wf_mock_pds_register(pds, "app.bsky.feed.getAuthorFeed", AUTHOR_FEED_JSON);
    wf_mock_pds_register(pds, "app.bsky.feed.getLikes", LIKES_JSON);
    wf_mock_pds_register(pds, "app.bsky.feed.getRepostedBy", REPOSTED_JSON);
    wf_mock_pds_register(pds, "app.bsky.actor.getPreferences", PREFS_JSON);
    wf_mock_pds_register(pds, "app.bsky.feed.getFeedGenerators", GENERATORS_JSON);
    wf_mock_pds_register(pds, "app.bsky.graph.getLists", LISTS_JSON);
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
