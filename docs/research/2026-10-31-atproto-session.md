# AT Protocol session and XRPC practice

Date: 2026-10-31. Sources checked this day.

## Sources

- https://atproto.com/specs/xrpc (search result; direct fetch timed out)
- https://atproto.com/specs/oauth (search result; direct fetch timed out)
- https://atproto.com/blog/oauth-improvements (2025-06-12)
- https://github.com/bluesky-social/atproto (lexicons and SDKs; raw file fetches timed out)
- https://atproto.com/blog/oauth-atproto (2024-09-25)

## Findings

- XRPC methods are `GET|POST https://<service>/xrpc/<nsid>` with query
  parameters or a JSON body. Authenticated calls use
  `Authorization: Bearer <accessJWT>`.
- The XRPC spec says access JWTs are short-lived and refreshed periodically;
  the exact lifetime is a server decision, not a constant a client may assume.
  The JWT `exp` claim is the only per-session value available without parsing
  server configuration.
- `com.atproto.server.createSession` and `refreshSession` are the legacy
  password-session path. They are not the OAuth token endpoint; an AT Protocol
  OAuth session refreshes through the authorization server's token endpoint
  with DPoP. This matters because a native OAuth implementation cannot reuse the
  refreshSession code.
- Error responses use an envelope with a machine-readable `error` and a human
  `message`; `ExpiredToken`, `InvalidToken` and `AuthRequired` mean refresh or
  sign-in, `InvalidRequest` means a client bug or bad input, and
  `RateLimitExceeded` means back off.
- Pagination is cursor-based; the cursor is opaque and must be passed back
  exactly, not reconstructed.
- OAuth requires PKCE and PAR, and DPoP; access tokens should be under 30
  minutes and refresh tokens time-limited (the OAuth spec's guidance was
  tightened over 2025). This remains phase 2.
- No single published numeric rate limit applies to every PDS/AppView. HTTP
  429 plus `Retry-After` is the interoperable signal. The client must not
  hammer a 429 and must not retry non-idempotent writes automatically.

## Not verified

- Exact access/refresh lifetimes for app-password sessions on current
  bsky.social. I did not have a test account, and atproto.com timed out. The
  implementation therefore treats `exp` as a hint and a 401 as authoritative.
- Current server behaviour for refresh-token reuse/rotation on the legacy
  session path. No live test.
- Whether every PDS returns `ExpiredToken` rather than a bare 401. The client
  handles both.

## ADR impact

- ADR 0001 stands: XRPC over the existing transport is the right shape.
- ADR 0004 is superseded: session persistence needs a real secure store, not a
  Preferences file.
- ADR 0005 is revised: the entropy premise changed (see the transport note),
  but OAuth/DPoP remains blocked.
