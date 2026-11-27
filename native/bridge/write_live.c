#include "write_live.h"

#include <string.h>

#include <wolfram/agent.h>
#include <wolfram/feed_typed.h>

platinum_write_result
platinum_toggle_live(void *ctx, int kind, const char *uri, const char *cid, int on,
                     int *out_on, long *out_count)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_agent_post_list list;
    const wf_agent_post_view *post;
    const char *uris[1];
    const char *existing;
    long count;
    wf_status st = WF_OK;

    if (out_on != NULL) *out_on = 0;
    if (out_count != NULL) *out_count = 0;
    if (agent == NULL || uri == NULL || cid == NULL || out_on == NULL || out_count == NULL ||
        (kind != 0 && kind != 1)) {
        return PLATINUM_WRITE_ERROR;
    }

    memset(&list, 0, sizeof list);
    uris[0] = uri;
    if (wf_agent_get_posts_typed(agent, uris, 1, &list) != WF_OK) {
        wf_agent_post_list_free(&list);
        return PLATINUM_WRITE_ERROR;
    }
    /* The post is gone: api.ts answers on false with a zero count. */
    if (list.post_count == 0) {
        wf_agent_post_list_free(&list);
        return PLATINUM_WRITE_OK;
    }

    post = &list.posts[0];
    if (kind == 0) {
        existing = post->viewer.like;
        count = post->has_like_count ? post->like_count : 0;
    } else {
        existing = post->viewer.repost;
        count = post->has_repost_count ? post->repost_count : 0;
    }
    if (existing != NULL && existing[0] == '\0') existing = NULL;

    if (on && existing == NULL) {
        wf_agent_post_result res;

        /* The client's cid, as api.ts sends it. */
        memset(&res, 0, sizeof res);
        st = kind == 0 ? wf_agent_like(agent, uri, cid, &res)
                       : wf_agent_repost(agent, uri, cid, &res);
        wf_agent_post_result_free(&res);
        count += 1;
    } else if (!on && existing != NULL) {
        st = kind == 0 ? wf_agent_unlike(agent, existing)
                       : wf_agent_delete_repost(agent, existing);
        count = count > 0 ? count - 1 : 0;
    }
    wf_agent_post_list_free(&list);
    if (st != WF_OK) return PLATINUM_WRITE_ERROR;

    *out_on = on ? 1 : 0;
    *out_count = count;
    return PLATINUM_WRITE_OK;
}

platinum_write_result
platinum_subject_live(void *ctx, int kind, const char *did, int on)
{
    wf_agent *agent = (wf_agent *) ctx;
    wf_response res;
    wf_agent_profile profile;
    wf_status st;

    if (agent == NULL || did == NULL || kind < 0 || kind > 2) return PLATINUM_WRITE_ERROR;

    /* The raw call keeps the HTTP status, so a 400 can be told apart from any other
     * failure: api.ts answers 400 from getProfile as "no such account". */
    memset(&res, 0, sizeof res);
    st = wf_agent_get_profile_raw(agent, did, &res);
    if (st == WF_ERR_HTTP && res.status == 400) {
        wf_response_free(&res);
        return PLATINUM_WRITE_NOT_FOUND;
    }
    if (st != WF_OK || res.body == NULL) {
        wf_response_free(&res);
        return PLATINUM_WRITE_ERROR;
    }
    memset(&profile, 0, sizeof profile);
    st = wf_agent_parse_profile(res.body, res.body_len, &profile);
    wf_response_free(&res);
    if (st != WF_OK) {
        wf_agent_profile_free(&profile);
        return PLATINUM_WRITE_ERROR;
    }

    st = WF_OK;
    if (kind == 1) {
        /* Muting is private to the account and leaves no record. */
        if (on && !profile.muted) st = wf_agent_mute_actor(agent, did);
        else if (!on && profile.muted) st = wf_agent_unmute_actor(agent, did);
    } else {
        const char *existing = kind == 0 ? profile.following : profile.blocking;

        if (existing != NULL && existing[0] == '\0') existing = NULL;
        if (on && existing == NULL) {
            wf_agent_post_result created;

            memset(&created, 0, sizeof created);
            st = kind == 0 ? wf_agent_follow(agent, did, &created)
                           : wf_agent_block(agent, did, &created);
            wf_agent_post_result_free(&created);
        } else if (!on && existing != NULL) {
            st = kind == 0 ? wf_agent_unfollow(agent, existing)
                           : wf_agent_unblock(agent, existing);
        }
    }
    wf_agent_profile_free(&profile);
    return st == WF_OK ? PLATINUM_WRITE_OK : PLATINUM_WRITE_ERROR;
}

platinum_content_result
platinum_delete_live(void *ctx, const char *uri)
{
    static const char prefix[] = "at://";
    wf_agent *agent = (wf_agent *) ctx;
    const char *me;
    const char *authority;
    const char *slash;
    size_t n;

    if (agent == NULL || uri == NULL) return PLATINUM_CONTENT_ERROR;
    if (strncmp(uri, prefix, sizeof prefix - 1) != 0) return PLATINUM_CONTENT_NOT_YOURS;

    /* api.ts compares the URI's repository with the account before any request. */
    authority = uri + sizeof prefix - 1;
    slash = strchr(authority, '/');
    me = wf_agent_get_did(agent);
    if (slash == NULL || me == NULL) return PLATINUM_CONTENT_NOT_YOURS;
    n = (size_t) (slash - authority);
    if (n != strlen(me) || strncmp(authority, me, n) != 0) return PLATINUM_CONTENT_NOT_YOURS;

    /* deleteRecord of a record that is already gone succeeds: the state asked for is
     * the state it is in. */
    return wf_agent_delete_post(agent, uri) == WF_OK ? PLATINUM_CONTENT_OK
                                                     : PLATINUM_CONTENT_ERROR;
}
