/*
 * test_profile.c -- host-side tests for loading a profile (your own or someone
 * else's) and following. Links the real profile_feed.c, the timeline post
 * parser and the bridge client over the stubbed Wolfram transport. Not Classic
 * Mac OS 9 validation.
 */

#include <stdio.h>
#include <string.h>

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

int main(void)
{
    test_other();
    test_follow();
    if (failures != 0) {
        printf("test_profile: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_profile: all %d checks passed\n", checks);
    return 0;
}
