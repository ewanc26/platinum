#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>
#include <wolfram/agent.h>

#include "account_routes.h"
#include "auth.h"
#include "collection_routes.h"
#include "content_routes.h"
#include "conversations.h"
#include "conversations_wolfram.h"
#include "image_cdn.h"
#include "image_route.h"
#include "notifications_route.h"
#include "notifications_wolfram.h"
#include "post_routes.h"
#include "profile.h"
#include "profile_live.h"
#include "profile_route.h"
#include "publish_live.h"
#include "routes.h"
#include "search_live.h"
#include "search_routes.h"
#include "timeline_route.h"
#include "timeline_wolfram.h"
#include "write_routes.h"

int
platinum_bind_is_private(const char *host)
{
    unsigned a, b, c, d;
    if (host == NULL) return 0;
    if (strcmp(host, "::1") == 0 || strcmp(host, "localhost") == 0) return 1;
    if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    if (a == 127) return 1;
    if (a == 10) return 1;
    if (a == 172 && b >= 16 && b <= 31) return 1;
    if (a == 192 && b == 168) return 1;
    if (a == 169 && b == 254) return 1;
    return 0;
}

static int
write_json(char *out, size_t cap, const char *json)
{
    size_t n = strlen(json);
    if (n >= cap) {
        out[0] = '\0';
        return 500;
    }
    memcpy(out, json, n + 1);
    return 0;
}

/* The query value of name in target (a path and query), or NULL. The value is copied
 * into buf; a value that does not fit is reported as absent. */
static const char *
query_value(const char *target, const char *name, char *buf, size_t cap)
{
    const char *q = strchr(target, '?');
    size_t n = strlen(name);
    if (q == NULL) return NULL;
    q++;
    while (*q) {
        const char *end = strchr(q, '&');
        size_t len = end ? (size_t)(end - q) : strlen(q);
        if (len > n && strncmp(q, name, n) == 0 && q[n] == '=') {
            size_t vlen = len - n - 1;
            if (vlen >= cap) return NULL;
            memcpy(buf, q + n + 1, vlen);
            buf[vlen] = '\0';
            return buf;
        }
        if (len == n && strncmp(q, name, n) == 0) {
            buf[0] = '\0';
            return buf;
        }
        if (end == NULL) break;
        q = end + 1;
    }
    return NULL;
}

/* The path part of target, without the query string, copied into buf. */
static int
path_of(const char *target, char *buf, size_t cap)
{
    size_t n = strcspn(target, "?");
    if (n >= cap) return -1;
    memcpy(buf, target, n);
    buf[n] = '\0';
    return 0;
}

/* Reads a string member of a JSON object into out, or NULL when absent or not a string. */
static const char *
json_string(const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static int
handle_post(void *agent, const char *body, size_t body_len, char *out, size_t cap)
{
    cJSON *root;
    platinum_post_request req;
    platinum_ref reply;
    platinum_ref quote;
    const cJSON *rt;
    const cJSON *qt;
    const cJSON *gate;
    int st;

    root = cJSON_ParseWithLength(body, body_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) cJSON_Delete(root);
        return write_json(out, cap, "{\"error\":\"invalid_json\",\"message\":\"The request body is not valid JSON.\"}") == 0 ? 400 : 500;
    }

    memset(&req, 0, sizeof req);
    memset(&reply, 0, sizeof reply);
    memset(&quote, 0, sizeof quote);
    req.text = json_string(root, "text");

    rt = cJSON_GetObjectItemCaseSensitive(root, "replyTo");
    if (rt != NULL) {
        req.has_reply = 1;
        req.reply_is_object = cJSON_IsObject(rt);
        reply.uri = json_string(rt, "uri");
        reply.cid = json_string(rt, "cid");
        req.reply = reply;
    }
    qt = cJSON_GetObjectItemCaseSensitive(root, "quote");
    if (qt != NULL) {
        req.has_quote = 1;
        req.quote_is_object = cJSON_IsObject(qt);
        quote.uri = json_string(qt, "uri");
        quote.cid = json_string(qt, "cid");
        req.quote = quote;
    }
    gate = cJSON_GetObjectItemCaseSensitive(root, "replyGate");
    if (gate != NULL) {
        req.has_gate = 1;
        req.gate = cJSON_IsString(gate) ? gate->valuestring : NULL;
    }

    st = platinum_route_post(agent, platinum_publish_live, &req, out, cap);
    cJSON_Delete(root);
    return st;
}

