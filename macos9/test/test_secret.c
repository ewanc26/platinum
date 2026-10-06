/*
 * test_secret.c -- host-side tests for the masked password buffer and for the
 * app-password sign-in request. Links the real secret.c, text_codec.c and bridge
 * client over the stubbed Wolfram transport. It checks what this code controls:
 * the entry buffer and the sign-in scratch area are wiped. It cannot see the
 * transport's own copy of the request, which is Wolfram's. Not Classic Mac OS 9
 * validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge_client.h"
#include "secret.h"
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

static int all_zero(const char *data, size_t size)
{
    size_t i;

    for (i = 0; i < size; ++i)
        if (data[i] != 0)
            return 0;
    return 1;
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

static void test_entry(void)
{
    static platinum_secret secret;
    char mask[16];
    char utf8[64];
    const char *word = "abcd-efgh";
    long i;

    platinum_secret_init(&secret);
    for (i = 0; word[i] != '\0'; ++i)
        platinum_secret_key(&secret, (unsigned char)word[i]);
    check(platinum_secret_length(&secret) == 9, "characters accumulate");
    check(platinum_secret_mask(&secret, mask, sizeof(mask)) == 9 &&
              (unsigned char)mask[0] == 0xA5 && strchr(mask, 'a') == NULL &&
              mask[9] == '\0',
          "the mask is bullets and never the text");
    check(platinum_secret_mask(&secret, mask, 9) == -1, "a mask that does not fit is refused");

    check(platinum_secret_key(&secret, 8) == 1 && platinum_secret_length(&secret) == 8, "backspace removes one");
    check(platinum_secret_key(&secret, 13) == 0 && platinum_secret_key(&secret, 9) == 0 &&
              platinum_secret_length(&secret) == 8,
          "control characters are ignored");
    check(platinum_secret_utf8(&secret, utf8, sizeof(utf8)) == 8 && strcmp(utf8, "abcd-efg") == 0,
          "read out as UTF-8");
    check(platinum_secret_utf8(&secret, utf8, 4) == -1, "an output that does not fit is refused");

    platinum_secret_wipe(&secret);
    check(platinum_secret_length(&secret) == 0 && all_zero(secret.data, sizeof(secret.data)),
          "wipe zeroes every byte");
    check(platinum_secret_utf8(&secret, utf8, sizeof(utf8)) == -1 &&
              platinum_secret_key(&secret, 8) == 0,
          "an empty secret reads as nothing and backspace on it does nothing");

    for (i = 0; i < PLATINUM_SECRET_MAX + 10; ++i)
        platinum_secret_key(&secret, 'x');
    check(platinum_secret_length(&secret) == PLATINUM_SECRET_MAX, "the buffer is bounded");
    platinum_secret_wipe(&secret);
    check(platinum_secret_key(NULL, 'a') == 0 && platinum_secret_length(NULL) == 0, "NULL is safe");
}

static const char *kPaired =
    "{\"protocol\":1,\"token\":\"tok123\",\"did\":\"did:plc:me\",\"installationId\":\"inst1\"}";

static void test_login(void)
{
    platinum_bridge_client *client;
    platinum_bridge_pairing pairing;
    int reason;
    static char pw[40];

    client = platinum_bridge_client_new("https://bridge.example");
    strcpy(pw, "abcd-efgh-ijkl-mnop");

    respond(WF_OK, 200, kPaired);
    check(platinum_bridge_login_app_password(client, "me.test", pw, &pairing, &reason) == WF_OK,
          "sign-in succeeds");
    check(strcmp(last_url, "https://bridge.example/v1/login/app-password") == 0 && last_method == 2,
          "posts to the app-password route");
    check(strcmp(last_body, "{\"identifier\":\"me.test\",\"password\":\"abcd-efgh-ijkl-mnop\"}") == 0,
          "the body carries the handle and the password");
    check(pairing.token != NULL && strcmp(pairing.token, "tok123") == 0 &&
              strcmp(pairing.did, "did:plc:me") == 0,
          "the answer is read like pairing");
    check(platinum_bridge_login_scratch_clear(), "the scratch area is zeroed after success");
    check(strcmp(pw, "abcd-efgh-ijkl-mnop") == 0, "the caller's own copy is left for the caller to wipe");
    platinum_bridge_pairing_free(&pairing);

    respond(WF_ERR_HTTP, 401, "{\"error\":\"invalid_credentials\",\"message\":\"x\"}");
    check(platinum_bridge_login_app_password(client, "me.test", pw, &pairing, &reason) == WF_ERR_HTTP &&
              reason == PLATINUM_LOGIN_INVALID && pairing.token == NULL,
          "a refusal says why and yields no token");
    check(platinum_bridge_login_scratch_clear(), "the scratch area is zeroed after a refusal");

    respond(WF_ERR_HTTP, 403, "{\"error\":\"app_password_disabled\"}");
    platinum_bridge_login_app_password(client, "me.test", pw, &pairing, &reason);
    check(reason == PLATINUM_LOGIN_DISABLED, "disabled is recognised");
    respond(WF_ERR_HTTP, 429, "{\"error\":\"too_many_attempts\"}");
    platinum_bridge_login_app_password(client, "me.test", pw, &pairing, &reason);
    check(reason == PLATINUM_LOGIN_TOO_MANY, "rate limiting is recognised");
    respond(WF_ERR_HTTP, 500, "not json");
    platinum_bridge_login_app_password(client, "me.test", pw, &pairing, &reason);
    check(reason == PLATINUM_LOGIN_OTHER, "anything else is other");

    respond(WF_OK, 200, "{\"protocol\":1,\"token\":\"t\"}");
    check(platinum_bridge_login_app_password(client, "me.test", pw, &pairing, &reason) != WF_OK &&
              pairing.token == NULL && platinum_bridge_login_scratch_clear(),
          "a malformed success is refused and the scratch is clean");

    respond(WF_OK, 200, kPaired);
    check(platinum_bridge_login_app_password(client, "", pw, &pairing, NULL) == WF_ERR_INVALID_ARG &&
              platinum_bridge_login_app_password(client, "me.test", "", &pairing, NULL) == WF_ERR_INVALID_ARG &&
              platinum_bridge_login_app_password(client, "me.test", "a\nb", &pairing, NULL) == WF_ERR_INVALID_ARG &&
              last_method == 0,
          "empty fields and control characters are refused with no request");
    {
        static char big[300];
        memset(big, 'a', 280);
        big[280] = '\0';
        check(platinum_bridge_login_app_password(client, "me.test", big, &pairing, NULL) == WF_ERR_INVALID_ARG &&
                  last_method == 0 && platinum_bridge_login_scratch_clear(),
              "an over-long password is refused with no request");
    }

    strcpy(pw, "p\"w\\x");
    respond(WF_OK, 200, kPaired);
    platinum_bridge_login_app_password(client, "me.test", pw, &pairing, NULL);
    check(strcmp(last_body, "{\"identifier\":\"me.test\",\"password\":\"p\\\"w\\\\x\"}") == 0,
          "quotes and backslashes are escaped");
    platinum_bridge_pairing_free(&pairing);

    platinum_bridge_wipe(pw, sizeof(pw));
    check(all_zero(pw, sizeof(pw)), "the wipe helper zeroes what it is given");
    platinum_bridge_wipe(NULL, 5);
    platinum_bridge_client_free(client);
}

int main(void)
{
    test_entry();
    test_login();
    if (failures != 0) {
        printf("test_secret: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_secret: all %d checks passed\n", checks);
    return 0;
}
