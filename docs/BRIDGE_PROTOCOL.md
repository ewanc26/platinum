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

## Endpoints

GET /health
GET /client-metadata.json
GET /login?handle=...
POST /v1/pair
POST /v1/revoke
GET /v1/profile
GET /v1/timeline?limit=20&cursor=...
GET /v1/notifications?limit=20&cursor=...
POST /v1/post

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
      "quoteCount": 0
    }
  ],
  "cursor": "optional-next-cursor"
}
```

The bridge deliberately does not expose raw feed-view embeds, reply views or AppView-specific extension fields. Clients should treat absent optional author fields as empty.

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

### Post response

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