/*
 * Stub callbacks for route families that have handler logic (input validation, status
 * codes) but no live Wolfram call wired yet.  Each returns the error enum value so the
 * handler produces a well-formed 500 body rather than a raw 501.
 */

static platinum_collection_result
stub_collection(void *ctx, int kind, const char *key, int count, const char *cursor,
                char *body, size_t cap)
{
    (void) ctx; (void) kind; (void) key; (void) count; (void) cursor; (void) cap;
    if (body != NULL && cap > 0) body[0] = '\0';
    return PLATINUM_COLLECTION_ERROR;
}

static platinum_write_result
stub_toggle(void *ctx, int kind, const char *uri, const char *cid, int on,
            int *out_on, long *out_count)
{
    (void) ctx; (void) kind; (void) uri; (void) cid; (void) on;
    if (out_on) *out_on = 0;
    if (out_count) *out_count = 0;
    return PLATINUM_WRITE_ERROR;
}

static platinum_write_result
stub_subject(void *ctx, int kind, const char *did, int on)
{
    (void) ctx; (void) kind; (void) did; (void) on;
    return PLATINUM_WRITE_ERROR;
}

static platinum_account_result
stub_word(void *ctx, const char *word, int on)
{
    (void) ctx; (void) word; (void) on;
    return PLATINUM_ACCOUNT_ERROR;
}

static platinum_content_result
stub_muted_list(void *ctx, platinum_muted_view *views, size_t max, size_t *count)
{
    (void) ctx; (void) views; (void) max;
    if (count) *count = 0;
    return PLATINUM_CONTENT_ERROR;
}

static platinum_content_result
stub_delete(void *ctx, const char *uri)
{
    (void) ctx; (void) uri;
    return PLATINUM_CONTENT_ERROR;
}

/*
 * Body parsers shared by groups of POST routes.
 *
 * Each returns 0 on success (root_out is set and must be freed by the caller) or a
 * non-zero HTTP status on parse failure (the error body is already written into out).
 */

static int
parse_toggle_body(const char *body, size_t body_len, size_t max_bytes,
                  char *out, size_t cap,
                  cJSON **root_out,
                  const char **uri_out, const char **cid_out,
                  int *on_valid_out, int *on_out)
{
    cJSON *root;
    const cJSON *on_item;

    if (body_len > max_bytes) {
        write_json(out, cap,
                   "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}");
        return 413;
    }
    root = cJSON_ParseWithLength(body, body_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) cJSON_Delete(root);
        write_json(out, cap,
                   "{\"error\":\"invalid_json\",\"message\":\"The request body is not valid JSON.\"}");
        return 400;
    }
    *uri_out = json_string(root, "uri");
    *cid_out = json_string(root, "cid");
    on_item = cJSON_GetObjectItemCaseSensitive(root, "on");
    *on_valid_out = cJSON_IsBool(on_item);
    *on_out = cJSON_IsTrue(on_item) ? 1 : 0;
    *root_out = root;
    return 0;
}

static int
parse_subject_body(const char *body, size_t body_len, size_t max_bytes,
                   char *out, size_t cap,
                   cJSON **root_out,
                   const char **did_out,
                   int *on_valid_out, int *on_out)
{
    cJSON *root;
    const cJSON *did_item;
    const cJSON *on_item;

    if (body_len > max_bytes) {
        write_json(out, cap,
                   "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}");
        return 413;
    }
    root = cJSON_ParseWithLength(body, body_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) cJSON_Delete(root);
        write_json(out, cap,
                   "{\"error\":\"invalid_json\",\"message\":\"The request body is not valid JSON.\"}");
        return 400;
    }
    did_item = cJSON_GetObjectItemCaseSensitive(root, "did");
    *did_out = cJSON_IsString(did_item) ? did_item->valuestring : NULL;
    on_item  = cJSON_GetObjectItemCaseSensitive(root, "on");
    *on_valid_out = cJSON_IsBool(on_item);
    *on_out = cJSON_IsTrue(on_item) ? 1 : 0;
    *root_out = root;
    return 0;
}

