# Native client migration

Platinum is moving the AT Protocol work off the bridge and into the Mac
application. The goal is a client that talks to a PDS itself, with no Node
service, no companion process and no host-side helper at runtime. The window,
menus, input, storage and drawing stay as they are; only where the data comes
from changes.

This document is the map, not a claim of completion. The bridge is still the
runtime for most screens. The first native modules exist and are host-tested;
the endpoint table says exactly which ones are not wired yet.

## Layers

```
Platinum windows and view models
        |
AT Protocol client (C89, host-tested)          atproto_url, atproto_identity,
        |                                      atproto_session, atproto_client
Wolfram XRPC/HTTP API                          wf_xrpc_client_* in
        |                                      wolfram-macos9-transport
Open Transport + macTLS (verified TLS)         external, never bypassed
        |
PDS / AppView
```

The layers stay separate:

| Layer | Where | What it owns | What it must not do |
|---|---|---|---|
| Net | Wolfram transport + macTLS | TCP, TLS, certificate verification, HTTP/1.1, timeouts, yield | Know about JSON or AT Protocol records |
| AT core | `macos9/atproto_*.c` | URLs, identity, sessions, XRPC request and response mapping | Include Toolbox headers or block the event loop |
| UI | `macos9/*.c` windows and view models | Drawing, input, bounded display models | Build protocol URLs or hold credentials |

## Endpoint mapping

Every route in `docs/BRIDGE_PROTOCOL.md` maps to one XRPC method. The owner is
the module that will issue the call or consume its answer. "Parsing done" means
the named module and test exist in this change; "pending" means the route still
goes through the bridge.

