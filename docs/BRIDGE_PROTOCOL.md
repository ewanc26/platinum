# Platinum Bridge Protocol

Platinum is deliberately split into two layers:

    Mac OS 9 / Platinum
            |
        HTTP + JSON
            |
      Platinum Bridge
            |
       AT Protocol
            |
        PDS / AppView

The Mac application never needs to implement modern OAuth, DPoP, TLS, DID
resolution or the full AT Protocol stack.

## Pairing

1. Open the bridge's /login?handle=... URL in a modern browser.
2. Complete AT Protocol OAuth.
3. The bridge displays a short-lived pairing code.
4. Platinum sends POST /v1/pair with {"code":"..."}.
5. The bridge returns protocol version 1, a bridge token, the account DID, and an installation ID.
6. Platinum stores the bridge token and uses it for authenticated requests.

The pairing code is single-use and expires after ten minutes. OAuth refresh
credentials remain on the bridge and are never sent to Mac OS 9.

## App-password sign-in

A separate path from OAuth, for accounts where browser pairing is not wanted.
It is **off by default**: the operator sets `PLATINUM_BRIDGE_ALLOW_APP_PASSWORD=1`,
because it means the bridge, and the Mac client on the way to it, handle an
app password. Over plain HTTP that password is readable on the network between
the Mac and the bridge; use it only over HTTPS or a network you trust.

`POST /v1/login/app-password` with `{"identifier":"handle","password":"app password","service":"https://bsky.social"}`
(`service` optional, public https hostnames only) returns exactly what
`/v1/pair` returns: `{protocol, token, did, installationId}`. The password is
held in memory for that one call and is never stored or logged; the PDS session
is kept on the bridge in `app-password-sessions.json` (mode 0600), and revoking
the token deletes it. Errors: `403 app_password_disabled`, `400 invalid_service`,
`400 invalid_request`, `401 invalid_credentials` (fixed text, no upstream
detail), `429 too_many_attempts` (five failures per client address per ten
minutes).

## Endpoints

GET /health
GET /client-metadata.json
GET /login?handle=...
POST /v1/pair
POST /v1/login/app-password
POST /v1/revoke
GET /v1/profile?actor=...
GET /v1/follows?actor=...&cursor=...
GET /v1/followers?actor=...&cursor=...
GET /v1/author-feed?actor=...&cursor=...
POST /v1/follow
GET /v1/timeline?limit=20&cursor=...
GET /v1/notifications?limit=20&cursor=...
GET /v1/thread?uri=...
POST /v1/post
POST /v1/like
POST /v1/repost
POST /v1/notifications/seen

### Profile response

```json
{
  "did": "did:plc:...",
  "handle": "example.test",
  "displayName": "Example",
  "description": "A short biography.",
  "followersCount": 12,
  "followsCount": 34,
  "postsCount": 56
}
```

### Other people's profiles

`GET /v1/profile` with no `actor` is your own account. With `?actor=` (a handle
or a DID, never a URL) it is that account, and the response gains `following`
and `followedBy` (booleans, absent for your own profile) and `pinned`, the
pinned post in the timeline post shape if there is one. `404 actor_not_found`
for an account that doesn't exist; `400 invalid_actor` for something that is not
a handle or DID.

`GET /v1/follows` and `GET /v1/followers` take `actor`, `limit` and `cursor`
and return `{"actors":[{"did","handle","displayName"}],"cursor":"..."}`.
`GET /v1/author-feed` takes the same and returns the timeline response for that
account's own posts, replies left out.

`POST /v1/follow` with `{"did":"did:plc:...","on":true}` sets the follow state
idempotently: the bridge reads the current state first, so the Mac never holds
a follow record URI. Returns `{"did","on"}`.

### Timeline response

Timeline responses contain only fields that the Classic Mac client needs:

```json
{
  "posts": [
    {
      "uri": "at://did:plc:.../app.bsky.feed.post/...",
      "cid": "bafy...",
      "author": {
        "did": "did:plc:...",
        "handle": "example.test",
        "displayName": "Example"
      },
      "text": "Hello from Platinum.",
      "createdAt": "2026-10-05T00:00:00.000Z",
      "likeCount": 4,
      "repostCount": 2,
      "replyCount": 1,
      "quoteCount": 0,
      "liked": false,
      "reposted": true
    }
  ],
  "cursor": "optional-next-cursor"
}
```

