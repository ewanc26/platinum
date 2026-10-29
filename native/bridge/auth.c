#include "auth.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>
#include <wolfram/agent.h>
#include <wolfram/oauth_pairing.h>
#include <wolfram/session.h>

#include "fsstore.h"
#include "policy.h"
#include "routes.h"
#include "token.h"
#include "token_persist.h"

#define AUTH_SESSION_MAX 16384
#define AUTH_ID_MAX 40
#define AUTH_CACHE_SLOTS 8
/* One slot per node pairing that has not been exchanged yet. The node holds
 * 64, so the bridge holds 64 too: a reloaded sign-in page starts another
 * pairing, and a smaller table would refuse a fresh attempt while the page it
 * replaced is still alive. */
#define AUTH_PENDING_SLOTS 64
#define AUTH_SESSION_PREFIX "session-"
#define AUTH_PAIR_ALPHABET "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
#define AUTH_PAIR_CODE_LEN 6
/* bsky.social occasionally fails to fetch the node's client metadata within
 * its own 10-second bound and answers the begin request with a 502. A fresh
 * request makes it fetch again, so retry rather than showing the Mac a
 * transient failure. */
#define AUTH_BEGIN_ATTEMPTS 3

typedef struct platinum_auth_pending {
    int used;
    long expires_at;
    char code[AUTH_PAIR_CODE_LEN + 1];
    char node_code[WF_OAUTH_PAIR_CODE_MAX];
} platinum_auth_pending;

typedef struct platinum_auth_cache_entry {
    int used;
    char id[AUTH_ID_MAX];
    wf_agent *agent;
    char service[256];
    char *saved_session; /* last JSON written, so refresh updates are persisted */
    long used_at;
} platinum_auth_cache_entry;

struct platinum_bridge_auth {
    platinum_bridge_config cfg;
    platinum_token_store tokens;
    platinum_failure_limiter limiter;
    platinum_auth_cache_entry cache[AUTH_CACHE_SLOTS];
    platinum_auth_pending pending[AUTH_PENDING_SLOTS];
    long clock;
};

static int
secure_random(unsigned char *buf, size_t len)
{
    FILE *f;
    size_t got;

    if (buf == NULL || len == 0) return -1;
    f = fopen("/dev/urandom", "rb");
    if (f == NULL) return -1;
    got = fread(buf, 1, len, f);
    fclose(f);
    return got == len ? 0 : -1;
}

static int
random_bytes(unsigned char *buf, size_t len)
{
    return secure_random(buf, len);
}

static int
valid_id(const char *id)
{
    const char *p;

    if (id == NULL || id[0] == '\0' || strlen(id) >= AUTH_ID_MAX) return 0;
    for (p = id; *p != '\0'; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
              || (*p >= '0' && *p <= '9') || *p == '-')) {
            return 0;
        }
    }
    return 1;
}

