/*
 * test_atproto_url.c -- host tests for the native AT Protocol URL builder.
 *
 * These are dialect and logic checks, not Mac OS 9 validation. The module has
 * no Toolbox dependency, so the same code the Mac target compiles is exercised
 * here against hostile and boundary inputs.
 */

#include <stdio.h>
#include <string.h>

#include "atproto_url.h"

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

static void check_status(wf_status got, wf_status want, const char *what)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s (got %d, want %d)\n", what, (int)got, (int)want);
    }
}

static void check_str(const char *got, const char *want, const char *what)
{
    checks++;
    if (got == NULL || strcmp(got, want) != 0) {
        failures++;
        printf("FAIL: %s (got \"%s\", want \"%s\")\n", what,
               got != NULL ? got : "(null)", want);
    }
}

static void test_service_base(void)
{
    char out[PLATINUM_ATPROTO_URL_MAX];

    check_status(platinum_atproto_service_base("https://bsky.social", out,
                                               sizeof(out)),
                 WF_OK, "plain service base");
    check_str(out, "https://bsky.social", "plain service base value");

    check_status(platinum_atproto_service_base("HTTPS://BSky.Social/", out,
                                               sizeof(out)),
                 WF_OK, "scheme and host are case-insensitive");
    check_str(out, "https://bsky.social", "lowercased and slash removed");

    check_status(platinum_atproto_service_base("https://example.test:8443",
                                               out, sizeof(out)),
                 WF_OK, "explicit port");
    check_str(out, "https://example.test:8443", "port kept");

    check_status(platinum_atproto_service_base("http://bsky.social", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "plain http refused");
    check_status(platinum_atproto_service_base("https://user@bsky.social", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "userinfo refused");
    check_status(platinum_atproto_service_base("https://bsky.social/xrpc", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "path refused");
    check_status(platinum_atproto_service_base("https://bsky.social?x=1", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "query refused");
    check_status(platinum_atproto_service_base("https://bsky.social#f", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "fragment refused");
    check_status(platinum_atproto_service_base("https://", out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "empty host refused");
    check_status(platinum_atproto_service_base("https://-bad.test", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "leading hyphen label refused");
    check_status(platinum_atproto_service_base("https://bad-.test", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "trailing hyphen label refused");
    check_status(platinum_atproto_service_base("https://a..b", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "empty label refused");
    check_status(platinum_atproto_service_base("https://host:0", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "port zero refused");
    check_status(platinum_atproto_service_base("https://host:65536", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "port too large refused");
    check_status(platinum_atproto_service_base("https://host:abc", out,
                                               sizeof(out)),
                 WF_ERR_INVALID_ARG, "non-numeric port refused");
    check_status(platinum_atproto_service_base(NULL, out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "NULL input refused");
    check_status(platinum_atproto_service_base("https://bsky.social", out, 1),
                 WF_ERR_ALLOC, "no room refused");
    check_str(out, "", "failure empties the output");
}

static void test_escape(void)
{
    char out[64];

    check_status(platinum_atproto_escape("AZaz09-._~", out, sizeof(out)),
                 WF_OK, "unreserved kept");
    check_str(out, "AZaz09-._~", "unreserved value");

    check_status(platinum_atproto_escape("a b/c?d=e&f", out, sizeof(out)),
                 WF_OK, "reserved escaped");
    check_str(out, "a%20b%2Fc%3Fd%3De%26f", "reserved value");

    check_status(platinum_atproto_escape("caf\xc3\xa9", out, sizeof(out)),
                 WF_OK, "non-ASCII escaped");
    check_str(out, "caf%C3%A9", "non-ASCII value");

    check_status(platinum_atproto_escape("", out, sizeof(out)), WF_OK,
                 "empty value allowed");
    check_str(out, "", "empty value");

    check_status(platinum_atproto_escape("abc", out, 4), WF_OK,
                 "exactly fitting value");
    check_str(out, "abc", "exactly fitting value text");
    check_status(platinum_atproto_escape("abc", out, 3), WF_ERR_ALLOC,
                 "one byte short refused");
}

static void test_xrpc_url(void)
{
    char out[PLATINUM_ATPROTO_URL_MAX];
    const char *names[PLATINUM_ATPROTO_PARAMS_MAX + 1];
    const char *values[PLATINUM_ATPROTO_PARAMS_MAX + 1];
    int index;

    names[0] = "limit";
    values[0] = "20";
    names[1] = "cursor";
    values[1] = "abc:def";
    check_status(platinum_atproto_xrpc_url("https://bsky.social/",
                                           "app.bsky.feed.getTimeline", names,
                                           values, 2, out, sizeof(out)),
                 WF_OK, "query built");
    check_str(out,
              "https://bsky.social/xrpc/app.bsky.feed.getTimeline"
              "?limit=20&cursor=abc%3Adef",
              "query value");

    names[0] = "uri";
    values[0] = "at://did:plc:x/app.bsky.feed.post/1";
    check_status(platinum_atproto_xrpc_url("https://pds.example",
                                           "app.bsky.feed.getPostThread", names,
                                           values, 1, out, sizeof(out)),
                 WF_OK, "single parameter");
    check_str(out,
              "https://pds.example/xrpc/app.bsky.feed.getPostThread"
              "?uri=at%3A%2F%2Fdid%3Aplc%3Ax%2Fapp.bsky.feed.post%2F1",
              "AT URI parameter encoded");

    check_status(platinum_atproto_xrpc_url("https://bsky.social",
                                           "App.bsky.feed.getTimeline", NULL,
                                           NULL, 0, out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "uppercase NSID refused");
    check_status(platinum_atproto_xrpc_url("https://bsky.social",
                                           "app.bsky", NULL, NULL, 0, out,
                                           sizeof(out)),
                 WF_ERR_INVALID_ARG, "two-segment NSID refused");
    check_status(platinum_atproto_xrpc_url("http://bsky.social",
                                           "app.bsky.feed.getTimeline", NULL,
                                           NULL, 0, out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "plain http base refused");

    names[0] = "cursor";
    values[0] = "";
    check_status(platinum_atproto_xrpc_url("https://bsky.social",
                                           "app.bsky.feed.getTimeline", names,
                                           values, 1, out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "empty value refused");

    for (index = 0; index <= PLATINUM_ATPROTO_PARAMS_MAX; index++) {
        names[index] = "a";
        values[index] = "b";
    }
    check_status(platinum_atproto_xrpc_url("https://bsky.social",
                                           "app.bsky.feed.getTimeline", names,
                                           values,
                                           PLATINUM_ATPROTO_PARAMS_MAX + 1, out,
                                           sizeof(out)),
                 WF_ERR_INVALID_ARG, "too many parameters refused");

    names[0] = "limit";
    values[0] = "20";
    check_status(platinum_atproto_xrpc_url("https://bsky.social",
                                           "app.bsky.feed.getTimeline", names,
                                           values, 1, out, 20),
                 WF_ERR_ALLOC, "short output refused");
}

static void test_dids(void)
{
    char long_did[300];

    check(platinum_atproto_valid_did("did:plc:abc123"), "plc DID valid");
    check(platinum_atproto_valid_did("did:web:example.com"), "web DID valid");
    check(platinum_atproto_valid_did("did:key:z6Mkabc_DEF-1"), "key DID valid");
    check(!platinum_atproto_valid_did("DID:plc:abc"), "uppercase scheme invalid");
    check(!platinum_atproto_valid_did("did:PLC:abc"), "uppercase method invalid");
    check(!platinum_atproto_valid_did("did:plc:"), "empty id invalid");
    check(!platinum_atproto_valid_did("did::abc"), "empty method invalid");
    check(!platinum_atproto_valid_did("did:plc:ab c"), "space invalid");
    check(!platinum_atproto_valid_did(NULL), "NULL DID invalid");

    memset(long_did, 'a', sizeof(long_did));
    memcpy(long_did, "did:plc:", 8);
    long_did[sizeof(long_did) - 1] = '\0';
    check(!platinum_atproto_valid_did(long_did), "over-long DID invalid");
}

static void test_handles(void)
{
    char label[70];
    char handle[300];

    check(platinum_atproto_valid_handle("bsky.app"), "simple handle valid");
    check(platinum_atproto_valid_handle("foo-bar.example.com"),
          "hyphenated handle valid");
    check(platinum_atproto_valid_handle("BSKY.APP"),
          "handle case accepted");
    check(!platinum_atproto_valid_handle("foo"), "single label invalid");
    check(!platinum_atproto_valid_handle("-foo.com"), "leading hyphen invalid");
    check(!platinum_atproto_valid_handle("foo-.com"), "trailing hyphen invalid");
    check(!platinum_atproto_valid_handle("foo..com"), "empty label invalid");
    check(!platinum_atproto_valid_handle("foo_bar.com"), "underscore invalid");
    check(!platinum_atproto_valid_handle("foo.com."), "trailing dot invalid");
    check(!platinum_atproto_valid_handle(""), "empty handle invalid");

    memset(label, 'a', sizeof(label));
    label[64] = '\0';
    strcpy(handle, label);
    strcat(handle, ".test");
    check(!platinum_atproto_valid_handle(handle), "64-byte label invalid");
    label[63] = '\0';
    strcpy(handle, label);
    strcat(handle, ".test");
    check(platinum_atproto_valid_handle(handle), "63-byte label valid");
}

static void test_nsids(void)
{
    check(platinum_atproto_valid_nsid("app.bsky.feed.getTimeline"),
          "timeline NSID valid");
    check(platinum_atproto_valid_nsid("com.atproto.server.createSession"),
          "createSession NSID valid");
    check(platinum_atproto_valid_nsid("a.b.c"), "minimum segments valid");
    check(!platinum_atproto_valid_nsid("app.bsky"), "two segments invalid");
    check(!platinum_atproto_valid_nsid("App.bsky.feed.x"),
          "uppercase invalid");
    check(!platinum_atproto_valid_nsid("app..feed.x"),
          "empty segment invalid");
    check(!platinum_atproto_valid_nsid("app.bsky.feed."),
          "trailing dot invalid");
    check(!platinum_atproto_valid_nsid("app.bsky.1feed.x"),
          "segment starting with a digit invalid");
    check(!platinum_atproto_valid_nsid("app.bsky.feed.get-Timeline"),
          "hyphen before a capital invalid");
    check(!platinum_atproto_valid_nsid(NULL), "NULL NSID invalid");
}

static void test_at_uris(void)
{
    check(platinum_atproto_valid_at_uri(
              "at://did:plc:abc123/app.bsky.feed.post/3kabc"),
          "DID AT URI valid");
    check(platinum_atproto_valid_at_uri(
              "at://bsky.app/app.bsky.feed.post/3kabc"),
          "handle AT URI valid");
    check(!platinum_atproto_valid_at_uri(
              "at://did:plc:abc123/app.bsky.feed.post"),
          "missing rkey invalid");
    check(!platinum_atproto_valid_at_uri(
              "at://did:plc:abc123/app.bsky.feed.post/3k/extra"),
          "extra path invalid");
    check(!platinum_atproto_valid_at_uri(
              "at://did:plc:abc123/app.bsky.feed.post/3k?x=1"),
          "query invalid");
    check(!platinum_atproto_valid_at_uri(
              "at://did:plc:abc123/app.bsky/3k"),
          "bad collection invalid");
    check(!platinum_atproto_valid_at_uri(
              "http://did:plc:abc123/app.bsky.feed.post/3k"),
          "wrong scheme invalid");
    check(!platinum_atproto_valid_at_uri(NULL), "NULL URI invalid");
}

int main(void)
{
    test_service_base();
    test_escape();
    test_xrpc_url();
    test_dids();
    test_handles();
    test_nsids();
    test_at_uris();

    if (failures != 0) {
        printf("test_atproto_url: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_atproto_url: all %d checks passed\n", checks);
    return 0;
}
