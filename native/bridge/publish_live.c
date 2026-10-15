#include "publish_live.h"

#include <stdio.h>
#include <string.h>

#include <wolfram/agent.h>
#include <wolfram/threadgate_postgate.h>

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

    /* Replies are not wired: see the header. */
    if (reply != NULL) return PLATINUM_POST_ERROR;

    if (quote != NULL) {
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
