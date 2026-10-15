#include "publish_live.h"

#include <stdio.h>
#include <string.h>

#include <cJSON.h>
#include <wolfram/agent.h>
#include <wolfram/feed_typed.h>
#include <wolfram/threadgate_postgate.h>

/* Copies a string member of obj into out (cap bytes). Returns 0, or -1 if it is missing
 * or does not fit. */
static int
string_member(const cJSON *obj, const char *key, char *out, size_t cap)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    size_t n;
    if (!cJSON_IsString(item) || item->valuestring == NULL) return -1;
    n = strlen(item->valuestring);
    if (n >= cap) return -1;
    memcpy(out, item->valuestring, n + 1);
    return 0;
}

/* Reads the parent post and works out the thread root the way api.ts does: the
 * parent's own reply.root if it is a reply, otherwise the parent itself. Returns the
 * platinum result, and fills parent_cid and the root. */
static platinum_post_result
thread_refs(wf_agent *agent, const char *parent_uri, char *parent_cid, size_t cid_cap,
            char *root_uri, size_t root_cap, char *root_cid, size_t root_cid_cap)
{
    wf_agent_post_list list;
    const char *uris[1];
    const wf_agent_post_view *post;
    const cJSON *record;
    const cJSON *reply;
    const cJSON *root;
    wf_status st;
    platinum_post_result out = PLATINUM_POST_OK;

    memset(&list, 0, sizeof list);
    uris[0] = parent_uri;
    st = wf_agent_get_posts_typed(agent, uris, 1, &list);
    if (st != WF_OK) {
        wf_agent_post_list_free(&list);
        return PLATINUM_POST_ERROR;
    }
    if (list.post_count == 0) {
        wf_agent_post_list_free(&list);
        return PLATINUM_POST_NOT_FOUND;
    }
    post = &list.posts[0];
    if (post->cid == NULL || strlen(post->cid) >= cid_cap) {
        out = PLATINUM_POST_ERROR;
    } else {
        strcpy(parent_cid, post->cid);
        record = post->record;
        reply = record != NULL ? cJSON_GetObjectItemCaseSensitive(record, "reply") : NULL;
        root = reply != NULL ? cJSON_GetObjectItemCaseSensitive(reply, "root") : NULL;
        if (root != NULL && string_member(root, "uri", root_uri, root_cap) == 0 &&
            string_member(root, "cid", root_cid, root_cid_cap) == 0) {
            /* A reply to a reply: the root is the thread's first post. */
        } else if (post->uri != NULL && strlen(post->uri) < root_cap && strlen(post->cid) < root_cid_cap) {
            strcpy(root_uri, post->uri);
            strcpy(root_cid, post->cid);
        } else {
            out = PLATINUM_POST_ERROR;
        }
    }
    wf_agent_post_list_free(&list);
    return out;
}

/* JSON-safe only: the uri and cid are written unescaped, so refuse any character
 * that would need escaping rather than write broken JSON. */
static int
plain_token(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    for (; *p; p++) {
        if (*p == '"' || *p == '\\' || *p < 0x20) return 0;
    }
    return 1;
}

platinum_post_result
platinum_publish_live(void *ctx, const char *text, const platinum_ref *reply,
                      const platinum_ref *quote, const char *gate, char *body, size_t cap)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_post_result res;
    wf_status st;
    int applied = 1;
    int gate_asked = gate != NULL && strcmp(gate, "everyone") != 0;
    int gate_supported = 0;
    wf_reply_gate wgate = WF_REPLY_GATE_EVERYONE;
    size_t need;

    if (agent == NULL || text == NULL || body == NULL || cap == 0) return PLATINUM_POST_ERROR;
    body[0] = '\0';
    memset(&res, 0, sizeof res);

    if (reply != NULL) {
        char parent_cid[256];
        char root_uri[512];
        char root_cid[256];
        platinum_post_result rr = thread_refs(agent, reply->uri, parent_cid, sizeof parent_cid,
                                              root_uri, sizeof root_uri, root_cid, sizeof root_cid);
        if (rr != PLATINUM_POST_OK) return rr;
        st = wf_agent_reply_refs(agent, text, root_uri, root_cid, reply->uri, parent_cid, &res);
    } else if (quote != NULL) {
        st = wf_agent_quote(agent, text, quote->uri, quote->cid, &res);
    } else {
        st = wf_agent_post(agent, text, &res);
    }
    if (st == WF_ERR_NOT_FOUND) {
        wf_agent_post_result_free(&res);
        return PLATINUM_POST_NOT_FOUND;
    }
    if (st != WF_OK || res.uri == NULL || res.cid == NULL) {
        return PLATINUM_POST_ERROR;
    }
    if (!plain_token(res.uri) || !plain_token(res.cid)) {
        return PLATINUM_POST_ERROR;
    }

    if (gate_asked) {
        if (strcmp(gate, "nobody") == 0) {
            wgate = WF_REPLY_GATE_NOBODY;
            gate_supported = 1;
        } else {
            gate_supported = 0;
        }
        if (gate_supported) {
            applied = wf_agent_set_reply_gate(agent, res.uri, wgate) == WF_OK;
        } else {
            applied = 0;
        }
    }

    need = strlen(res.uri) + strlen(res.cid) + 64;
    if (need >= cap) {
        wf_agent_post_result_free(&res);
        return PLATINUM_POST_ERROR;
    }
    if (applied || !gate_asked) {
        sprintf(body, "{\"uri\":\"%s\",\"cid\":\"%s\"}", res.uri, res.cid);
    } else {
        sprintf(body, "{\"uri\":\"%s\",\"cid\":\"%s\",\"replyGateApplied\":false}", res.uri, res.cid);
    }
    wf_agent_post_result_free(&res);
    return PLATINUM_POST_OK;
}
