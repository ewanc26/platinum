#ifndef PLATINUM_PROFILE_H
#define PLATINUM_PROFILE_H

#include "bridge_client.h"

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
    char status[PLATINUM_PROFILE_STATUS_MAX + 1];
    int loading;
} platinum_profile;

enum {
    PLATINUM_PROFILE_NONE = 0,
    PLATINUM_PROFILE_CLOSE = 1
};

void platinum_profile_init(platinum_profile *profile);
wf_status platinum_profile_refresh(platinum_profile *profile,
                                   platinum_bridge_client *bridge);
OSErr platinum_profile_open(platinum_profile *profile);
void platinum_profile_close(platinum_profile *profile);
int platinum_profile_handle_event(platinum_profile *profile,
                                   EventRecord *event);
void platinum_profile_draw(platinum_profile *profile);

#ifdef __cplusplus
}
#endif

#endif
