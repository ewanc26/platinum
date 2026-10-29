# Native bridge plan

The Node bridge exists because the Mac cannot do modern TLS, DPoP OAuth or DID
resolution. The plan is a native C bridge that does the same jobs, built on
Wolfram, so Platinum no longer needs Node. This file lists the parts, which
ones can start now, and what is blocked.

The direction is that the whole bridge ends up native: every route in
`bridge/src/server.ts` must be served by `native/bridge`, and no feature is
added to the Node bridge alone. The native binary can now sign in and hold a
session, so Node is the runnable reference, not the destination.

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
| 5 | Mac pairing code and `/v1/pair` (the code table) | done and served | Part 3 |
| 6 | App-password policy, service-URL rules, failure limiter | done and served | Part 4 |
| 7 | OAuth glue | done through the hosted Wolfram OAuth node; node sessions are still memory-only (#75) | #75 |
| 8 | Domain routes | route families done, and the home timeline and notifications are live, see below | Wolfram typed headers |
| 9 | Image route `GET /v1/image` | done, decision on #118 | nothing further |
| 10 | HTTP server with the chosen transport | dispatch and sign-in complete: every route family is wired with full input validation, and the native `main` runs the server with the token store, session files and image CDN | the transport decision below |

Part 8, route families. Done as pure handlers with tests: routing and the bearer rule,
profile, the write routes (like, repost, follow, mute, block), notifications seen,
muted words (set and list), post delete, search, feed, list, actor graph, post
engagement, thread, and post creation (publish live, including every reply gate and
replies). Live against Wolfram: profile fetch, post publishing, the image source fetch
over the CDN, the home timeline, the notification list and seen mark, the conversation
list, one conversation's messages, sending a message, actor search, the actor graph
(follows and followers), post likes and reposts, the author feed, and the saved feed and
list names, and the whole thread around a post, and the muted-word list and its
edits. Not started: post search, whose timeline post mapping is not yet shared with
the search route.

The timeline route keeps server.ts's decisions: the limit is 1 to 50 with 20 the
default, the cursor is passed through, and muted words are applied before the page is
built, so a page can come back short while its cursor still moves on. One native
bound is added: a cursor over 255 bytes is refused with `400 invalid_cursor` rather
than truncated, because the page model holds it in a fixed field and a cut cursor
would page nowhere. Fetching the timeline reads the account's preferences once per
page for the muted words; a preferences failure shows the page unfiltered. One mute
rule is not honoured: a word's "exclude following" target, because the feed item's
author view does not expose the viewer's following state through Wolfram, so such a
word hides matching posts from followed accounts too.

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
- **#75, upstream OAuth.** The native bridge uses the shared hosted Wolfram OAuth
  node at `https://oauth.croft.click`, the same node as Cobalt and Indigo, so the Mac
  never receives a PDS password, OAuth refresh token or DPoP key. The node keeps
  OAuth sessions in memory, so restarting it invalidates installations that signed
  in through it until they pair again; the bridge keeps each node bearer in its
  session file, but cannot refresh a session the node has forgotten. Persisting node
  sessions in Wolfram remains the fix. App-password sign-in stays available as the
  operator opt-in from Part 6.
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

Part 10 remaining: share the timeline post mapping with post search and replace its
stub callback once that is in place. Muted words are read and written through the
same preferences call the timeline filter uses, so the list a user edits is the list
that filters their timeline; a write leaves the account's other preferences alone. The dispatch wiring, input validation, sign-in
routes (hosted-node pairing, app-password login and revoke) and token persistence are
complete. `native/bridge/main.c` links the server to the token store, session files,
the OAuth node client and the image CDN client.

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
