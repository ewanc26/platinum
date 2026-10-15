# Native bridge plan

The Node bridge exists because the Mac cannot do modern TLS, DPoP OAuth or DID
resolution. The plan is a native C bridge that does the same jobs, built on
Wolfram, so Platinum no longer needs Node. This file lists the parts, which
ones can start now, and what is blocked.

## Parts

Status as of the last update to this file. "Done" means implemented, tested in
`native/bridge` (`make test`), and for the Wolfram-backed parts also run against
Wolfram's library and a local server (`make live`).

| # | Part | Status | Depends on |
|---|------|--------|------------|
| 1 | PLIM encoder (RGBA8 to the bridge's byte-exact PLIM) | done | bridge/test/vectors/image/convert.json |
| 2 | Config and health (`GET /health`, env, version) | done | nothing |
| 3 | Token store: CSPRNG, SHA-256 at rest, bounded lookup | done | #116 (storage design) |
| 4 | Atomic file storage for every stored file | done | #117 |
| 5 | Mac pairing code and `/v1/pair` (the code table) | done; the route needs Part 7 | Part 3 |
| 6 | App-password policy, service-URL rules, failure limiter | done | Part 4 |
| 7 | OAuth glue | deferred by decision on #75 | #75 |
| 8 | Domain routes | route families done, see below | Wolfram typed headers |
| 9 | Image route `GET /v1/image` | done, decision on #118 | nothing further |
| 10 | HTTP server with the chosen transport | first slice done: private bind only, `/health`, live `/v1/profile` and `POST /v1/post`, bearer check in server.ts order; every other route answers 501 | the transport decision below |

Part 8, route families. Done as pure handlers with tests: routing and the bearer rule,
profile, the write routes (like, repost, follow, mute, block), notifications seen,
muted words (set and list), post delete, search, feed, list, actor graph, post
engagement, thread, and post creation (publish live, including every reply gate and
replies). Live against Wolfram: profile fetch and post publishing. Not started: the
notifications list and the other read routes that only need their typed Wolfram calls
wired (timeline, notifications, conversations, messages, feeds, lists, author feed,
image source fetch over the CDN).

## Decisions

These were made by the project owner's delegation, recorded here with the reason, and
can be revisited.

- **#119, transport.** The bridge listens only on a private address (the default is
  127.0.0.1, and a LAN address is allowed). It never listens on a public address. The
  Mac link is plain HTTP over the LAN, with bearer tokens. The Mac cannot speak TLS and
  a TLS terminator in front of the bridge would still carry the same LAN hop, so
  TLS cannot be added on the Mac's side. For access from outside the LAN, the operator
  puts the bridge behind a VPN; the bridge does not ship a tunnel. Recorded in
  `docs/BRIDGE_PROTOCOL.md`.
- **#75, upstream OAuth.** Not migrated. Wolfram's OAuth node keeps sessions only in
  memory, so moving the bridge to it would break installations on every restart. The
  Node bridge stays the OAuth owner. The native bridge signs in with an app password
  only (Part 6, operator opt-in), until Wolfram persists node sessions. Part 7 is
  deferred until then.
- **#118, image decode.** Flatten first, as the JavaScript does. The PLIM encoder
  flattens onto white and shrinks, and it matches the vectors byte for byte. The
  native path decodes the full image with Wolfram and never uses Wolfram's shrink,
  which averages premultiplied alpha. Wolfram's decoder refuses sources over 2048
  pixels a side, so the native path refuses them with 413. The JavaScript allows up to
  12 million pixels. This stricter limit is recorded in `docs/BRIDGE_PROTOCOL.md`. The
  Node bridge keeps its own limit.
- **Reply gates and the threadgate rkey.** Wolfram's `wf_agent_create_threadgate`
  writes the threadgate with a new rkey, which the AppView does not link to the post.
  The native bridge writes it with `putRecord` and the post's rkey, as the TypeScript
  does. Wolfram is not changed here; the gap is filed as ewanc26/wolfram#203.

Part 10 remaining: wire the other route families through the server (each needs its
live Wolfram call), sign-in (Part 7 is deferred, so pairing needs the app-password
sign-in route), and token persistence (token records must survive restart, so the
server needs Part 4 storage behind the token lookup). The server keeps tokens only as
the caller provides them.

## Blocked

- **Indigo 3DS device build.** The devkitPro hosts are blocked by the environment's
  network policy. It needs `apt.devkitpro.org` and `pkg.devkitpro.org` allowed in the
  environment's network settings. That is an environment setting, not a code decision.

## Rules for the parts

- Each part has byte-exact or behaviour-exact tests against the existing
  vectors and the TypeScript tests, so the two bridges can be compared.
- C89 for anything the Mac build checks; Wolfram's C23 is used only in the
  bridge itself.
- No part adds a second AT Protocol stack. Protocol work goes to Wolfram.