| Bridge route or function | Native replacement | Owner (planned if pending) | Test | State |
|---|---|---|---|---|
| `POST /v1/pair` (browser OAuth) | none: OAuth is phase 2, blocked on entropy | — | — | removed with the bridge; see ADR 0005 |
| `POST /v1/login/app-password` | `com.atproto.server.createSession` | `atproto_client.c` | `test_atproto_client` | request/response, refresh-retry, bootstrap-then-sign-in and the handle-only sign-in window are done; the bridge route is no longer called by the UI |
| `POST /v1/revoke` | `com.atproto.server.deleteSession` | `session.c` | `test_session` (to add) | pending |
| `GET /health` | `com.atproto.server.describeServer` | `diag.c` | `test_diag` (to rewrite) | pending |
| `GET /v1/profile` | `app.bsky.actor.getProfile` | `profile_feed.c` | `test_profile` (to rewrite) | pending |
| `GET /v1/follows` | `app.bsky.graph.getFollows` | `people_feed.c` | `test_people` (to rewrite) | pending |
| `GET /v1/followers` | `app.bsky.graph.getFollowers` | `people_feed.c` | `test_people` | pending |
| `GET /v1/author-feed` | `app.bsky.feed.getAuthorFeed` | `profile_feed.c` | `test_profile` | pending |
| `POST /v1/follow` | `com.atproto.repo.createRecord` / `deleteRecord`, collection `app.bsky.graph.follow` | `profile_feed.c` | `test_profile` | pending |
| `POST /v1/mute` | `app.bsky.actor.mute` / `unmute` | `profile_feed.c` | `test_profile` | pending |
| `POST /v1/block` | `com.atproto.repo.createRecord` / `deleteRecord`, collection `app.bsky.graph.block` | `profile_feed.c` | `test_profile` | pending |
| \`GET /v1/timeline\` | \`app.bsky.feed.getTimeline\` | \`atproto_feed.c\` + \`timeline_native.c\` | \`test_timeline\` | native end to end: paging, 40-row cap, and failure handling; viewer interaction URIs are retained |
| \`POST /v1/like\` | \`com.atproto.repo.createRecord\` / \`deleteRecord\`, \`app.bsky.feed.like\` | \`atproto_feed.c\` + \`timeline_native.c\` | \`test_timeline\` | native end to end; creates and removes the exact like record from the AppView viewer URI |
| \`POST /v1/repost\` | \`com.atproto.repo.createRecord\` / \`deleteRecord\`, \`app.bsky.feed.repost\` | \`atproto_feed.c\` + \`timeline_native.c\` | \`test_timeline\` | native end to end; creates and removes the exact repost record from the AppView viewer URI |
| `GET /v1/notifications` | `app.bsky.notification.listNotifications` | `notifications_feed.c` | `test_notifications` | native: the window fetches and pages through the native session, with the same 20-row pages, 40-row cap and seen mark |
| `POST /v1/notifications/seen` | `app.bsky.notification.updateSeen` | `notifications_feed.c` | `test_notifications` | native: the newest loaded item's `indexedAt`, never the Mac clock |
| `GET /v1/thread` | `app.bsky.feed.getPostThread`, flattened in C with depth and size caps | `thread_feed.c` | `test_thread` (to rewrite) | pending |
| `GET /v1/search/actors` | `app.bsky.actor.searchActors` | `application_views.c` / SQL | `test_search` | pending |
| `GET /v1/search/posts` | `app.bsky.feed.searchPosts` | `application_views.c` | `test_search` | pending |
| `GET /v1/feeds` | `app.bsky.actor.getPreferences` (saved feeds) | `application_views.c` | `test_prefs` (to extend) | pending |
| `GET /v1/feed` | `app.bsky.feed.getFeed` | `timeline.c` | `test_timeline` | pending |
| `GET /v1/lists` | `app.bsky.graph.getLists` | `application_views.c` | `test_people` | pending |
| `GET /v1/list` | `app.bsky.graph.getList` | `people_feed.c` | `test_people` | pending |
| `GET /v1/post/likes` | `app.bsky.feed.getLikes` | `people_feed.c` | `test_people` | pending |
| `GET /v1/post/reposts` | `app.bsky.feed.getRepostedBy` | `people_feed.c` | `test_people` | pending |
| `POST /v1/post` | `com.atproto.repo.createRecord` (`app.bsky.feed.post`), threadgate record for reply gates | `application_post.c` + a post mapper | `test_compose` (to extend) | pending |
| `POST /v1/post/delete` | `com.atproto.repo.deleteRecord` | `application_post.c` | `test_compose` | pending |
| `GET /v1/muted-words` | `app.bsky.actor.getPreferences` | `muted_words.c` (matcher) + the screen | `test_muted_words` | matcher done and host-tested; fetching and the screen wiring are pending |
| `POST /v1/muted-words` | `app.bsky.actor.putPreferences` | same | `test_muted_words` (to extend) | pending |
| `GET /v1/conversations` | `chat.bsky.convo.listConvos` via `atproto-proxy: did:web:api.bsky.chat#bsky_chat` | `conversations_feed.c` | `test_conversations` (to rewrite) | pending |
| `GET /v1/conversation` | `chat.bsky.convo.getMessages` | `conversations_feed.c` | `test_conversations` | pending |
| `POST /v1/conversation/message` | `chat.bsky.convo.sendMessage` | `conversations_feed.c` | `test_conversations` | pending |
| `GET /v1/image` (PLIM rendering) | none: fetch the CDN image and decode it on the Mac | `image_blob.c` / a decoder | `test_image_blob` | pending; Wolfram's decoder exists but is not C89-clean, so the bridge route stays (ADR 0011) |
| `GET /health`, `/client-metadata.json`, `/login` | none: diagnostics use the session, OAuth metadata goes with phase 2 | `diag.c` | `test_diag` | pending |

Bridge functions with no endpoint:

| Function | Native replacement | State |
|---|---|---|
| OAuth session store and hosted-node client | Full AT Protocol OAuth with PKCE, PAR and DPoP, on the Mac | blocked by ADR 0005 |
| Bridge token issue, hash, revoke, installation registry | none: there is no bridge credential once the Mac holds its own refresh token | removed with the bridge |
| App-password session file | Mac Preferences: refresh token only, never the password (ADR 0004) | design decided, storage pending |
| Update manifest/signature/installer | native updater, issue #48 | pending, unchanged by this migration |
| Post display shaping (record, embeds, facets) | parse the AppView post view directly into the existing preview structs | pending |
| Muted-word filter | native matcher over `getPreferences`; same rules as the SDK | pending |
| Image convert / PLIM | no replacement: native decode and draw | pending |

