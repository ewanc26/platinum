/*
 * test_profile.c -- host-side tests for loading a profile (your own or someone
 * else's) and following. Links the real profile_feed.c, the timeline post
 * parser and the bridge client over the stubbed Wolfram transport. Not Classic
 * Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

#include "atproto_client.h"
#include "profile.h"
#include "wolfram_stub.h"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static void respond(int status, long http, const char *body)
{
    fake_status = status;
    fake_http_status = http;
    fake_body = body;
    last_url[0] = '\0';
    last_body[0] = '\0';
    last_method = 0;
}

static const char *kOther =
    "{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\",\"displayName\":\"Bob\","
    "\"description\":\"hi\",\"followersCount\":3,\"followsCount\":4,\"postsCount\":5,"
    "\"following\":true,\"followedBy\":false,\"pinned\":{\"uri\":\"at://p/1\","
    "\"cid\":\"c1\",\"author\":{\"did\":\"did:plc:bob\"},\"text\":\"pinned words\"}}";

static void test_other(void)
{
    static platinum_profile profile;
    platinum_bridge_client *bridge;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_profile_init(&profile);

    respond(WF_OK, 200, kOther);
    check(platinum_profile_load(&profile, bridge, "@Bob.test") == WF_OK, "loads");
    check(strstr(last_url, "/v1/profile?actor=Bob.test") != NULL,
          "the @ is dropped and the actor encoded");
    check(profile.other == 1 && profile.following == 1 && profile.followed_by == 0,
          "follow state read");
    check(strcmp(profile.target_did, "did:plc:bob") == 0, "DID kept exactly");
    check(profile.followers_count == 3 && strcmp(profile.display_name, "Bob") == 0,
          "ordinary fields read");
    check(profile.has_pinned == 1 && strstr(profile.pinned.line1, "pinned words") != NULL,
          "pinned post read");

    respond(WF_OK, 200, "{\"did\":\"did:plc:me\",\"handle\":\"me.test\","
                        "\"following\":true}");
    check(platinum_profile_refresh(&profile, bridge) == WF_OK, "own profile loads");
    check(strstr(last_url, "actor") == NULL, "own profile sends no actor");
    check(profile.other == 0 && profile.following == 0,
          "follow state is ignored on your own profile");
    check(profile.has_pinned == 0, "no pinned post when none is sent");

    respond(WF_OK, 200, "{\"did\":\"did:plc:bob\",\"following\":\"true\"}");
    platinum_profile_load(&profile, bridge, "bob.test");
    check(profile.following == 0, "a string \"true\" is not a boolean");

    respond(WF_ERR_HTTP, 404, "{\"error\":\"actor_not_found\"}");
    check(platinum_profile_load(&profile, bridge, "nobody.test") != WF_OK &&
              strcmp(profile.status, "No such account.") == 0,
          "an unknown account says so");

    respond(WF_OK, 200, "not json");
    check(platinum_profile_load(&profile, bridge, "bob.test") == WF_ERR_PARSE,
          "invalid data is a parse error");

    platinum_bridge_client_free(bridge);
}

static void test_follow(void)
{
    static platinum_profile profile;
    platinum_bridge_client *bridge;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_profile_init(&profile);
    respond(WF_OK, 200, kOther);
    platinum_profile_load(&profile, bridge, "bob.test");

    respond(WF_OK, 200, "{\"did\":\"did:plc:bob\",\"on\":false}");
    check(platinum_profile_set_follow(&profile, bridge, 0) == WF_OK, "unfollow succeeds");
    check(strstr(last_url, "/v1/follow") != NULL && last_method == 2, "posts to /v1/follow");
    check(strcmp(last_body, "{\"did\":\"did:plc:bob\",\"on\":false}") == 0, "exact body");
    check(profile.following == 0, "state follows the answer");

    respond(WF_ERR_HTTP, 500, "{}");
    check(platinum_profile_set_follow(&profile, bridge, 1) != WF_OK && profile.following == 0,
          "a failed follow changes nothing");
    check(strcmp(profile.status, "The follow did not go through.") == 0, "and says so");

    respond(WF_OK, 200, "{\"did\":\"did:plc:bob\"}");
    check(platinum_profile_set_follow(&profile, bridge, 1) == WF_ERR_PARSE &&
              profile.following == 0,
          "an answer without \"on\" is not trusted");

    respond(WF_OK, 200, "{\"did\":\"did:plc:me\",\"handle\":\"me.test\"}");
    platinum_profile_refresh(&profile, bridge);
    last_method = 0;
    check(platinum_profile_set_follow(&profile, bridge, 1) == WF_ERR_INVALID_ARG &&
              last_method == 0,
          "you cannot follow yourself: no request");

    platinum_bridge_client_free(bridge);
}

static void test_mute_block(void)
{
    static platinum_profile profile;
    platinum_bridge_client *bridge;

    bridge = platinum_bridge_client_new("https://bridge.example");
    platinum_profile_init(&profile);
    respond(WF_OK, 200,
            "{\"did\":\"did:plc:bob\",\"handle\":\"bob.test\",\"muted\":true,"
            "\"blocking\":false,\"following\":false,\"followedBy\":false}");
    platinum_profile_load(&profile, bridge, "bob.test");
    check(profile.muted == 1 && profile.blocking == 0, "mute and block state read");

    respond(WF_OK, 200, "{\"did\":\"did:plc:bob\",\"on\":false}");
    check(platinum_profile_set_relation(&profile, bridge, PLATINUM_RELATION_MUTE, 0) == WF_OK &&
              profile.muted == 0 && strstr(last_url, "/v1/mute") != NULL,
          "unmute posts to /v1/mute and clears the state");

    respond(WF_OK, 200, "{\"did\":\"did:plc:bob\",\"on\":true}");
    check(platinum_profile_set_relation(&profile, bridge, PLATINUM_RELATION_BLOCK, 1) == WF_OK &&
              profile.blocking == 1 && profile.muted == 0 && profile.following == 0 &&
              strstr(last_url, "/v1/block") != NULL &&
              strcmp(last_body, "{\"did\":\"did:plc:bob\",\"on\":true}") == 0,
          "block posts to /v1/block and touches only blocking");

    respond(WF_ERR_HTTP, 500, "{}");
    check(platinum_profile_set_relation(&profile, bridge, PLATINUM_RELATION_BLOCK, 0) != WF_OK &&
              profile.blocking == 1 &&
              strcmp(profile.status, "The block did not go through.") == 0,
          "a failed unblock changes nothing and says so");

    platinum_bridge_client_free(bridge);
}

static const char *native_profile_body;

static void native_profile_script(int call, int method, const char *url,
                                  const char *body)
{
    (void)call;
    (void)method;
    (void)url;
    (void)body;
    fake_http_status = 200;
    fake_status = WF_OK;
    fake_body = native_profile_body;
}

static platinum_atproto_client *native_profile_client(void)
{
    platinum_atproto_session session;
    platinum_atproto_client *client;

    client = platinum_atproto_client_new("https://pds.example");
    if (client == NULL)
        return NULL;
    memset(&session, 0, sizeof(session));
    strcpy(session.access_jwt, "access");
    strcpy(session.refresh_jwt, "refresh");
    strcpy(session.did, "did:plc:me");
    strcpy(session.handle, "me.test");
    strcpy(session.pds_url, "https://pds.example");
    if (platinum_atproto_client_set_session(client, &session) != WF_OK) {
        platinum_atproto_client_free(client);
        return NULL;
    }
    return client;
}

static void test_native_profile(void)
{
    static platinum_profile profile;
    static char body[4096];
    platinum_atproto_client *client;

    client = native_profile_client();
    if (client == NULL)
        return;
    platinum_profile_init(&profile);

    strcpy(body, "{\"did\":\"did:plc:bob\",");
    strcat(body, "\"handle\":\"bob.test\",\"displayName\":\"Bob\",");
    strcat(body, "\"description\":\"hi\",\"followersCount\":3,\"followsCount\":4,");
    strcat(body, "\"postsCount\":5,\"viewer\":{\"following\":\"at://did:plc:me/app.bsky.graph.follow/f1\",");
    strcat(body, "\"followedBy\":\"at://did:plc:bob/app.bsky.graph.follow/f2\",\"muted\":true,");
    strcat(body, "\"blocking\":\"at://did:plc:bob/app.bsky.graph.block/b1\"},");
    strcat(body, "\"pinnedPost\":{\"uri\":\"at://did:plc:bob/app.bsky.feed.post/p1\",");
    strcat(body, "\"cid\":\"c1\",\"author\":{\"did\":\"did:plc:bob\"},");
    strcat(body, "\"record\":{\"text\":\"pinned words\"},\"viewer\":{}}}");
    native_profile_body = body;
    fake_script = native_profile_script;
    check(platinum_profile_load_native(&profile, client, "@bob.test") == WF_OK,
          "native profile loads");
    check(strcmp(last_nsid, "app.bsky.actor.getProfile") == 0,
          "native profile calls getProfile");
    check(profile.other == 1 && profile.following == 1 && profile.followed_by == 1,
          "viewer presence is follow state");
    check(profile.muted == 1 && profile.blocking == 1, "mute and block read");
    check(strcmp(profile.target_did, "did:plc:bob") == 0, "DID kept exactly");
    check(profile.followers_count == 3 && strcmp(profile.display_name, "Bob") == 0,
          "ordinary fields read");
    check(profile.has_pinned == 1 && strstr(profile.pinned.line1, "pinned words") != NULL,
          "pinnedPost read as a postView");
    fake_script = NULL;

    strcpy(body, "{\"did\":\"did:plc:me\",\"handle\":\"me.test\"}");
    native_profile_body = body;
    fake_script = native_profile_script;
    check(platinum_profile_refresh_native(&profile, client) == WF_OK,
          "own profile loads");
    check(profile.other == 0 && profile.following == 0,
          "follow state ignored on your own profile");
    check(strcmp(last_param_values[0], "did:plc:me") == 0,
          "own profile asks for the session DID");
    fake_script = NULL;

    check(platinum_profile_load_native(&profile, client, "@") == WF_ERR_INVALID_ARG,
          "a bare @ is refused");

    platinum_atproto_client_free(client);
}

int main(void)
{
    test_mute_block();
    test_native_profile();
    fake_script = NULL;
    test_other();
    test_follow();
    if (failures != 0) {
        printf("test_profile: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_profile: all %d checks passed\n", checks);
    return 0;
}
