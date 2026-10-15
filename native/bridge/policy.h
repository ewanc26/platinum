/*
 * App-password policy: which service URLs the bridge will connect to, and how
 * many failed sign-ins a key may make in a window. Mirrors
 * bridge/src/auth/app-password.ts (validateService, FailureLimiter).
 */
#ifndef PLATINUM_POLICY_H
#define PLATINUM_POLICY_H

#include <stddef.h>

#define PLATINUM_DEFAULT_SERVICE "https://bsky.social"

/* Return codes. 0 is success; every rejection is negative and distinct. */
#define PLATINUM_POLICY_OK                0
#define PLATINUM_POLICY_ERR_NOT_URL      (-1)  /* not parseable as scheme://authority */
#define PLATINUM_POLICY_ERR_NOT_HTTPS    (-2)  /* scheme is not https */
#define PLATINUM_POLICY_ERR_CREDENTIALS  (-3)  /* userinfo present */
#define PLATINUM_POLICY_ERR_PORT         (-4)  /* explicit port other than 443 */
#define PLATINUM_POLICY_ERR_IP_LITERAL   (-5)  /* IPv4 dotted decimal or [IPv6] */
#define PLATINUM_POLICY_ERR_NOT_PUBLIC   (-6)  /* no dot, localhost, *.localhost, *.local, *.internal */
#define PLATINUM_POLICY_ERR_TOO_LONG     (-7)  /* input or host longer than supported, or out_size too small */
#define PLATINUM_POLICY_ERR_ARGS         (-8)  /* out_origin NULL or out_size 0 */

/*
 * Validates a client-supplied service URL and writes its origin
 * ("https://" + lowercased host, no port, no path, no trailing slash).
 *
 * input NULL, or only whitespace (including ""), means PLATINUM_DEFAULT_SERVICE.
 * Surrounding whitespace is trimmed. The host is ASCII letters, digits, '-'
 * and '.'; anything else is PLATINUM_POLICY_ERR_NOT_URL.
 *
 * Returns PLATINUM_POLICY_OK on success, or a negative PLATINUM_POLICY_ERR_*.
 * On failure out_origin, if out_size > 0, is set to the empty string.
 */
int platinum_policy_validate_service(const char *input, char *out_origin, size_t out_size);

/*
 * Failure limiter: at most `max` failures per key within `window_seconds`.
 * A success never resets it (there is no success call).
 *
 * Bounds: 64 keys, each key up to 127 bytes (longer keys are truncated, so
 * two keys sharing their first 127 bytes share a counter, which only ever
 * over-blocks), and 8 failure timestamps per key. Because only 8 are kept,
 * init clamps max to 1..8. max <= 0 selects the default 5, and
 * window_seconds <= 0 selects the default 600.
 *
 * Eviction: when all 64 slots are in use and a new key fails, the key whose
 * most recent failure is oldest is evicted, so memory stays bounded. A key
 * that is already in the table is never evicted by this.
 *
 * A NULL or empty key is never blocked and its failures are ignored.
 * `now` is passed in (seconds); the limiter reads no clock.
 */
#define PLATINUM_LIMITER_KEYS          64
#define PLATINUM_LIMITER_KEY_LEN      128
#define PLATINUM_LIMITER_STAMPS         8
#define PLATINUM_LIMITER_DEFAULT_MAX    5
#define PLATINUM_LIMITER_DEFAULT_WINDOW 600L

typedef struct platinum_limiter_entry {
    int in_use;
    int count;                                   /* stamps in use, oldest first */
    char key[PLATINUM_LIMITER_KEY_LEN];
    long when[PLATINUM_LIMITER_STAMPS];
} platinum_limiter_entry;

typedef struct platinum_failure_limiter {
    int max;
    long window;
    int used;                                    /* slots in use, <= PLATINUM_LIMITER_KEYS */
    platinum_limiter_entry entries[PLATINUM_LIMITER_KEYS];
} platinum_failure_limiter;

void platinum_limiter_init(platinum_failure_limiter *l, int max, long window_seconds);
int platinum_limiter_blocked(platinum_failure_limiter *l, const char *key, long now);   /* 1 blocked, 0 not */
void platinum_limiter_fail(platinum_failure_limiter *l, const char *key, long now);

#endif /* PLATINUM_POLICY_H */