static int
make_installation_id(char out[AUTH_ID_MAX])
{
    unsigned char b[16];
    int written;

    if (random_bytes(b, sizeof b) != 0) return -1;
    b[6] = (unsigned char) ((b[6] & 0x0f) | 0x40);
    b[8] = (unsigned char) ((b[8] & 0x3f) | 0x80);
    written = snprintf(out, AUTH_ID_MAX,
                       "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                       "%02x%02x%02x%02x%02x%02x",
                       b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                       b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return written > 0 && written < AUTH_ID_MAX ? 0 : -1;
}

static int
make_pair_code(char out[AUTH_PAIR_CODE_LEN + 1])
{
    unsigned char bytes[AUTH_PAIR_CODE_LEN];
    int i;

    if (random_bytes(bytes, sizeof bytes) != 0) return -1;
    for (i = 0; i < AUTH_PAIR_CODE_LEN; i++) {
        /* 32 letters in the alphabet and 256 divides by 32, so this is uniform. */
        out[i] = AUTH_PAIR_ALPHABET[bytes[i] & 31U];
    }
    out[AUTH_PAIR_CODE_LEN] = '\0';
    return 0;
}

static void
prune_pending(platinum_bridge_auth *auth, long now)
{
    int i;

    for (i = 0; i < AUTH_PENDING_SLOTS; i++) {
        if (auth->pending[i].used && auth->pending[i].expires_at < now) {
            memset(&auth->pending[i], 0, sizeof auth->pending[i]);
        }
    }
}

static platinum_auth_pending *
find_pending(platinum_bridge_auth *auth, const char *code)
{
    char key[AUTH_PAIR_CODE_LEN + 1];
    int i;

    if (code == NULL || strlen(code) != AUTH_PAIR_CODE_LEN) return NULL;
    for (i = 0; i < AUTH_PAIR_CODE_LEN; i++) {
        key[i] = (char) toupper((unsigned char) code[i]);
    }
    key[AUTH_PAIR_CODE_LEN] = '\0';
    for (i = 0; i < AUTH_PENDING_SLOTS; i++) {
        if (auth->pending[i].used && strcmp(auth->pending[i].code, key) == 0) {
            return &auth->pending[i];
        }
    }
    return NULL;
}

static int
create_pending(platinum_bridge_auth *auth, const char *node_code, long expires_at,
               char out_code[AUTH_PAIR_CODE_LEN + 1])
{
    platinum_auth_pending *slot = NULL;
    char code[AUTH_PAIR_CODE_LEN + 1];
    int attempt;
    int i;

    if (node_code == NULL || strlen(node_code) >= WF_OAUTH_PAIR_CODE_MAX) return -1;
    prune_pending(auth, (long) time(NULL));
    for (i = 0; i < AUTH_PENDING_SLOTS; i++) {
        if (!auth->pending[i].used) {
            slot = &auth->pending[i];
            break;
        }
    }
    if (slot == NULL) return -1;
    for (attempt = 0; attempt < 8; attempt++) {
        if (make_pair_code(code) != 0) return -1;
        if (find_pending(auth, code) == NULL) {
            memset(slot, 0, sizeof *slot);
            slot->used = 1;
            slot->expires_at = expires_at;
            memcpy(slot->code, code, sizeof code);
            strncpy(slot->node_code, node_code, sizeof slot->node_code - 1);
            memcpy(out_code, code, sizeof code);
            return 0;
        }
    }
    return -1;
}

static int
session_name(const char *id, char *out, size_t cap)
{
    int written;

    if (!valid_id(id)) return -1;
    written = snprintf(out, cap, AUTH_SESSION_PREFIX "%s.json", id);
    return written > 0 && (size_t) written < cap ? 0 : -1;
}

static void
write_json_string(char *out, size_t cap, const char *json)
{
    size_t n;

    if (out == NULL || cap == 0) return;
    if (json == NULL) {
        out[0] = '\0';
        return;
    }
    n = strlen(json);
    if (n >= cap) n = cap - 1;
    memcpy(out, json, n);
    out[n] = '\0';
}

static int
write_status_json(char *out, size_t cap, int status, const char *error,
                  const char *message)
{
    cJSON *root;
    char *json;

    root = cJSON_CreateObject();
    if (root == NULL) return status;
    cJSON_AddStringToObject(root, "error", error);
    cJSON_AddStringToObject(root, "message", message);
    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) return status;
    write_json_string(out, cap, json);
    free(json);
    return status;
}

