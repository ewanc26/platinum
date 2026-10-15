#include <stdio.h>
#include <string.h>

#include "policy.h"

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        fprintf(stderr, "%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void expect_service(const char *input, const char *want)
{
    char out[128];
    int rc = platinum_policy_validate_service(input, out, sizeof(out));

    checks++;
    if (rc != PLATINUM_POLICY_OK || strcmp(out, want) != 0) {
        failures++;
        fprintf(stderr, "validate(%s): rc=%d out=\"%s\", want \"%s\"\n",
                input == NULL ? "NULL" : input, rc, out, want);
    }
}

static void expect_reject(const char *input, int code)
{
    char out[128];
    int rc = platinum_policy_validate_service(input, out, sizeof(out));

    checks++;
    if (rc != code) {
        failures++;
        fprintf(stderr, "validate(%s): rc=%d, want %d\n", input, rc, code);
    }
    checks++;
    if (out[0] != '\0') {
        failures++;
        fprintf(stderr, "validate(%s): out not cleared on rejection\n", input);
    }
}

static void fail_times(platinum_failure_limiter *l, const char *key, int n, long now)
{
    int i;

    for (i = 0; i < n; i++) {
        platinum_limiter_fail(l, key, now);
    }
}

static void test_service(void)
{
    char out[128];

    /* accepts */
    expect_service(NULL, "https://bsky.social");
    expect_service("", "https://bsky.social");
    expect_service("   \t ", "https://bsky.social");
    expect_service("https://eurosky.social/", "https://eurosky.social");
    expect_service("https://eurosky.social", "https://eurosky.social");
    expect_service("  https://eurosky.social/xrpc?x#y  ", "https://eurosky.social");
    expect_service("https://bsky.social:443", "https://bsky.social");
    expect_service("https://bsky.social:", "https://bsky.social");
    expect_service("https://bsky.social.", "https://bsky.social.");
    expect_service("HTTPS://EuroSky.Social/", "https://eurosky.social");
    expect_service("https://PDS.Example.NET", "https://pds.example.net");

    /* rejects, one distinct code each */
    expect_reject("not a url", PLATINUM_POLICY_ERR_NOT_URL);
    expect_reject("https://", PLATINUM_POLICY_ERR_NOT_URL);
    expect_reject("https://bad host.example.com", PLATINUM_POLICY_ERR_NOT_URL);
    expect_reject("https://bsky.social:port", PLATINUM_POLICY_ERR_NOT_URL);
    expect_reject("http://bsky.social", PLATINUM_POLICY_ERR_NOT_HTTPS);
    expect_reject("ftp://x.com", PLATINUM_POLICY_ERR_NOT_HTTPS);
    expect_reject("https://user:pw@bsky.social", PLATINUM_POLICY_ERR_CREDENTIALS);
    expect_reject("https://user@bsky.social", PLATINUM_POLICY_ERR_CREDENTIALS);
    expect_reject("https://bsky.social:8443", PLATINUM_POLICY_ERR_PORT);
    expect_reject("https://bsky.social:0", PLATINUM_POLICY_ERR_PORT);
    expect_reject("https://127.0.0.1", PLATINUM_POLICY_ERR_IP_LITERAL);
    expect_reject("https://10.0.0.1:443", PLATINUM_POLICY_ERR_IP_LITERAL);
    expect_reject("https://127.1", PLATINUM_POLICY_ERR_IP_LITERAL);
    expect_reject("https://[::1]", PLATINUM_POLICY_ERR_IP_LITERAL);
    expect_reject("https://[2001:db8::1]:443", PLATINUM_POLICY_ERR_IP_LITERAL);
    expect_reject("localhost", PLATINUM_POLICY_ERR_NOT_URL);
    expect_reject("https://localhost", PLATINUM_POLICY_ERR_NOT_PUBLIC);
    expect_reject("https://foo.localhost", PLATINUM_POLICY_ERR_NOT_PUBLIC);
    expect_reject("https://box.local", PLATINUM_POLICY_ERR_NOT_PUBLIC);
    expect_reject("https://host.internal", PLATINUM_POLICY_ERR_NOT_PUBLIC);
    expect_reject("https://intranet", PLATINUM_POLICY_ERR_NOT_PUBLIC);
    expect_reject("https://intranet.", PLATINUM_POLICY_ERR_NOT_PUBLIC);
    expect_reject("https://localhost.", PLATINUM_POLICY_ERR_NOT_PUBLIC);

    /* output buffer handling */
    CHECK(platinum_policy_validate_service("https://eurosky.social", NULL, 10) == PLATINUM_POLICY_ERR_ARGS);
    CHECK(platinum_policy_validate_service("https://eurosky.social", out, 0) == PLATINUM_POLICY_ERR_ARGS);
    CHECK(platinum_policy_validate_service("https://eurosky.social", out, 22) == PLATINUM_POLICY_ERR_TOO_LONG);
    CHECK(platinum_policy_validate_service("https://eurosky.social", out, 23) == PLATINUM_POLICY_OK);
    CHECK(strcmp(out, "https://eurosky.social") == 0);
}

static void test_limiter(void)
{
    platinum_failure_limiter l;
    char key[32];
    int i;

    /* 5 failures block, default window */
    platinum_limiter_init(&l, PLATINUM_LIMITER_DEFAULT_MAX, PLATINUM_LIMITER_DEFAULT_WINDOW);
    CHECK(l.max == 5);
    CHECK(l.window == 600L);
    CHECK(platinum_limiter_blocked(&l, "ip-a", 100) == 0);
    fail_times(&l, "ip-a", 4, 100);
    CHECK(platinum_limiter_blocked(&l, "ip-a", 100) == 0);
    platinum_limiter_fail(&l, "ip-a", 100);
    CHECK(platinum_limiter_blocked(&l, "ip-a", 100) == 1);
    CHECK(platinum_limiter_blocked(&l, "ip-a", 699) == 1);

    /* the window expires: a stamp at t is counted while t > now - window */
    CHECK(platinum_limiter_blocked(&l, "ip-a", 700) == 0);

    /* separate keys are independent; NULL and empty keys never block */
    CHECK(platinum_limiter_blocked(&l, "ip-b", 100) == 0);
    CHECK(platinum_limiter_blocked(&l, NULL, 100) == 0);
    CHECK(platinum_limiter_blocked(&l, "", 100) == 0);
    platinum_limiter_fail(&l, NULL, 100);
    platinum_limiter_fail(&l, "", 100);
    CHECK(l.used == 1);

    /* failures in a window that has moved on do not count */
    platinum_limiter_init(&l, 3, 1000);
    CHECK(l.max == 3);
    fail_times(&l, "k", 2, 0);
    fail_times(&l, "k", 1, 1001);
    CHECK(platinum_limiter_blocked(&l, "k", 1001) == 0);
    fail_times(&l, "k", 2, 1001);
    CHECK(platinum_limiter_blocked(&l, "k", 1001) == 1);

    /* max is clamped to the 8 stamps kept per key, default when <= 0 */
    platinum_limiter_init(&l, 20, 600);
    CHECK(l.max == PLATINUM_LIMITER_STAMPS);
    platinum_limiter_init(&l, 0, 0);
    CHECK(l.max == 5);
    CHECK(l.window == 600L);

    /* eviction when the table is full keeps it bounded */
    platinum_limiter_init(&l, 5, 600);
    for (i = 0; i < PLATINUM_LIMITER_KEYS; i++) {
        sprintf(key, "k%d", i);
        fail_times(&l, key, 5, 1000 + i);
    }
    CHECK(l.used == PLATINUM_LIMITER_KEYS);
    CHECK(platinum_limiter_blocked(&l, "k0", 1100) == 1);
    CHECK(platinum_limiter_blocked(&l, "k1", 1100) == 1);

    /* k0 fails again, so its latest failure is now the newest; k1 is oldest */
    platinum_limiter_fail(&l, "k0", 2000);
    platinum_limiter_fail(&l, "new", 2000);
    CHECK(l.used == PLATINUM_LIMITER_KEYS);
    CHECK(platinum_limiter_blocked(&l, "k1", 1100) == 0);
    CHECK(platinum_limiter_blocked(&l, "k0", 2000) == 0);   /* 5 at 1000 are out of the 1400+ window */
    CHECK(platinum_limiter_blocked(&l, "k2", 1100) == 1);
    fail_times(&l, "new", 4, 2000);
    CHECK(platinum_limiter_blocked(&l, "new", 2000) == 1);

    /* many more distinct keys: the table never grows past its capacity */
    for (i = 0; i < 500; i++) {
        sprintf(key, "bulk%d", i);
        platinum_limiter_fail(&l, key, 3000 + i);
        CHECK(l.used <= PLATINUM_LIMITER_KEYS);
    }
    CHECK(l.used == PLATINUM_LIMITER_KEYS);
}

int main(void)
{
    test_service();
    test_limiter();

    if (failures != 0) {
        printf("test_policy: %d of %d checks FAILED\n", failures, checks);
        return 1;
    }
    printf("test_policy: all %d checks passed\n", checks);
    return 0;
}
