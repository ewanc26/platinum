#include "profile_live.h"

#include <wolfram/agent.h>

#include <string.h>

#include "profile_wolfram.h"

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
    if (st != WF_OK) {
        wf_agent_profile_free(&wp);
        return PLATINUM_FETCH_ERROR;
    }
    rc = platinum_profile_from_wolfram(&wp, actor == NULL, out);
    wf_agent_profile_free(&wp);
    return rc == 0 ? PLATINUM_FETCH_OK : PLATINUM_FETCH_ERROR;
}