static char *
password_session_json(const char *service, const wf_session_data *data)
{
    cJSON *root;
    char *json;

    if (service == NULL || data == NULL || data->access_jwt == NULL
        || data->refresh_jwt == NULL || data->handle == NULL
        || data->did == NULL) {
        return NULL;
    }
    root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    cJSON_AddStringToObject(root, "kind", "password");
    cJSON_AddStringToObject(root, "service", service);
    cJSON_AddStringToObject(root, "accessJwt", data->access_jwt);
    cJSON_AddStringToObject(root, "refreshJwt", data->refresh_jwt);
    cJSON_AddStringToObject(root, "handle", data->handle);
    cJSON_AddStringToObject(root, "did", data->did);
    if (data->pds_url != NULL && data->pds_url[0] != '\0') {
        cJSON_AddStringToObject(root, "pdsUrl", data->pds_url);
    }
    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static char *
node_session_json(const char *service, const char *token, const char *handle,
                  const char *did)
{
    cJSON *root;
    char *json;

    if (service == NULL || token == NULL || handle == NULL || did == NULL) {
        return NULL;
    }
    root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    cJSON_AddStringToObject(root, "kind", "node");
    cJSON_AddStringToObject(root, "service", service);
    cJSON_AddStringToObject(root, "token", token);
    cJSON_AddStringToObject(root, "handle", handle);
    cJSON_AddStringToObject(root, "did", did);
    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static const char *
json_string(const cJSON *root, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static int
save_session(platinum_bridge_auth *auth, const char *id, const char *json)
{
    char name[80];

    if (session_name(id, name, sizeof name) != 0 || json == NULL) return -1;
    return platinum_fs_write_atomic(auth->cfg.data_dir, name,
                                    (const unsigned char *) json,
                                    strlen(json)) == PLATINUM_FS_OK ? 0 : -1;
}

static void
delete_session(platinum_bridge_auth *auth, const char *id)
{
    char name[80];

    if (session_name(id, name, sizeof name) == 0) {
        (void) platinum_fs_remove(auth->cfg.data_dir, name);
    }
}

static char *
load_session_json(platinum_bridge_auth *auth, const char *id)
{
    char name[80];
    unsigned char *data = NULL;
    size_t len = 0;
    char *json;

    if (session_name(id, name, sizeof name) != 0) return NULL;
    if (platinum_fs_read_all(auth->cfg.data_dir, name, &data, &len,
                             AUTH_SESSION_MAX) != PLATINUM_FS_OK) {
        return NULL;
    }
    json = (char *) malloc(len + 1);
    if (json == NULL) {
        free(data);
        return NULL;
    }
    memcpy(json, data, len);
    json[len] = '\0';
    free(data);
    return json;
}

static wf_agent *
agent_from_password(const char *service, const cJSON *root)
{
    const char *access = json_string(root, "accessJwt");
    const char *refresh = json_string(root, "refreshJwt");
    const char *handle = json_string(root, "handle");
    const char *did = json_string(root, "did");
    const char *pds = json_string(root, "pdsUrl");
    wf_session_data data;
    wf_agent *agent;

    if (access == NULL || refresh == NULL || handle == NULL || did == NULL) {
        return NULL;
    }
    agent = wf_agent_new(pds != NULL && pds[0] != '\0' ? pds : service);
    if (agent == NULL) return NULL;

    memset(&data, 0, sizeof data);
    data.access_jwt = (char *) access;
    data.refresh_jwt = (char *) refresh;
    data.handle = (char *) handle;
    data.did = (char *) did;
    data.pds_url = (char *) pds;
    if (wf_agent_resume(agent, &data) != WF_OK) {
        wf_agent_free(agent);
        return NULL;
    }
    return agent;
}

static wf_agent *
agent_from_node(const cJSON *root)
{
    const char *service = json_string(root, "service");
    const char *token = json_string(root, "token");
    const char *handle = json_string(root, "handle");
    const char *did = json_string(root, "did");
    wf_agent *agent;

    if (service == NULL || token == NULL || handle == NULL || did == NULL) {
        return NULL;
    }
    agent = wf_agent_new(service);
    if (agent == NULL) return NULL;
    if (wf_agent_set_bearer(agent, token, handle, did) != WF_OK) {
        wf_agent_free(agent);
        return NULL;
    }
    return agent;
}

static wf_agent *
agent_from_session(platinum_bridge_auth *auth, const char *id,
                   char *service_out, size_t service_cap)
{
    char *json;
    cJSON *root;
    const char *kind;
    const char *service;
    wf_agent *agent = NULL;

    if (service_out != NULL && service_cap > 0) service_out[0] = '\0';
    json = load_session_json(auth, id);
    if (json == NULL) return NULL;
    root = cJSON_Parse(json);
    free(json);
    if (root == NULL) return NULL;

    kind = json_string(root, "kind");
    service = json_string(root, "service");
    if (kind != NULL && service != NULL && strcmp(kind, "password") == 0) {
        agent = agent_from_password(service, root);
    } else if (kind != NULL && strcmp(kind, "node") == 0) {
        agent = agent_from_node(root);
    }
    if (agent != NULL && service_out != NULL && service_cap > 0
        && strlen(service) < service_cap) {
        strcpy(service_out, service);
    }
    cJSON_Delete(root);
    return agent;
}

static platinum_auth_cache_entry *
cache_find(platinum_bridge_auth *auth, const char *id)
{
    int i;

    for (i = 0; i < AUTH_CACHE_SLOTS; i++) {
        if (auth->cache[i].used && strcmp(auth->cache[i].id, id) == 0) {
            return &auth->cache[i];
        }
    }
    return NULL;
}

static platinum_auth_cache_entry *
cache_slot(platinum_bridge_auth *auth)
{
    int i;
    int victim = 0;

    for (i = 0; i < AUTH_CACHE_SLOTS; i++) {
        if (!auth->cache[i].used) return &auth->cache[i];
        if (auth->cache[i].used_at < auth->cache[victim].used_at) victim = i;
    }
    if (auth->cache[victim].agent != NULL) wf_agent_free(auth->cache[victim].agent);
    free(auth->cache[victim].saved_session);
    memset(&auth->cache[victim], 0, sizeof auth->cache[victim]);
    return &auth->cache[victim];
}

static void
cache_put(platinum_bridge_auth *auth, const char *id, wf_agent *agent,
          const char *service)
{
    platinum_auth_cache_entry *entry = cache_find(auth, id);

    if (entry == NULL) entry = cache_slot(auth);
    if (entry->agent != NULL) wf_agent_free(entry->agent);
    free(entry->saved_session);
    memset(entry, 0, sizeof *entry);
    entry->used = 1;
    strncpy(entry->id, id, sizeof entry->id - 1);
    entry->id[sizeof entry->id - 1] = '\0';
    if (service != NULL) {
        strncpy(entry->service, service, sizeof entry->service - 1);
        entry->service[sizeof entry->service - 1] = '\0';
    }
    entry->agent = agent;
    entry->used_at = ++auth->clock;
}

static void
cache_remove(platinum_bridge_auth *auth, const char *id)
{
    platinum_auth_cache_entry *entry = cache_find(auth, id);

    if (entry == NULL) return;
    if (entry->agent != NULL) wf_agent_free(entry->agent);
    free(entry->saved_session);
    memset(entry, 0, sizeof *entry);
}

/* Persist a refreshed PDS session, but only when it differs from the last
 * write, so an idle lookup does not fsync the same file on every request. */
static void
persist_if_changed(platinum_bridge_auth *auth, platinum_auth_cache_entry *entry)
{
    wf_session_data data;
    char *json;

    if (entry->agent == NULL || entry->service[0] == '\0') return;
    memset(&data, 0, sizeof data);
    if (wf_agent_get_session_data(entry->agent, &data) != WF_OK) return;
    json = password_session_json(entry->service, &data);
    wf_agent_session_data_free(&data);
    if (json == NULL) return;
    if (entry->saved_session == NULL || strcmp(entry->saved_session, json) != 0) {
        if (save_session(auth, entry->id, json) == 0) {
            free(entry->saved_session);
            entry->saved_session = json;
            return;
        }
    }
    free(json);
}

platinum_bridge_auth *
platinum_auth_new(const platinum_bridge_config *cfg)
{
    platinum_bridge_auth *auth;

    if (cfg == NULL) return NULL;
    auth = (platinum_bridge_auth *) calloc(1, sizeof *auth);
    if (auth == NULL) return NULL;
    auth->cfg = *cfg;
    platinum_token_store_init(&auth->tokens);
    platinum_limiter_init(&auth->limiter, PLATINUM_LIMITER_DEFAULT_MAX,
                          PLATINUM_LIMITER_DEFAULT_WINDOW);
    return auth;
}

int
platinum_auth_load(platinum_bridge_auth *auth)
{
    int rc;

    if (auth == NULL) return -1;
    if (platinum_fs_ensure_dir(auth->cfg.data_dir) != PLATINUM_FS_OK) return -1;
    rc = platinum_token_store_load(&auth->tokens, auth->cfg.data_dir);
    if (rc == 1) return 1;
    return rc == 0 ? 0 : -1;
}

void
platinum_auth_free(platinum_bridge_auth *auth)
{
    int i;

    if (auth == NULL) return;
    for (i = 0; i < AUTH_CACHE_SLOTS; i++) {
        if (auth->cache[i].agent != NULL) wf_agent_free(auth->cache[i].agent);
        free(auth->cache[i].saved_session);
    }
    free(auth);
}

size_t
platinum_auth_cached_agents(const platinum_bridge_auth *auth)
{
    size_t n = 0;
    int i;

    if (auth == NULL) return 0;
    for (i = 0; i < AUTH_CACHE_SLOTS; i++) {
        if (auth->cache[i].used) n++;
    }
    return n;
}

void
platinum_auth_invalidate(platinum_bridge_auth *auth, const char *installation_id)
{
    if (auth == NULL || installation_id == NULL) return;
    cache_remove(auth, installation_id);
}

void *
platinum_auth_lookup(void *user, const char *token)
{
    platinum_bridge_auth *auth = (platinum_bridge_auth *) user;
    platinum_auth_cache_entry *entry;
    wf_agent *agent;
    char service[256];
    int slot;

    if (auth == NULL || token == NULL) return NULL;
    slot = platinum_token_store_authenticate(&auth->tokens, token, time(NULL));
    if (slot < 0) return NULL;
    entry = cache_find(auth, auth->tokens.slots[slot].id);
    if (entry != NULL) {
        entry->used_at = ++auth->clock;
        persist_if_changed(auth, entry);
        return entry->agent;
    }
    agent = agent_from_session(auth, auth->tokens.slots[slot].id, service,
                               sizeof service);
    if (agent == NULL) return NULL;
    cache_put(auth, auth->tokens.slots[slot].id, agent, service);
    entry = cache_find(auth, auth->tokens.slots[slot].id);
    if (entry != NULL) persist_if_changed(auth, entry);
    return agent;
}

int
platinum_auth_app_password_login(platinum_bridge_auth *auth, const char *body,
                                 size_t body_len, const char *peer, char *out,
                                 size_t cap)
{
    cJSON *root;
    const cJSON *id_item;
    const cJSON *password_item;
    const cJSON *service_item;
    const char *identifier;
    const char *password;
    char service[256];
    char session_service[256];
    char installation_id[AUTH_ID_MAX];
    char token[PLATINUM_TOKEN_BUF];
    char did_buf[PLATINUM_TOKEN_DID_MAX];
    char *pds = NULL;
    wf_agent *agent;
    wf_session_data data;
    char *json;
    char *response;
    cJSON *reply;
    long now;

    if (auth == NULL || out == NULL || cap == 0) return 500;
    out[0] = '\0';
    if (!auth->cfg.allow_app_password) {
        return write_status_json(out, cap, 403, "app_password_disabled",
                                 "App-password sign-in is not enabled on this bridge.");
    }
    if (body_len > (size_t) auth->cfg.max_body_bytes) {
        return write_status_json(out, cap, 413, "invalid_json",
                                 "The request body is too large.");
    }
    if (peer == NULL || peer[0] == '\0') peer = "unknown";
    now = (long) time(NULL);
    if (platinum_limiter_blocked(&auth->limiter, peer, now)) {
        return write_status_json(out, cap, 429, "too_many_attempts",
                                 "Too many failed sign-in attempts. Try again later.");
    }
    root = cJSON_ParseWithLength(body, body_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) cJSON_Delete(root);
        return write_status_json(out, cap, 400, "invalid_json",
                                 "The request body is not valid JSON.");
    }
    id_item = cJSON_GetObjectItemCaseSensitive(root, "identifier");
    password_item = cJSON_GetObjectItemCaseSensitive(root, "password");
    service_item = cJSON_GetObjectItemCaseSensitive(root, "service");
    identifier = cJSON_IsString(id_item) ? id_item->valuestring : NULL;
    password = cJSON_IsString(password_item) ? password_item->valuestring : NULL;
    if (identifier == NULL || identifier[0] == '\0' || password == NULL
        || password[0] == '\0' || strlen(identifier) > 256
        || strlen(password) > 256) {
        cJSON_Delete(root);
        return write_status_json(out, cap, 400, "invalid_request",
                                 "A handle and an app password are required.");
    }
    if (platinum_policy_validate_service(
            cJSON_IsString(service_item) ? service_item->valuestring : NULL,
            service, sizeof service) != PLATINUM_POLICY_OK) {
        cJSON_Delete(root);
        return write_status_json(out, cap, 400, "invalid_service",
                                 "The service must be a public HTTPS hostname.");
    }

    agent = wf_agent_new(service);
    if (agent == NULL) {
        cJSON_Delete(root);
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }
    if (wf_agent_login_discovered(agent, identifier, password, &pds) != WF_OK) {
        wf_agent_free(agent);
        free(pds);
        cJSON_Delete(root);
        platinum_limiter_fail(&auth->limiter, peer, now);
        return write_status_json(out, cap, 401, "invalid_credentials",
                                 "The handle or app password is not valid.");
    }
    cJSON_Delete(root);
    if (make_installation_id(installation_id) != 0) {
        wf_agent_free(agent);
        free(pds);
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }
    memset(&data, 0, sizeof data);
    if (wf_agent_get_session_data(agent, &data) != WF_OK) {
        wf_agent_free(agent);
        free(pds);
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }
    snprintf(session_service, sizeof session_service, "%s",
             pds != NULL ? pds : service);
    json = password_session_json(session_service, &data);
    if (data.did == NULL || strlen(data.did) >= sizeof did_buf) {
        free(json);
        wf_agent_session_data_free(&data);
        free(pds);
        wf_agent_free(agent);
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }
    strcpy(did_buf, data.did);
    wf_agent_session_data_free(&data);
    free(pds);
    if (json == NULL || save_session(auth, installation_id, json) != 0) {
        free(json);
        wf_agent_free(agent);
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }
    free(json);

    if (platinum_token_store_add(&auth->tokens, random_bytes, did_buf,
                                 now, installation_id, token) < 0) {
        delete_session(auth, installation_id);
        wf_agent_free(agent);
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }
    if (platinum_token_store_save(&auth->tokens, auth->cfg.data_dir) != 0) {
        (void) platinum_token_store_revoke(&auth->tokens, token);
        delete_session(auth, installation_id);
        wf_agent_free(agent);
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }

    cache_put(auth, installation_id, agent, session_service);
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }
    cJSON_AddNumberToObject(reply, "protocol", 1);
    cJSON_AddStringToObject(reply, "token", token);
    cJSON_AddStringToObject(reply, "did", did_buf);
    cJSON_AddStringToObject(reply, "installationId", installation_id);
    response = cJSON_PrintUnformatted(reply);
    cJSON_Delete(reply);
    if (response == NULL) {
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The backend could not complete the request.");
    }
    write_json_string(out, cap, response);
    free(response);
    return 200;
}

int
platinum_auth_add_node_session(platinum_bridge_auth *auth, const char *service,
                               const char *token, const char *handle,
                               const char *did, char out_token[64],
                               char out_id[40])
{
    char installation_id[AUTH_ID_MAX];
    char *json;
    long now;

    if (auth == NULL || service == NULL || token == NULL || handle == NULL
        || did == NULL || out_token == NULL || out_id == NULL) {
        return -1;
    }
    if (make_installation_id(installation_id) != 0) return -1;
    json = node_session_json(service, token, handle, did);
    if (json == NULL) return -1;
    if (save_session(auth, installation_id, json) != 0) {
        free(json);
        return -1;
    }
    free(json);
    now = (long) time(NULL);
    if (platinum_token_store_add(&auth->tokens, random_bytes, did, now,
                                 installation_id, out_token) < 0) {
        delete_session(auth, installation_id);
        return -1;
    }
    if (platinum_token_store_save(&auth->tokens, auth->cfg.data_dir) != 0) {
        (void) platinum_token_store_revoke(&auth->tokens, out_token);
        delete_session(auth, installation_id);
        return -1;
    }
    strcpy(out_id, installation_id);
    return 0;
}

static void
html_escape(const char *s, char *out, size_t cap)
{
    size_t used = 0;

    if (cap == 0) return;
    for (; s != NULL && *s != '\0'; s++) {
        const char *rep = NULL;
        switch (*s) {
            case '&': rep = "&amp;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            case '"': rep = "&quot;"; break;
            case '\'': rep = "&#39;"; break;
            default: break;
        }
        if (rep != NULL) {
            size_t n = strlen(rep);
            if (used + n + 1 >= cap) break;
            memcpy(out + used, rep, n);
            used += n;
        } else {
            if (used + 2 >= cap) break;
            out[used++] = *s;
        }
    }
    out[used] = '\0';
}

int
platinum_auth_login_page(platinum_bridge_auth *auth, const char *handle,
                         char *out, size_t cap, const char **content_type)
{
    wf_xrpc_client *client;
    wf_oauth_pair_begin begin;
    char code[AUTH_PAIR_CODE_LEN + 1];
    char url[WF_OAUTH_PAIR_URL_MAX * 6];
    char label[512];
    long now;
    int written;
    size_t i;

    if (auth == NULL || out == NULL || cap == 0) return 500;
    out[0] = '\0';
    if (content_type != NULL) *content_type = "application/json; charset=utf-8";
    if (handle == NULL || handle[0] == '\0' || strlen(handle) > 256) {
        return write_status_json(out, cap, 400, "invalid_handle",
                                 "A Bluesky handle is required.");
    }
    for (i = 0; handle[i] != '\0'; i++) {
        if ((unsigned char) handle[i] < 0x20) {
            return write_status_json(out, cap, 400, "invalid_handle",
                                     "A Bluesky handle is required.");
        }
    }

    memset(&begin, 0, sizeof begin);
    client = wf_xrpc_client_new(auth->cfg.oauth_node_url);
    if (client == NULL) {
        return write_status_json(out, cap, 502, "oauth_unavailable",
                                 "The OAuth node is not reachable.");
    }
    {
        wf_status begin_status = WF_ERR_STATE;
        int attempt;

        for (attempt = 0; attempt < AUTH_BEGIN_ATTEMPTS; attempt++) {
            begin_status = wf_oauth_pair_begin_request(client, handle, &begin);
            if (begin_status == WF_OK) break;
        }
        if (begin_status != WF_OK) {
            fprintf(stderr, "platinum-bridge: OAuth begin failed (%d after %d attempts)\n",
                    (int) begin_status, AUTH_BEGIN_ATTEMPTS);
            wf_xrpc_client_free(client);
            return write_status_json(out, cap, 502, "oauth_unavailable",
                                     "The OAuth node could not start sign-in.");
        }
    }
    wf_xrpc_client_free(client);
    now = (long) time(NULL);
    if (begin.expires_at <= now) {
        return write_status_json(out, cap, 502, "oauth_unavailable",
                                 "The OAuth node returned an expired pairing.");
    }
    if (create_pending(auth, begin.pair_code, (long) begin.expires_at, code) != 0) {
        return write_status_json(out, cap, 503, "oauth_busy",
                                 "Too many sign-ins are waiting. Try again shortly.");
    }

    html_escape(begin.pair_url, url, sizeof url);
    html_escape(handle, label, sizeof label);
    written = snprintf(out, cap,
                       "<!doctype html><meta charset=utf-8>"
                       "<meta name=viewport content='width=device-width,initial-scale=1'>"
                       "<title>Platinum sign-in</title>"
                       "<body style='font:20px system-ui;max-width:42rem;margin:4rem auto;padding:1rem'>"
                       "<h1>Platinum</h1><p>Sign in as <strong>%s</strong> at "
                       "<a href='%s'>your PDS</a>. The OAuth node never asks for your "
                       "password here.</p><p>When the browser says the account is "
                       "connected, enter this code in Platinum:</p>"
                       "<p><code style='font-size:2rem;letter-spacing:.25rem'>%s</code></p>"
                       "<p>The code expires with the node pairing and can only be used once.</p>",
                       label, url, code);
    if (written < 0 || (size_t) written >= cap) {
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The sign-in page did not fit.");
    }
    if (content_type != NULL) *content_type = "text/html; charset=utf-8";
    return 200;
}

int
platinum_auth_pair(platinum_bridge_auth *auth, const char *body, size_t body_len,
                   char *out, size_t cap)
{
    cJSON *root;
    const cJSON *code_item;
    const char *code;
    platinum_auth_pending *pending;
    wf_xrpc_client *client;
    wf_oauth_pair_poll poll;
    wf_status status;
    char token[64];
    char installation_id[40];
    cJSON *reply;
    char *json;
    long now;

    if (auth == NULL || out == NULL || cap == 0) return 500;
    out[0] = '\0';
    if (body_len > (size_t) auth->cfg.max_body_bytes) {
        return write_status_json(out, cap, 413, "invalid_json",
                                 "The request body is too large.");
    }
    root = cJSON_ParseWithLength(body, body_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) cJSON_Delete(root);
        return write_status_json(out, cap, 400, "invalid_json",
                                 "The request body is not valid JSON.");
    }
    code_item = cJSON_GetObjectItemCaseSensitive(root, "code");
    code = cJSON_IsString(code_item) ? code_item->valuestring : NULL;
    if (code == NULL || strlen(code) != AUTH_PAIR_CODE_LEN) {
        cJSON_Delete(root);
        return write_status_json(out, cap, 400, "invalid_code",
                                 "The pairing code is invalid.");
    }
    now = (long) time(NULL);
    prune_pending(auth, now);
    pending = find_pending(auth, code);
    if (pending == NULL || pending->expires_at < now) {
        if (pending != NULL) memset(pending, 0, sizeof *pending);
        cJSON_Delete(root);
        return write_status_json(out, cap, 400, "invalid_code",
                                 "The pairing code is invalid or expired.");
    }
    cJSON_Delete(root);

    memset(&poll, 0, sizeof poll);
    client = wf_xrpc_client_new(auth->cfg.oauth_node_url);
    if (client == NULL) {
        return write_status_json(out, cap, 502, "oauth_unavailable",
                                 "The OAuth node is not reachable.");
    }
    status = wf_oauth_pair_poll_once(client, pending->node_code, &poll);
    wf_xrpc_client_free(client);
    if (status != WF_OK) {
        fprintf(stderr, "platinum-bridge: OAuth poll failed (%d)\n", (int) status);
        wf_oauth_pair_poll_wipe(&poll);
        return write_status_json(out, cap, 502, "oauth_unavailable",
                                 "The OAuth node could not answer.");
    }
    if (poll.state == WF_OAUTH_POLL_PENDING) {
        wf_oauth_pair_poll_wipe(&poll);
        return write_status_json(out, cap, 409, "pairing_pending",
                                 "Finish signing in in the browser, then try again.");
    }
    if (poll.state == WF_OAUTH_POLL_ERROR) {
        int written;
        cJSON *error = cJSON_CreateObject();
        if (error == NULL) {
            wf_oauth_pair_poll_wipe(&poll);
            return 500;
        }
        cJSON_AddStringToObject(error, "error", "pairing_failed");
        cJSON_AddStringToObject(error, "message",
                                poll.message[0] != '\0' ? poll.message
                                                        : "Sign-in did not complete.");
        json = cJSON_PrintUnformatted(error);
        cJSON_Delete(error);
        written = json != NULL ? 400 : 500;
        if (json != NULL) write_json_string(out, cap, json);
        free(json);
        wf_oauth_pair_poll_wipe(&poll);
        return written;
    }

    if (platinum_auth_add_node_session(auth, poll.service, poll.token, poll.handle,
                                       poll.did, token, installation_id) != 0) {
        wf_oauth_pair_poll_wipe(&poll);
        return write_status_json(out, cap, 500, "bridge_error",
                                 "The installation could not be stored.");
    }
    memset(pending, 0, sizeof *pending);
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        wf_oauth_pair_poll_wipe(&poll);
        return 500;
    }
    cJSON_AddNumberToObject(reply, "protocol", 1);
    cJSON_AddStringToObject(reply, "token", token);
    cJSON_AddStringToObject(reply, "did", poll.did);
    cJSON_AddStringToObject(reply, "installationId", installation_id);
    json = cJSON_PrintUnformatted(reply);
    cJSON_Delete(reply);
    wf_oauth_pair_poll_wipe(&poll);
    if (json == NULL) return 500;
    write_json_string(out, cap, json);
    free(json);
    return 200;
}

