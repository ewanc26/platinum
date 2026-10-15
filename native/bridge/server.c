#include "server.h"

#include <stdio.h>
#include <string.h>

#include <cJSON.h>
#include <wolfram/agent.h>

#include "post_routes.h"
#include "profile.h"
#include "profile_live.h"
#include "profile_route.h"
#include "publish_live.h"
#include "routes.h"

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

int
platinum_server_dispatch(const platinum_server_ctx *ctx, const char *method, const char *target,
                         const char *authorization, const char *body, size_t body_len,
                         char *out, size_t cap)
{
    char path[512];
    char actor_buf[256];
    char bearer[512];
    platinum_route_id id = PLATINUM_ROUTE_NONE;
    int needs_token = 0;
    platinum_route_result rr;
    void *agent;
    const char *actor;

    if (ctx == NULL || method == NULL || target == NULL || out == NULL || cap == 0) return 500;
    out[0] = '\0';

    if (path_of(target, path, sizeof path) != 0) {
        return write_json(out, cap, "{\"error\":\"not_found\",\"message\":\"The requested endpoint does not exist.\"}") == 0 ? 404 : 500;
    }

    rr = platinum_route_match(method, path, &id, &needs_token);
    if (rr == PLATINUM_ROUTE_NOT_FOUND) {
        return write_json(out, cap, "{\"error\":\"not_found\",\"message\":\"The requested endpoint does not exist.\"}") == 0 ? 404 : 500;
    }
    if (rr == PLATINUM_ROUTE_METHOD_NOT_ALLOWED) {
        return write_json(out, cap, "{\"error\":\"method_not_allowed\",\"message\":\"The method is not allowed for this endpoint.\"}") == 0 ? 405 : 500;
    }

    if (id == PLATINUM_ROUTE_HEALTH) {
        char json[128];
        snprintf(json, sizeof json, "{\"ok\":true,\"service\":\"platinum-bridge\",\"version\":\"%s\"}",
                 ctx->version != NULL ? ctx->version : "0.0.0");
        return write_json(out, cap, json) == 0 ? 200 : 500;
    }

    /* Every other route needs a valid bearer token: check it before anything else. */
    if (needs_token) {
        if (platinum_route_bearer_token(authorization, bearer, sizeof bearer) != 0) {
            return write_json(out, cap, "{\"error\":\"missing_bearer_token\",\"message\":\"A Platinum installation token is required.\"}") == 0 ? 401 : 500;
        }
        agent = ctx->lookup != NULL ? ctx->lookup(ctx->user, bearer) : NULL;
        if (agent == NULL) {
            return write_json(out, cap, "{\"error\":\"invalid_token\",\"message\":\"The Platinum installation token is not valid.\"}") == 0 ? 401 : 500;
        }
    } else {
        agent = NULL;
    }

    if (id == PLATINUM_ROUTE_PROFILE) {
        actor = query_value(target, "actor", actor_buf, sizeof actor_buf);
        return platinum_route_profile(agent, platinum_profile_fetch_live, actor, out, cap);
    }
    if (id == PLATINUM_ROUTE_POST) {
        if (body_len > ctx->max_body_bytes) {
            return write_json(out, cap, "{\"error\":\"invalid_json\",\"message\":\"The request body is too large.\"}") == 0 ? 413 : 500;
        }
        return handle_post(agent, body, body_len, out, cap);
    }

    return write_json(out, cap, "{\"error\":\"not_implemented\",\"message\":\"This endpoint is not wired in this bridge build.\"}") == 0 ? 501 : 500;
}