## Screens

The endpoint table says what each route maps to. This says what the windows do
today, and what a reader sees when a session is missing. `screen_state.c`
(host-tested) is the single decision; no window guesses.

| Screen | Data source | State |
|---|---|---|
| Sign-in | `atproto_bootstrap.c` + `createSession` | native |
| Home timeline | `atproto_feed.c` + `timeline_native.c` | native end to end, including likes and reposts |
| Notifications | `atproto_feed.c` + `notifications_feed.c` | native |
| Profile | bridge | pending |
| Thread | bridge | pending |
| Search (accounts and posts) | bridge | pending |
| Saved feeds and lists | bridge | pending |
| Compose, reply, quote | bridge | pending |
| Conversations | bridge | pending |
| Connection Status | bridge | pending |
| Muted words | bridge | pending |

A screen that still needs the bridge says so in its own status line: "This
screen still uses the bridge. Pair one under File > Pair Account." With no
session at all it says "Sign in first: choose File > Sign In with App
Password." Both sentences come from `screen_state_text`, so they are tested
rather than repeated in every window, and the bridge is never silently assumed.

## Identity resolution

```
handle --https://<handle>/.well-known/atproto-did--> did
       --resolveHandle on a known service, if the well-known is unavailable--> did
did:plc --https://plc.directory/did:plc:...--> DID document
did:web --https://<host>[/path]/did.json--> DID document
DID document --service#atproto_pds--> PDS endpoint
```

`atproto_bootstrap.c` follows that sequence with a caller-supplied fetch
callback, so the host tests drive the exact request order against recorded
fixtures and the Mac build passes Wolfram's `wf_http_get_limited`. The DID
document must carry the resolved `id` and must name `at://<handle>` in
`alsoKnownAs`; a document that merely names a PDS for another account is
refused (ADR 0008).

`did:plc` is the normal case. `did:web` is implemented with the method's port
and path rules. Other DID methods are refused rather than guessed. DNS TXT
lookup is deliberately not used (ADR 0003).

## Build toolchain

- **Mac target:** Retro68 (`gcc` for 68k and PowerPC), CMake 3.16+, strict C89
  (`-std=c90`, no extensions), `tools/build-macos9.sh [68k|ppc|both]`. The
  Retro68 image is `ghcr.io/autc04/retro68`.
- **Headers:** Wolfram's public headers, pinned by CI at `v0.36.2` (the current
  Wolfram release is v0.44.2; the pin is unchanged in this migration).
- **Transport:** Wolfram's `wolfram-macos9-transport` static library
  (`xrpc_mac9.c`, `macos9_tls.c`) over Open Transport and macTLS. Native
  networking is PowerPC-only (ADR 0010): the 68k build still compiles and
  links, but it is not a supported network client. CI cannot build Open
  Transport, so it links the test transport stub instead. That binary compiles
  and links; it cannot network and is never shipped.
- **Host checks:** every `macos9/*.c` and `macos9/test/*.c` is compiled by clang
  and gcc with `-pedantic-errors -Wall -Wextra -Werror`, then the host test
  binaries run. This is a dialect and logic check, not Mac OS 9 validation.
- **Runtime:** `tools/macos9-qemu/harness.py` can boot an owner-supplied Mac OS
  9.2.2 image under QEMU with QMP input and screenshots. It is blocked here
  because no image or ROM is present; nothing is downloaded.

## Phases

1. **Core (done).** https-only service bases, XRPC URLs, DID/handle
   resolution, createSession/refreshSession parsing, expiry hint, error
   mapping, token wiping. Host-tested. Bridge untouched.
2. **Client (done).** The transport-facing XRPC call layer, authenticated
   GET/POST, createSession, one refresh-and-retry, the handle-to-PDS bootstrap
   and native sign-in are host-tested. The sign-in window now asks for a handle
   and an app password and runs that sequence; Preferences says which handle
   method found the account. The home timeline fetches and pages through
   `timeline_native.c`. The bridge is no longer needed for either, but it is
   still configured and used by the screens below.
