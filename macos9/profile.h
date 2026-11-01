#ifndef PLATINUM_PROFILE_H
#define PLATINUM_PROFILE_H

#include "bridge_client.h"
#include "state_panel.h"
#include "timeline.h"

#include <Events.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_PROFILE_DID_MAX 255
#define PLATINUM_PROFILE_HANDLE_MAX 255
#define PLATINUM_PROFILE_NAME_MAX 96
#define PLATINUM_PROFILE_DESCRIPTION_MAX 511
#define PLATINUM_PROFILE_STATUS_MAX 127

typedef struct platinum_profile {
    WindowPtr window;
    char did[PLATINUM_PROFILE_DID_MAX + 1];
    char handle[PLATINUM_PROFILE_HANDLE_MAX + 1];
    char display_name[PLATINUM_PROFILE_NAME_MAX + 1];
    char description[PLATINUM_PROFILE_DESCRIPTION_MAX + 1];
    long followers_count;
    long follows_count;
    long posts_count;
    /* Someone else's profile: the follow state below is meaningful. */
    int other;
    int following;
    int followed_by;
    int muted;
    int blocking;
    /* Block asks twice: set by the first request, cleared by anything else. */
    int block_armed;
    /* The account's DID, copied exactly (the display copy above may clip). */
    char target_did[PLATINUM_PROFILE_DID_MAX + 1];
    int has_pinned;
    platinum_post_preview pinned;
    char status[PLATINUM_PROFILE_STATUS_MAX + 1];
    /* Sign-in or bridge notice, drawn as the shared state panel. */
    platinum_panel panel;
    int loading;
} platinum_profile;

enum {
    PLATINUM_PROFILE_NONE = 0,
    PLATINUM_PROFILE_CLOSE = 1,
    PLATINUM_PROFILE_SIGN_IN = 2,
    PLATINUM_PROFILE_PAIR = 3
};

void platinum_profile_init(platinum_profile *profile);
/* Your own profile. */
wf_status platinum_profile_refresh(platinum_profile *profile,
                                   platinum_bridge_client *bridge);
/* Another account, by handle (with or without @) or DID; NULL or empty is your
 * own. Follow state and the pinned post come with it. */
wf_status platinum_profile_load(platinum_profile *profile,
                                platinum_bridge_client *bridge,
                                const char *actor);
/*
 * GET app.bsky.actor.getProfile for the actor (handle with or without @, or
 * DID; NULL or empty is the signed-in account), filling the same model:
 * viewer.following/followedBy as presence, muted as a boolean, blocking as
 * presence, the pinned post from pinnedPost. Follow, mute and block writes
 * stay on the bridge for now.
 */
wf_status platinum_profile_load_native(platinum_profile *profile,
                                       platinum_atproto_client *client,
                                       const char *actor);
wf_status platinum_profile_refresh_native(platinum_profile *profile,
                                          platinum_atproto_client *client);
/* Follow or unfollow the account shown, idempotently. Only for someone else's
 * profile. On failure the shown state is unchanged. */
wf_status platinum_profile_set_follow(platinum_profile *profile,
                                      platinum_bridge_client *bridge,
                                      int on);
enum {
    PLATINUM_RELATION_FOLLOW = 0,
    PLATINUM_RELATION_MUTE = 1,
    PLATINUM_RELATION_BLOCK = 2
};
/* Follow, mute or block (or undo it) on the account shown, idempotently. The
 * follow, mute and block states change only on a well-formed answer. */
wf_status platinum_profile_set_relation(platinum_profile *profile,
                                        platinum_bridge_client *bridge,
                                        int kind, int on);
/* Set status text without touching any window. */
void platinum_profile_status_text(platinum_profile *profile,
                                  const char *status);
OSErr platinum_profile_open(platinum_profile *profile);
void platinum_profile_close(platinum_profile *profile);
int platinum_profile_handle_event(platinum_profile *profile,
                                   EventRecord *event);
void platinum_profile_draw(platinum_profile *profile);
void platinum_profile_set_status(platinum_profile *profile,
                                 const char *status);

#ifdef __cplusplus
}
#endif

#endif