static int
dispatch_inner(const platinum_server_ctx *ctx, const char *method,
               const char *target, const char *authorization,
               const char *peer, const char *body, size_t body_len,
               char *out, size_t cap, const char **content_type, size_t *binary_len)
{
    char path[512];
    char actor_buf[256];
    char bearer[512];
    platinum_route_id id = PLATINUM_ROUTE_NONE;
    int needs_token = 0;
    platinum_route_result rr;
    void *agent;
    const char *actor;

    if (binary_len != NULL) *binary_len = 0;
    if (ctx == NULL || method == NULL || target == NULL || out == NULL || cap == 0) return 500;
    out[0] = '\0';
    if (content_type != NULL) *content_type = "application/json; charset=utf-8";

    if (path_of(target, path, sizeof path) != 0) {
        return write_json(out, cap,
                          "{\"error\":\"not_found\",\"message\":\"The requested endpoint does not exist.\"}") == 0 ? 404 : 500;
    }

    rr = platinum_route_match(method, path, &id, &needs_token);
    if (rr == PLATINUM_ROUTE_NOT_FOUND) {
        return write_json(out, cap,
                          "{\"error\":\"not_found\",\"message\":\"The requested endpoint does not exist.\"}") == 0 ? 404 : 500;
    }
    if (rr == PLATINUM_ROUTE_METHOD_NOT_ALLOWED) {
        return write_json(out, cap,
                          "{\"error\":\"method_not_allowed\",\"message\":\"The method is not allowed for this endpoint.\"}") == 0 ? 405 : 500;
    }

    if (id == PLATINUM_ROUTE_HEALTH) {
        char json[128];
        snprintf(json, sizeof json,
                 "{\"ok\":true,\"service\":\"platinum-bridge\",\"version\":\"%s\"}",
                 ctx->version != NULL ? ctx->version : "0.0.0");
        return write_json(out, cap, json) == 0 ? 200 : 500;
    }

    /* Sign-in routes carry their own credential rules: the app-password route has no
     * bridge token yet, and revoke validates the token itself so a broken session
     * file cannot make an otherwise valid token unrecoverable. */
    if (id == PLATINUM_ROUTE_LOGIN && ctx->auth != NULL) {
        char handle_buf[256];
        const char *handle = query_value(target, "handle", handle_buf, sizeof handle_buf);
        return platinum_auth_login_page((platinum_bridge_auth *) ctx->auth, handle,
                                        out, cap, content_type);
    }
    if (id == PLATINUM_ROUTE_PAIR && ctx->auth != NULL) {
        return platinum_auth_pair((platinum_bridge_auth *) ctx->auth, body, body_len,
                                  out, cap);
    }
    if (id == PLATINUM_ROUTE_APP_PASSWORD_LOGIN && ctx->auth != NULL) {
        return platinum_auth_app_password_login((platinum_bridge_auth *) ctx->auth,
                                                body, body_len, peer, out, cap);
    }
    if (id == PLATINUM_ROUTE_REVOKE && ctx->auth != NULL) {
        return platinum_auth_revoke((platinum_bridge_auth *) ctx->auth,
                                    authorization, out, cap);
    }

    /* Every route below this line is guarded by a bearer token. */
    if (needs_token) {
        if (platinum_route_bearer_token(authorization, bearer, sizeof bearer) != 0) {
            return write_json(out, cap,
                              "{\"error\":\"missing_bearer_token\",\"message\":\"A Platinum installation token is required.\"}") == 0 ? 401 : 500;
        }
        agent = ctx->lookup != NULL ? ctx->lookup(ctx->user, bearer) : NULL;
        if (agent == NULL) {
            return write_json(out, cap,
                              "{\"error\":\"invalid_token\",\"message\":\"The Platinum installation token is not valid.\"}") == 0 ? 401 : 500;
        }
    } else {
        agent = NULL;
    }

    /* --- GET routes --- */

    if (id == PLATINUM_ROUTE_PROFILE) {
        actor = query_value(target, "actor", actor_buf, sizeof actor_buf);
        return platinum_route_profile(agent, platinum_profile_fetch_live, actor, out, cap);
    }

    if (id == PLATINUM_ROUTE_IMAGE) {
        char ref_buf[320];
        char w_buf[16];
        char depth_buf[8];
        const char *ref   = query_value(target, "ref",   ref_buf,   sizeof ref_buf);
        const char *w     = query_value(target, "w",     w_buf,     sizeof w_buf);
        const char *depth = query_value(target, "depth", depth_buf, sizeof depth_buf);
        unsigned char *blob = NULL;
        long blob_len = 0;
        int st;

        if (ctx->cdn == NULL) {
            return write_json(out, cap,
                              "{\"error\":\"image_unavailable\",\"message\":\"The image could not be fetched.\"}") == 0 ? 502 : 500;
        }
        st = platinum_route_image(ctx->cdn,
                                  ctx->image_source != NULL ? ctx->image_source
                                                            : platinum_image_source_cdn,
                                  ref, w, depth, &blob, &blob_len);
        if (st == 200) {
            /* The blob is binary, so it is framed by its length and not by a
             * terminator: reserve one byte to NUL-terminate the buffer for every
             * caller that still treats `out` as a string, then report the real body
             * length. Without the length the HTTP layer would cut the body at the
             * first zero byte. */
            if ((size_t) blob_len + 1 > cap) {
                free(blob);
                return write_json(out, cap,
                                  "{\"error\":\"image_unavailable\",\"message\":\"The image could not be prepared.\"}") == 0 ? 502 : 500;
            }
            memcpy(out, blob, (size_t) blob_len);
            out[blob_len] = '\0';
            if (binary_len != NULL) *binary_len = (size_t) blob_len;
            if (content_type != NULL) *content_type = "application/x-platinum-image";
            free(blob);
            return 200;
        }
        free(blob);
        if (st == 400) {
            return write_json(out, cap,
                              "{\"error\":\"invalid_image\",\"message\":\"ref must be an image reference the bridge gave you; w is 16 to 320; depth is 4 or 8.\"}") == 0 ? 400 : 500;
        }
        if (st == 413) {
            return write_json(out, cap,
                              "{\"error\":\"image_unavailable\",\"message\":\"The image could not be prepared.\"}") == 0 ? 413 : 500;
        }
        return write_json(out, cap,
                          "{\"error\":\"image_unavailable\",\"message\":\"The image could not be fetched.\"}") == 0 ? 502 : 500;
    }

    if (id == PLATINUM_ROUTE_TIMELINE) {
        /* Wider than the route's own 255-byte cursor bound, so an over-long cursor
         * reaches the route and is refused there rather than silently dropped. */
        char cursor_buf[512];
        char limit_buf[16];
        const char *cursor = query_value(target, "cursor", cursor_buf, sizeof cursor_buf);
        const char *limit  = query_value(target, "limit",  limit_buf,  sizeof limit_buf);
        return platinum_route_timeline(agent, platinum_timeline_fetch_live, limit, cursor,
                                       out, cap);
    }

    if (id == PLATINUM_ROUTE_NOTIFICATIONS) {
        /* Wider than the route's own 255-byte cursor bound, so an over-long
         * cursor reaches the route and is refused there rather than dropped. */
        char cursor_buf[512];
        char limit_buf[16];
        const char *cursor = query_value(target, "cursor", cursor_buf, sizeof cursor_buf);
        const char *limit  = query_value(target, "limit",  limit_buf,  sizeof limit_buf);
        return platinum_route_notifications(agent, platinum_notifications_fetch_live,
                                            limit, cursor, out, cap);
    }

    if (id == PLATINUM_ROUTE_CONVERSATIONS) {
        char cursor_buf[256];
        char limit_buf[16];
        const char *cursor = query_value(target, "cursor", cursor_buf, sizeof cursor_buf);
        const char *limit  = query_value(target, "limit",  limit_buf,  sizeof limit_buf);
        return platinum_route_conversations(agent, platinum_conversations_fetch_live,
                                            limit, cursor, out, cap);
    }

    if (id == PLATINUM_ROUTE_CONVERSATION) {
        char id_buf[PLATINUM_CONVO_ID_MAX];
        char cursor_buf[256];
        char limit_buf[16];
        const char *convo_id = query_value(target, "id",     id_buf,     sizeof id_buf);
        const char *cursor   = query_value(target, "cursor", cursor_buf, sizeof cursor_buf);
        const char *limit    = query_value(target, "limit",  limit_buf,  sizeof limit_buf);
        return platinum_route_conversation(agent, platinum_messages_fetch_live,
                                           convo_id, limit, cursor, out, cap);
    }

    if (id == PLATINUM_ROUTE_FOLLOWS || id == PLATINUM_ROUTE_FOLLOWERS ||
        id == PLATINUM_ROUTE_AUTHOR_FEED) {
        char actor_raw[256];
        char cursor_buf[256];
        char limit_buf[16];
        int kind = (id == PLATINUM_ROUTE_FOLLOWS) ? 0 : (id == PLATINUM_ROUTE_FOLLOWERS) ? 1 : 2;
        const char *actor_q = query_value(target, "actor",  actor_raw,  sizeof actor_raw);
        const char *cursor  = query_value(target, "cursor", cursor_buf, sizeof cursor_buf);
        const char *limit   = query_value(target, "limit",  limit_buf,  sizeof limit_buf);
        return platinum_route_actor_graph(agent, stub_collection, kind,
                                          actor_q, limit, cursor, out, cap);
    }

    if (id == PLATINUM_ROUTE_POST_LIKES || id == PLATINUM_ROUTE_POST_REPOSTS) {
        char uri_buf[512];
        char cursor_buf[256];
        char limit_buf[16];
        int kind = (id == PLATINUM_ROUTE_POST_LIKES) ? 0 : 1;
        const char *uri    = query_value(target, "uri",    uri_buf,    sizeof uri_buf);
        const char *cursor = query_value(target, "cursor", cursor_buf, sizeof cursor_buf);
        const char *limit  = query_value(target, "limit",  limit_buf,  sizeof limit_buf);
        return platinum_route_engagement(agent, stub_collection, kind,
                                         uri, limit, cursor, out, cap);
    }

    if (id == PLATINUM_ROUTE_SEARCH_ACTORS || id == PLATINUM_ROUTE_SEARCH_POSTS) {
        char q_buf[128];
        char cursor_buf[256];
        char limit_buf[16];
        int posts = (id == PLATINUM_ROUTE_SEARCH_POSTS) ? 1 : 0;
        const char *q      = query_value(target, "q",      q_buf,      sizeof q_buf);
        const char *cursor = query_value(target, "cursor", cursor_buf, sizeof cursor_buf);
        const char *limit  = query_value(target, "limit",  limit_buf,  sizeof limit_buf);
        return platinum_route_search(agent, platinum_search_fetch_live, posts, q, limit, cursor,
                                     out, cap);
    }

    if (id == PLATINUM_ROUTE_FEEDS) {
        return write_json(out, cap,
                          "{\"error\":\"bridge_error\",\"message\":\"The backend could not complete the request.\"}") == 0 ? 500 : 500;
    }

    if (id == PLATINUM_ROUTE_MUTED_WORDS_GET) {
        return platinum_route_muted_list(agent, stub_muted_list, out, cap);
    }

    if (id == PLATINUM_ROUTE_LISTS) {
        return write_json(out, cap,
                          "{\"error\":\"bridge_error\",\"message\":\"The backend could not complete the request.\"}") == 0 ? 500 : 500;
    }

    if (id == PLATINUM_ROUTE_FEED || id == PLATINUM_ROUTE_LIST) {
        char uri_buf[512];
        char cursor_buf[256];
        char limit_buf[16];
        int is_feed = (id == PLATINUM_ROUTE_FEED) ? 1 : 0;
        const char *uri    = query_value(target, "uri",    uri_buf,    sizeof uri_buf);
        const char *cursor = query_value(target, "cursor", cursor_buf, sizeof cursor_buf);
        const char *limit  = query_value(target, "limit",  limit_buf,  sizeof limit_buf);
        return platinum_route_collection(agent, stub_collection, is_feed,
                                         uri, limit, cursor, out, cap);
    }

    if (id == PLATINUM_ROUTE_THREAD) {
        char uri_buf[512];
        const char *uri = query_value(target, "uri", uri_buf, sizeof uri_buf);
        return platinum_route_thread(agent, stub_collection, uri, out, cap);
    }

    /* --- POST routes --- */

    if (id == PLATINUM_ROUTE_POST) {
        if (body_len > ctx->max_body_bytes) {
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}") == 0 ? 413 : 500;
        }
        return handle_post(agent, body, body_len, out, cap);
    }

    if (id == PLATINUM_ROUTE_LIKE || id == PLATINUM_ROUTE_REPOST) {
        cJSON *root = NULL;
        const char *uri = NULL;
        const char *cid = NULL;
        int on_valid = 0;
        int on = 0;
        int kind = (id == PLATINUM_ROUTE_LIKE) ? 0 : 1;
        int st;

        st = parse_toggle_body(body, body_len, ctx->max_body_bytes, out, cap,
                               &root, &uri, &cid, &on_valid, &on);
        if (st != 0) return st;
        st = platinum_route_toggle(agent, stub_toggle, kind, uri, cid, on_valid, on, out, cap);
        cJSON_Delete(root);
        return st;
    }

    if (id == PLATINUM_ROUTE_NOTIFICATIONS_SEEN) {
        cJSON *root;
        const char *seen_at_raw;
        double now_ms;
        int st;

        if (body_len > ctx->max_body_bytes) {
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}") == 0 ? 413 : 500;
        }
        root = cJSON_ParseWithLength(body, body_len);
        if (root == NULL || !cJSON_IsObject(root)) {
            if (root != NULL) cJSON_Delete(root);
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is not valid JSON.\"}") == 0 ? 400 : 500;
        }
        seen_at_raw = json_string(root, "seenAt");
        now_ms = (double) time(NULL) * 1000.0;
        st = platinum_route_seen(agent, platinum_notifications_seen_live, seen_at_raw,
                                 now_ms, out, cap);
        cJSON_Delete(root);
        return st;
    }

    if (id == PLATINUM_ROUTE_FOLLOW || id == PLATINUM_ROUTE_MUTE ||
        id == PLATINUM_ROUTE_BLOCK) {
        cJSON *root = NULL;
        const char *did = NULL;
        int on_valid = 0;
        int on = 0;
        int kind = (id == PLATINUM_ROUTE_FOLLOW) ? 0 : (id == PLATINUM_ROUTE_MUTE) ? 1 : 2;
        int st;

        st = parse_subject_body(body, body_len, ctx->max_body_bytes, out, cap,
                                &root, &did, &on_valid, &on);
        if (st != 0) return st;
        st = platinum_route_subject(agent, stub_subject, kind, did, on_valid, on, out, cap);
        cJSON_Delete(root);
        return st;
    }

    if (id == PLATINUM_ROUTE_MUTED_WORDS_SET) {
        cJSON *root;
        const cJSON *on_item;
        const char *word;
        int on_valid;
        int on;
        int st;

        if (body_len > ctx->max_body_bytes) {
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}") == 0 ? 413 : 500;
        }
        root = cJSON_ParseWithLength(body, body_len);
        if (root == NULL || !cJSON_IsObject(root)) {
            if (root != NULL) cJSON_Delete(root);
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is not valid JSON.\"}") == 0 ? 400 : 500;
        }
        word     = json_string(root, "value");
        on_item  = cJSON_GetObjectItemCaseSensitive(root, "on");
        on_valid = cJSON_IsBool(on_item);
        on       = cJSON_IsTrue(on_item) ? 1 : 0;
        st = platinum_route_muted_set(agent, stub_word, word, on_valid, on, out, cap);
        cJSON_Delete(root);
        return st;
    }

    if (id == PLATINUM_ROUTE_POST_DELETE) {
        cJSON *root;
        const char *uri;
        int st;

        if (body_len > ctx->max_body_bytes) {
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}") == 0 ? 413 : 500;
        }
        root = cJSON_ParseWithLength(body, body_len);
        if (root == NULL || !cJSON_IsObject(root)) {
            if (root != NULL) cJSON_Delete(root);
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is not valid JSON.\"}") == 0 ? 400 : 500;
        }
        uri = json_string(root, "uri");
        st = platinum_route_post_delete(agent, stub_delete, uri, out, cap);
        cJSON_Delete(root);
        return st;
    }

    if (id == PLATINUM_ROUTE_CONVERSATION_MESSAGE) {
        cJSON *root;
        const char *convo_id;
        const char *text;
        int st;

        if (body_len > ctx->max_body_bytes) {
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}") == 0 ? 413 : 500;
        }
        root = cJSON_ParseWithLength(body, body_len);
        if (root == NULL || !cJSON_IsObject(root)) {
            if (root != NULL) cJSON_Delete(root);
            return write_json(out, cap,
                              "{\"error\":\"invalid_json\",\"message\":\"The request body is not valid JSON.\"}") == 0 ? 400 : 500;
        }
        convo_id = json_string(root, "convoId");
        text     = json_string(root, "text");
        if (convo_id == NULL || convo_id[0] == '\0') {
            cJSON_Delete(root);
            return write_json(out, cap,
                              "{\"error\":\"invalid_conversation\",\"message\":\"convoId must name a conversation.\"}") == 0 ? 400 : 500;
        }
        if (text == NULL || text[0] == '\0') {
            cJSON_Delete(root);
            return write_json(out, cap,
                              "{\"error\":\"missing_text\",\"message\":\"Message text is required.\"}") == 0 ? 400 : 500;
        }
        st = platinum_route_conversation_message(agent, platinum_send_message_live,
                                                 convo_id, text, out, cap);
        cJSON_Delete(root);
        return st;
    }

    /* Routes in the table with no handler here (the node's OAuth callback and
     * client metadata): answer 501. Those belong to the hosted OAuth node, not the
     * bridge. */
    return write_json(out, cap,
                      "{\"error\":\"not_implemented\",\"message\":\"This endpoint is not wired in this bridge build.\"}") == 0 ? 501 : 500;
}

int
platinum_server_dispatch_ex(const platinum_server_ctx *ctx, const char *method,
                            const char *target, const char *authorization,
                            const char *peer, const char *body, size_t body_len,
                            char *out, size_t cap, const char **content_type,
                            size_t *out_len)
{
    size_t binary_len = 0;
    int status = dispatch_inner(ctx, method, target, authorization, peer, body, body_len,
                                out, cap, content_type, &binary_len);
    if (out_len != NULL) {
        *out_len = out != NULL ? (binary_len != 0 ? binary_len : strlen(out)) : 0;
    }
    return status;
}

int
platinum_server_dispatch(const platinum_server_ctx *ctx, const char *method, const char *target,
                         const char *authorization, const char *body, size_t body_len,
                         char *out, size_t cap)
{
    return platinum_server_dispatch_ex(ctx, method, target, authorization, NULL, body,
                                       body_len, out, cap, NULL, NULL);
}