3. **Screens.** Notifications, profile, thread, search, feeds, lists, compose
   and conversations still call the bridge, each with its existing test
   rewritten against recorded AppView fixtures.
4. **Images and moderation.** Native image decode/draw and a native muted-word
   matcher. These are the two largest non-XRPC gaps.
5. **OAuth.** Only after ADR 0005 is revised with an entropy decision. No DPoP
   key generation before that.
6. **Bridge removal.** Delete `bridge/` and `native/bridge/` in their own
   commit once parity tests pass. Git history is the archive; no history is
   rewritten.

## Security requirements

- Every service base is `https://`; there is no plain-http path and no
  "continue anyway" option. macTLS verifies the chain, hostname and validity
  dates; a failure is a failure.
- The app password is used for one request and wiped. Only the refresh token is
  persisted; the access token is memory-only.
- Tokens, passwords and refresh material are never logged, including at
  diagnostics verbosity. Error mapping never copies the upstream message.
- Credentials for tests come from environment variables or a gitignored file,
  never from a fixture. Fixtures contain synthetic tokens and public documents.
- DPoP and OAuth are blocked until the entropy ADR is resolved.

## Open questions

1. **Images.** The bridge rendered PLIM on the desktop. With no bridge, the Mac
   must fetch image bytes over TLS and decode JPEG/PNG itself. Wolfram has the
   decoder (`wf_image_decode_rgba`, used by Indigo), but it pulls the agent and
   CBOR headers, which are not C89-clean, and `stb_image.h` uses `//` comments
   that `-Werror` rejects. ADR 0011 keeps the bridge image route and asks
   upstream for a standalone C89 image module rather than relaxing the Mac
   target's dialect.
2. **Muted words.** The AppView does not filter for the client. `muted_words.c`
   now implements the matching rules in C89 (whole words versus substrings,
   content and tag targets, "exclude following", expiry at a caller-supplied
   clock), with 39 host checks. Fetching `mutedWordsPref`, storing the list and
   filtering the screens are still on the bridge; the matcher is not wired yet.
3. **Thread flattening.** `getPostThread` returns a tree; the bounded flat list
   the Thread window expects has to be produced in C with the same 40-post,
   6-level and 10-ancestor limits.
4. **Bootstrap service.** The HTTPS well-known document gives a DID without a
   preconfigured PDS, and the DID document gives the PDS. The sequence is
   implemented and host-tested (`atproto_bootstrap.c`,
   `platinum_atproto_client_connect`), and the sign-in window runs it from a
   handle; the handle method used is reported through
   `platinum_atproto_entryway` and shown in Preferences.
5. **Transport.** macTLS now documents TLS 1.3 and a hardware-validated
   entropy pool, but its current implementation is PowerPC-only; the 68k build
   links the test stub and cannot network. ADR 0010 decides that native
   networking is PowerPC-only and the 68k build stays a compile target. Verify
   the cipher and certificate path against a current PDS in the guest (ADR
   0001).
6. **Entropy.** TLS handshakes on Classic Mac OS have a weak base entropy
   source; long-lived DPoP keys are not safe to generate. ADR 0005 is a release
   blocker for OAuth, not for phase 1.
7. **Keychain at rest.** Mac OS 9 does have the Carbon Keychain Manager (ADR
   0007), but the pinned Retro68 image ships no `Keychain.h` and no
   KeychainLib, so the implementation cannot compile here (verified 2026-10-31;
   see `docs/research/2026-10-31-keychain-surface.md`). Until a tested shim
   exists, the session is memory-only and the user signs in again at launch;
   there is no plaintext fallback.
8. **Live integration tests.** No test account or PDS credentials are available
   here. The suite is opt-in by design and has not run against a live PDS.
9. **Wolfram pin.** CI compiles against v0.36.2 while the newest release is
   v0.44.2. Moving the pin is a separate, deliberate change.
10. **Bridge deletion criteria.** Parity is per screen, not per endpoint. The
    bridge stays until every screen in `docs/PARITY.md` has native tests and no
    bridge-only route is reachable from the UI.