The bridge deliberately does not expose raw feed-view embeds, reply views or AppView-specific extension fields. Clients should treat absent optional author fields as empty. Text fields are bounded by Unicode code point count so the bridge cannot split a UTF-16 surrogate pair when limiting native-client payloads.

### Notifications response

```json
{
  "notifications": [
    {
      "uri": "at://did:plc:.../app.bsky.feed.post/...",
      "cid": "bafy...",
      "author": {
        "did": "did:plc:...",
        "handle": "example.test",
        "displayName": "Example"
      },
      "reason": "like",
      "indexedAt": "2026-10-05T00:01:00.000Z",
      "isRead": true
    }
  ],
  "cursor": "optional-next-cursor"
}
```

`liked` and `reposted` say whether the signed-in account has liked or reposted
the post. The like and repost record URIs stay on the bridge.

### Marking notifications seen

`POST /v1/notifications/seen` with `{"seenAt":"2026-10-05T00:01:00.000Z"}` marks
everything up to that time as read. Send the `indexedAt` of the newest
notification you displayed, exactly as the bridge sent it, not the Mac's clock:
notifications that arrive after the list was fetched then stay unread. A time in
the future is clamped to the bridge's now. Returns `{"seenAt": "..."}`; `400
invalid_seen_at` for anything that is not an ISO 8601 timestamp.

### Thread

`GET /v1/thread?uri=at://...` returns one flat, bounded list so the Mac never
walks a reply tree: ancestors first (oldest at the top, negative `depth`), the
post asked for at `depth` 0, then replies depth-first with positive `depth`.
Each entry is a timeline post plus `depth`. At most 40 posts, 6 levels of
replies and 10 ancestors; `truncated` is true when anything was left out.
Blocked and deleted posts are skipped. `400 invalid_post_ref` for a uri that is
not an `app.bsky.feed.post` AT URI; `404 post_not_found` when the post is gone.

```json
{ "posts": [ { "uri": "at://...", "depth": -1, "text": "..." } ], "truncated": false }
```

### Replying

`POST /v1/post` accepts an optional `"replyTo": {"uri": "...", "cid": "..."}`
naming the post being answered. The bridge reads that post's own record to find
the thread root, so the Mac never needs the root rule: a reply to a top-level
post uses it as both root and parent, and a reply to a reply keeps the original
root. Errors: `400 invalid_post_ref` for a malformed `replyTo`, `404
post_not_found` if the parent has gone.

### Like and repost

`POST /v1/like` and `POST /v1/repost` take `{"uri":"at://...","cid":"...","on":true}`
and set the state to `on`. They are idempotent: the bridge reads the current
state first, so sending `on` twice likes once, and undoing needs no record URI
from the client. The response is the state now and the count to display:

```json
{ "uri": "at://did:plc:.../app.bsky.feed.post/...", "on": true, "count": 5 }
```

The count is the AppView's count adjusted by this change, not a fresh read, so
it can lag other people's activity until the next refresh. Errors:
`400 invalid_post_ref` (the uri is not an `app.bsky.feed.post` AT URI, the cid
is not alphanumeric, or `on` is not a boolean) and `404 post_not_found` when
asked to like or repost a post that no longer exists.

### Post response

Post text is accepted as UTF-8. The native Classic Mac client converts its MacRoman TextEdit buffer to UTF-8 before submitting it and currently limits posts to 300 characters.

```json
{
  "uri": "at://did:plc:.../app.bsky.feed.post/...",
  "cid": "bafy..."
}
```

Authenticated endpoints use:

    Authorization: Bearer <bridge-token>

The protocol intentionally uses plain JSON. This keeps the classic Mac side
small and avoids requiring a complete AT Protocol implementation.

## Security

The bridge is trusted with the OAuth session. Platinum receives only a
revocable bridge token.

Do not expose an HTTP-only bridge to an untrusted network. For deployment,
put it behind HTTPS and an appropriate access boundary.

The initial implementation uses local file-backed state. Installation records contain a hashed token, account DID, creation time, last-used time, optional client metadata, and an optional revocation timestamp. The plaintext token is returned only when the installation is issued.

The protocol does not depend on that storage implementation, so a later database
or managed KV store can replace it.
