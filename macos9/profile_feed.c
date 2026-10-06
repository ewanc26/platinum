/*
 * profile_feed.c -- loading and parsing a profile, and following. No QuickDraw
 * here, so it links on a host for tests; the window is profile.c.
 */
#include "profile.h"
#include "text_codec.h"

#include "json_min.h"
#include <string.h>

static const char kNoProfile[] = "No profile information is available.";
static const char kLoading[] = "Loading profile...";

/*
 * Copy a string member of `root` into `destination`, converting it to
 * MacRoman on the way in.
 *
 * A member that is absent, or that is present but not a string, leaves the
 * destination empty. That is deliberate rather than lax: the bridge is trusted
 * to send the documented shape, and a field of the wrong type is a response
 * this client does not understand, so showing nothing is better than showing
 * whatever the parser happened to find there.
 */
static void profile_copy(char *destination,
                         long capacity,
                         platinum_json root,
                         const char *name)
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

static long profile_number(platinum_json root, const char *name)
{
    long value = 0;

    if (platinum_json_int(root, name, &value) != WF_OK)
        return 0;

    return value;
}

/* Status text only. profile.c wraps this to also invalidate the window, which
 * this file must not touch. */
void platinum_profile_status_text(platinum_profile *profile,
                                  const char *status)
{
    long length;

    if (profile == NULL)
        return;

    profile->status[0] = '\0';
    if (status == NULL)
        return;

    length = (long)strlen(status);
    if (length > PLATINUM_PROFILE_STATUS_MAX)
        length = PLATINUM_PROFILE_STATUS_MAX;

    memcpy(profile->status, status, (size_t)length);
    profile->status[length] = '\0';
}

void platinum_profile_init(platinum_profile *profile)
{
    if (profile == NULL)
        return;

    memset(profile, 0, sizeof(*profile));
    platinum_profile_status_text(profile, kNoProfile);
}

wf_status platinum_profile_load(platinum_profile *profile,
                                platinum_bridge_client *bridge,
                                const char *actor)
{
    wf_response response;
    platinum_json root;
    platinum_json pinned;
    char path[64 + 3 * 256];
    char did[PLATINUM_PROFILE_DID_MAX + 1];
    int flag;
    wf_status status;

    if (profile == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;

    strcpy(path, "/v1/profile");
    if (actor != NULL && actor[0] != '\0') {
        /* A handle is shown as @handle in the timeline; the bridge wants it
         * bare. */
        if (actor[0] == '@')
            ++actor;
        strcat(path, "?actor=");
        if (platinum_bridge_query_escape(actor, path + strlen(path),
                                         (long)(sizeof(path) - strlen(path)))
            < 0)
            return WF_ERR_INVALID_ARG;
    } else {
        actor = NULL;
    }

    memset(&response, 0, sizeof(response));
    profile->loading = 1;
    platinum_profile_status_text(profile, kLoading);

    status = platinum_bridge_get(bridge, path, &response);
    if (status != WF_OK) {
        profile->loading = 0;
        if (status == WF_ERR_AUTH)
            platinum_profile_status_text(
                profile, "Session expired. Pair the account again.");
        else if (response.status == 404)
            platinum_profile_status_text(profile, "No such account.");
        else
            platinum_profile_status_text(profile, "Profile refresh failed.");
        wf_response_free(&response);
        return status;
    }

    status = platinum_json_open(&root,
                               response.body != NULL ? response.body : "");
    if (status != WF_OK) {
        profile->loading = 0;
        platinum_profile_status_text(profile, "The bridge returned invalid profile data.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }

    /* The DID identifies the account for following, so it is read strictly: a
     * value that does not fit is no DID at all. It is also the only way a
     * profile counts as "someone else's" with follow state. */
    did[0] = '\0';
    if (platinum_json_string(root, "did", did, sizeof(did)) != WF_OK)
        did[0] = '\0';
    profile_copy(profile->did, sizeof(profile->did), root, "did");
    strcpy(profile->target_did, did);
    profile_copy(profile->handle, sizeof(profile->handle), root, "handle");
    profile_copy(profile->display_name, sizeof(profile->display_name), root,
                 "displayName");
    profile_copy(profile->description, sizeof(profile->description), root,
                 "description");
    profile->followers_count = profile_number(root, "followersCount");
    profile->follows_count = profile_number(root, "followsCount");
    profile->posts_count = profile_number(root, "postsCount");

    /* follow state is only present on someone else's profile. */
    profile->other = (actor != NULL);
    profile->following = 0;
    profile->followed_by = 0;
    if (profile->other) {
        if (platinum_json_bool(root, "following", &flag) == WF_OK)
            profile->following = flag;
        if (platinum_json_bool(root, "followedBy", &flag) == WF_OK)
            profile->followed_by = flag;
    }

    profile->has_pinned = 0;
    if (platinum_json_member(root, "pinned", &pinned) == WF_OK &&
        platinum_timeline_parse_post(&profile->pinned, pinned))
        profile->has_pinned = 1;

    /* Every member is read while the response is still alive, and the cursor
     * only ever pointed into response.body, so nothing here can be left
     * holding into a freed buffer. */
    wf_response_free(&response);

    profile->loading = 0;
    if (profile->handle[0] == '\0' && profile->did[0] == '\0')
        platinum_profile_status_text(profile, kNoProfile);
    else
        platinum_profile_status_text(profile, NULL);

    return WF_OK;
}

wf_status platinum_profile_refresh(platinum_profile *profile,
                                   platinum_bridge_client *bridge)
{
    return platinum_profile_load(profile, bridge, NULL);
}

wf_status platinum_profile_set_follow(platinum_profile *profile,
                                      platinum_bridge_client *bridge,
                                      int on)
{
    char did[PLATINUM_PROFILE_DID_MAX * 2 + 2];
    char body[PLATINUM_PROFILE_DID_MAX * 2 + 48];
    wf_response response;
    platinum_json root;
    int now_on;
    wf_status status;

    if (profile == NULL || bridge == NULL)
        return WF_ERR_INVALID_ARG;
    if (!profile->other || profile->target_did[0] == '\0') {
        platinum_profile_status_text(profile,
                                     "Open someone else's profile to follow them.");
        return WF_ERR_INVALID_ARG;
    }
    if (platinum_json_escape(did, sizeof(did), profile->target_did) != WF_OK)
        return WF_ERR_INVALID_ARG;

    strcpy(body, "{\"did\":\"");
    strcat(body, did);
    strcat(body, on ? "\",\"on\":true}" : "\",\"on\":false}");

    memset(&response, 0, sizeof(response));
    status = platinum_bridge_post(bridge, "/v1/follow", body, &response);
    if (status != WF_OK) {
        if (status == WF_ERR_AUTH)
            platinum_profile_status_text(
                profile, "Session expired. Pair the account again.");
        else
            platinum_profile_status_text(profile,
                                         "The follow did not go through.");
        wf_response_free(&response);
        return status;
    }

    /* Trust the bridge's answer, and only a well-formed one. */
    if (platinum_json_open(&root, response.body != NULL ? response.body : "")
            != WF_OK ||
        platinum_json_bool(root, "on", &now_on) != WF_OK) {
        platinum_profile_status_text(profile,
                                     "The bridge returned an invalid reply.");
        wf_response_free(&response);
        return WF_ERR_PARSE;
    }
    wf_response_free(&response);

    profile->following = now_on;
    platinum_profile_status_text(profile, NULL);
    return WF_OK;
}