int
platinum_auth_revoke(platinum_bridge_auth *auth, const char *authorization,
                     char *out, size_t cap)
{
    char token[512];
    char installation_id[AUTH_ID_MAX];
    int slot;

    if (auth == NULL || out == NULL || cap == 0) return 500;
    out[0] = '\0';
    if (platinum_route_bearer_token(authorization, token, sizeof token) != 0) {
        return write_status_json(out, cap, 401, "missing_bearer_token",
                                 "A Platinum installation token is required.");
    }
    slot = platinum_token_store_authenticate(&auth->tokens, token, time(NULL));
    if (slot < 0) {
        return write_status_json(out, cap, 404, "invalid_token",
                                 "The Platinum installation token is not valid.");
    }
    strncpy(installation_id, auth->tokens.slots[slot].id,
            sizeof installation_id - 1);
    installation_id[sizeof installation_id - 1] = '\0';
    if (platinum_token_store_revoke(&auth->tokens, token) != 0) {
        return write_status_json(out, cap, 404, "invalid_token",
                                 "The Platinum installation token is not valid.");
    }
    delete_session(auth, installation_id);
    cache_remove(auth, installation_id);
    (void) platinum_token_store_save(&auth->tokens, auth->cfg.data_dir);
    write_json_string(out, cap, "{\"ok\":true}");
    return 200;
}
