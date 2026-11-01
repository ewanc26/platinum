/*
 * profile_native.c -- the profile's native page fetch.
 *
 * Kept apart from profile_feed.c so the bridge path and its tests do not pull
 * in the session stack. It speaks app.bsky.actor.getProfile through the
 * session client and fills the same profile model: the DID strictly, text as
 * MacRoman, counts, follow state from the viewer (following/followedBy as
 * presence, muted as a boolean, blocking as presence) and the pinned post
 * from pinnedPost. Follow, mute and block writes stay on the bridge for now.
 */

#include "profile.h"

#include <string.h>

#include "text_codec.h"

static void profile_native_copy(char *destination, long capacity,
                                platinum_json root, const char *name)
{
    char utf8[PLATINUM_TEXT_UTF8_CAPACITY];
    long length;

    if (destination == NULL || capacity <= 0)
        return;
    destination[0] = '\0';
    if (platinum_json_string_truncating(root, name, utf8, sizeof(utf8))
        != WF_OK)
        return;
    length = platinum_text_utf8_to_macroman(utf8, destination, capacity, NULL);
    if (length < 0)
        destination[0] = '\0';
}

static long profile_native_number(platinum_json root, const char *name)
{
    long value = 0;

    if (platinum_json_int(root, name, &value) != WF_OK)
        return 0;
    return value;
}

wf_status platinum_profile_load_native(platinum_profile *profile,
                                       platinum_atproto_client *client,
                                       const char *actor)
{
    const platinum_atproto_session *session;
    const char *names[1];
    const char *values[1];
    wf_response response;
    platinum_json root;
    platinum_json viewer;
    platinum_json pinned;
    char did[PLATINUM_PROFILE_DID_MAX + 1];
    const char *query_actor;
    int is_other;
    int flag;
    wf_status status;

    if (profile == NULL || client == NULL)
        return WF_ERR_INVALID_ARG;

    session = platinum_atproto_client_session(client);
    if (session == NULL)
        return WF_ERR_INVALID_ARG;

    is_other = (actor != NULL && actor[0] != '\0');
    query_actor = actor;
    if (!is_other) {
        if (session->did[0] == '\0')
            return WF_ERR_INVALID_ARG;
        query_actor = session->did;
    } else if (query_actor[0] == '@') {
        ++query_actor;
        if (query_actor[0] == '\0')
            return WF_ERR_INVALID_ARG;
    }

    memset(&response, 0, sizeof(response));
    profile->loading = 1;
    platinum_profile_status_text(profile, "Loading profile...");

    names[0] = "actor";
    values[0] = query_actor;
    status = platinum_atproto_client_get(client,
                                         "app.bsky.actor.getProfile",
                                         names, values, 1, &response);
    if (status != WF_OK) {
        profile->loading = 0;
        if (status == WF_ERR_AUTH) {
            platinum_profile_status_text(profile,
                                         "Session expired. Sign in again.");
        } else if (status == WF_ERR_NOT_FOUND || response.status == 404) {
            platinum_profile_status_text(profile, "No such account.");
        } else {
            platinum_profile_status_text(profile, "Profile refresh failed.");
        }
        wf_response_free(&response);
        return status;
    }

    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
        != WF_OK) {
        profile->loading = 0;
        platinum_profile_status_text(
            profile, "The server returned invalid profile data.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    did[0] = '\0';
    if (platinum_json_string(root, "did", did, sizeof(did)) != WF_OK)
        did[0] = '\0';
    profile_native_copy(profile->did, sizeof(profile->did), root, "did");
    strcpy(profile->target_did, did);

    profile_native_copy(profile->handle, sizeof(profile->handle), root,
                        "handle");
    profile_native_copy(profile->display_name,
                        sizeof(profile->display_name), root, "displayName");
    profile_native_copy(profile->description, sizeof(profile->description),
                        root, "description");
    profile->followers_count =
        profile_native_number(root, "followersCount");
    profile->follows_count =
        profile_native_number(root, "followsCount");
    profile->posts_count = profile_native_number(root, "postsCount");

    profile->other = is_other;
    profile->following = 0;
    profile->followed_by = 0;
    profile->muted = 0;
    profile->blocking = 0;
    profile->block_armed = 0;
    if (profile->other &&
        platinum_json_member(root, "viewer", &viewer) == WF_OK) {
        platinum_json member;

        if (platinum_json_member(viewer, "following", &member) == WF_OK)
            profile->following = 1;
        if (platinum_json_member(viewer, "followedBy", &member) == WF_OK)
            profile->followed_by = 1;
        if (platinum_json_bool(viewer, "muted", &flag) == WF_OK)
            profile->muted = flag;
        if (platinum_json_member(viewer, "blocking", &member) == WF_OK)
            profile->blocking = 1;
    }

    profile->has_pinned = 0;
    if (platinum_json_member(root, "pinnedPost", &pinned) == WF_OK &&
        platinum_timeline_parse_view(&profile->pinned, pinned))
        profile->has_pinned = 1;

    wf_response_free(&response);

    profile->loading = 0;
    if (profile->handle[0] == '\0' && profile->did[0] == '\0')
        platinum_profile_status_text(
            profile, "No profile information is available.");
    else
        platinum_profile_status_text(profile, NULL);

    return WF_OK;
}

wf_status platinum_profile_refresh_native(platinum_profile *profile,
                                          platinum_atproto_client *client)
{
    if (profile == NULL || client == NULL)
        return WF_ERR_INVALID_ARG;
    return platinum_profile_load_native(profile, client, NULL);
}
