/*
 * test_compose.c -- host-side tests for what a compose window is replying to or
 * quoting, and who may reply. Links the real compose_model.c and the bridge
 * client's gate names. Not Classic Mac OS 9 validation: the window is compose.c.
 */

#include <stdio.h>
#include <string.h>

#include "bridge_client.h"
#include "compose.h"

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

static platinum_compose compose;

static void test_quote_and_gate(void)
{
    memset(&compose, 0, sizeof(compose));
    check(platinum_compose_set_quote(&compose, "at://did:plc:a/app.bsky.feed.post/3k", "cq", "bob.test") == 1,
          "a quote is set");
    check(strcmp(compose.caption, "Quoting @bob.test") == 0, "the caption names who");
    check(strcmp(compose.quote_uri, "at://did:plc:a/app.bsky.feed.post/3k") == 0 &&
              strcmp(compose.quote_cid, "cq") == 0 && compose.reply_uri[0] == '\0',
          "the quote carries its exact identifiers and no reply");

    check(platinum_compose_cycle_gate(&compose) == 1 && compose.reply_gate == 1,
          "a quote can set a reply gate");
    compose.reply_gate = PLATINUM_REPLY_GATE_COUNT - 1;
    platinum_compose_cycle_gate(&compose);
    check(compose.reply_gate == 0, "the gate wraps back to everyone");

    compose.reply_gate = 3;
    check(platinum_compose_set_reply(&compose, "at://r", "cr", "@amy.test") == 1,
          "a reply replaces the quote");
    check(compose.quote_uri[0] == '\0' && compose.quote_cid[0] == '\0' && compose.reply_gate == 0,
          "and drops the quote and the gate");
    check(strcmp(compose.caption, "Replying to @amy.test") == 0, "the @ is not doubled");
    check(platinum_compose_cycle_gate(&compose) == 0 && compose.reply_gate == 0,
          "a reply has no gate to change");

    check(platinum_compose_set_quote(&compose, "at://q", "cq", NULL) == 1 &&
              strcmp(compose.caption, "Quoting a post") == 0 && compose.reply_uri[0] == '\0',
          "quoting replaces a reply, and a missing handle gets a plain caption");
}

static void test_refusals(void)
{
    static char big[600];

    memset(&compose, 0, sizeof(compose));
    check(platinum_compose_set_quote(&compose, "", "c", "x") == 0 &&
              platinum_compose_set_quote(&compose, "at://q", NULL, "x") == 0,
          "an empty uri or missing cid is refused");
    check(compose.quote_uri[0] == '\0' && compose.caption[0] == '\0', "and leaves a plain post");
    memset(big, 'a', 550);
    big[550] = '\0';
    check(platinum_compose_set_quote(&compose, big, "c", "x") == 0 &&
              platinum_compose_set_reply(&compose, big, "c", "x") == 0,
          "an identifier that does not fit is refused");
    check(platinum_compose_set_quote(NULL, "a", "b", "c") == 0 && platinum_compose_cycle_gate(NULL) == 0,
          "NULL is safe");
}

int main(void)
{
    test_quote_and_gate();
    test_refusals();
    if (failures != 0) {
        printf("test_compose: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_compose: all %d checks passed\n", checks);
    return 0;
}
