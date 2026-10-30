#include "profile_live.h"

#include <ctype.h>
#include <string.h>

#include <wolfram/agent.h>

#include <string.h>

#include "profile_wolfram.h"

/* True when the XRPC error text says the account does not exist. The PDS answers an
 * unknown actor with HTTP 400 and the message "Profile not found". */
static int
says_not_found(const char *message)
{
    static const char needle[] = "not found";
    size_t n = sizeof needle - 1;
    const char *p;

    if (message == NULL) return 0;
    for (p = message; *p != '\0'; p++) {
        size_t i;
        for (i = 0; i < n && p[i] != '\0' && tolower((unsigned char) p[i]) == needle[i]; i++) {
        }
        if (i == n) return 1;
    }
    return 0;
}

platinum_fetch_result
platinum_profile_fetch_live(void *ctx, const char *actor, platinum_profile *out)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_profile wp;
    const char *target;
    wf_status st;
    int rc;

    if (agent == NULL || out == NULL) return PLATINUM_FETCH_ERROR;
    target = actor != NULL ? actor : wf_agent_get_did(agent);
    if (target == NULL) return PLATINUM_FETCH_ERROR;

    memset(&wp, 0, sizeof wp);
    st = wf_agent_get_profile(agent, target, &wp);
    if (st == WF_ERR_NOT_FOUND) return PLATINUM_FETCH_NOT_FOUND;
    /* api.ts maps any 400 for another account to "no such account". Wolfram reports the
     * PDS's 400 as WF_ERR_HTTP, so the message is what identifies an unknown account. */
    if (st == WF_ERR_HTTP && actor != NULL && says_not_found(wf_agent_last_error(agent))) {
        wf_agent_profile_free(&wp);
        return PLATINUM_FETCH_NOT_FOUND;
    }
    if (st != WF_OK) {
        wf_agent_profile_free(&wp);
        return PLATINUM_FETCH_ERROR;
    }
    rc = platinum_profile_from_wolfram(&wp, actor == NULL, out);
    wf_agent_profile_free(&wp);
    return rc == 0 ? PLATINUM_FETCH_OK : PLATINUM_FETCH_ERROR;
}
