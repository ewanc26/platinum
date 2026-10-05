# AGENTS.md — Platinum

Platinum is a native Bluesky / AT Protocol client for Classic Mac OS 9. It is deliberately split into a small native client and a modern bridge service.

This file is for AI coding agents and contributors working in the repository. Read it before making changes. If the code and this document disagree, the code is authoritative; update this document when the architecture changes.

---

## 1. Project intent

Platinum exists to make Bluesky usable from Classic Mac OS 9 hardware without pretending that a 1990s operating system can comfortably host the modern AT Protocol stack.

The project has two deliberately separate components:

1. **Platinum** — the native Mac OS 9 application.
2. **Platinum Bridge** — a modern service that handles OAuth, modern HTTPS and the AT Protocol itself.

The Mac client should feel like a real Classic Mac application, not a web page rendered inside an old browser.

The bridge should remain independently useful as the modern protocol boundary for the client.

Do not collapse these layers merely to make a feature appear faster.

---

## 2. Architecture

The intended data flow is:

```
Classic Mac OS 9
       |
       | HTTP + JSON
       v
Platinum Bridge
       |
       | AT Protocol
       v
PDS / AppView
```

### Mac OS 9 side

The native client owns:

- the application UI;
- input and event handling;
- bridge configuration;
- pairing-code entry;
- storage of the bridge token;
- the paired account DID and installation ID;
- presentation of profiles, timelines and notifications;
- composition of posts;
- HTTP/JSON communication with the bridge.

### Bridge side

The bridge owns:

- AT Protocol OAuth;
- OAuth state and refresh credentials;
- DPoP;
- DID resolution and identity handling;
- modern HTTPS/TLS;
- PDS/AppView requests;
- bridge-token issuance and revocation;
- translation between the native client protocol and modern AT Protocol APIs.

### Security boundary

The Mac client must never need:

- the user's normal Bluesky password;
- an OAuth refresh token;
- an OAuth private key;
- a DPoP private key;
- modern AT Protocol OAuth metadata;
- a PDS session refresh credential.

The Mac client receives only a revocable bridge token after browser-assisted pairing.

If a new feature requires one of the above on Mac OS 9, stop and reconsider the architecture before implementing it.

---

## 3. Repository structure

The important current paths are:

```
platinum/
├── AGENTS.md
├── README.md
├── docs/
│   └── BRIDGE_PROTOCOL.md
├── bridge/
│   ├── src/
│   ├── package.json
│   ├── tsconfig.json
│   └── .env.example
├── macos9/
│   ├── application.c
│   ├── application.h
│   ├── bridge_client.c
│   ├── bridge_client.h
│   ├── compose.c
│   ├── compose.h
│   ├── config.c
│   ├── notifications.c
│   ├── notifications.h
│   ├── preferences.c
│   ├── preferences.h
│   ├── profile.c
│   ├── profile.h
│   ├── config.h
│   ├── main.c
│   ├── session.c
│   ├── session.h
│   ├── text_codec.c
│   ├── text_codec.h
│   ├── timeline.c
│   ├── timeline.h
│   ├── ui.c
│   └── ui.h
└── .github/
    └── workflows/
        └── ci.yml
```

The repository will acquire additional Mac OS 9 application modules as the native UI is implemented. Keep platform-specific code under the Mac-side tree and modern bridge code under `bridge/`.

Do not move shared protocol concepts into a modern abstraction that forces the Mac client to depend on desktop runtime facilities.

---

## 4. Classic Mac OS 9 constraints

Treat Mac OS 9 as the actual target.

### Language

Prefer strict C89 / CodeWarrior-era C.

Do not casually introduce:

- C99 declarations in the middle of blocks;
- `//` comments if the target compiler does not support them;
- `snprintf` or other unavailable libc conveniences;
- POSIX-specific APIs;
- compiler extensions without a concrete target requirement.

Use repository compatibility helpers where one is needed.

### Networking

Use Wolfram's Mac OS 9 transport.

The intended stack is:

```
Platinum
   |
bridge_client
   |
Wolfram XRPC/HTTP API
   |
Open Transport
   |
macTLS
   |
network
```

Do not add:

- libcurl;
- OpenSSL;
- pthreads;
- blocking BSD sockets;
- modern desktop HTTP libraries;
- another TLS implementation.

Wolfram's Mac OS 9 transport is built around Open Transport and macTLS. It provides the cooperative transport boundary required by Classic Mac OS.

### Event loop

Classic Mac OS networking must remain cooperative.

Long network operations must yield to the host application's event loop through Wolfram's Mac OS 9 yield callback rather than blocking the UI indefinitely.

Do not hide a blocking network loop inside rendering or input handling.

### Memory

Assume memory is constrained.

Prefer bounded response sizes, explicit ownership, small structs, short-lived request buffers and incremental processing where practical.

Do not retain complete feeds, images or HTTP responses indefinitely without a clear ownership and size policy.

---

## 5. Wolfram dependency

Wolfram is the AT Protocol transport boundary used by Platinum.

The Mac OS 9 transport is provided by the `0.26.0` release line and its Mac OS 9/Open Transport support.

When changing the Wolfram integration:

1. inspect Wolfram's public headers first;
2. use the public API rather than private implementation details;
3. keep Platinum compatible with the released Wolfram API;
4. update the dependency documentation when the minimum version changes.

Do not add a Platinum-specific fork of Wolfram's transport unless there is a concrete reason the shared implementation cannot support the requirement.

If a Wolfram API is missing, fix Wolfram or store the small amount of client-specific state locally rather than inventing an undocumented API.

---

## 6. Platinum Bridge

The bridge lives under `bridge/` and is a modern Node.js service.

Current package:

- Node.js >= 22;
- TypeScript;
- native Node HTTP server;
- `@atproto/api`;
- `@atproto/oauth-client-node`.

The bridge intentionally has no web framework. Keep the dependency surface small unless there is a concrete operational or security benefit to adding one.

### Environment

The bridge reads:

- `PLATINUM_BRIDGE_URL`;
- `PLATINUM_BRIDGE_PUBLIC_URL`;
- `PLATINUM_BRIDGE_HOST`;
- `PLATINUM_BRIDGE_PORT`;
- `PLATINUM_BRIDGE_DATA_DIR`.

Production OAuth requires `PLATINUM_BRIDGE_PUBLIC_URL` to resolve to the externally reachable bridge origin.

Never commit `.env` or runtime state.

### Persistent bridge state

The current implementation uses file-backed JSON state under `.platinum-bridge/` for:

- OAuth state;
- OAuth sessions;
- bridge tokens.

This is an implementation detail, not part of the wire protocol. A future database/KV implementation must preserve the same security semantics.

### Pairing

Pairing is browser-assisted:

1. the user opens `/login?handle=...`;
2. the browser completes OAuth;
3. the bridge displays a six-character pairing code;
4. Platinum submits that code to `POST /v1/pair`;
5. the bridge returns protocol version 1, a token and DID.

Pairing codes are short-lived, single-use and must never be logged.

The bridge token is long-lived until explicitly revoked.

### API contract

The current protocol is documented in [docs/BRIDGE_PROTOCOL.md](docs/BRIDGE_PROTOCOL.md).

Current endpoints include:

- `GET /health`;
- `GET /client-metadata.json`;
- `GET /login?handle=...`;
- `GET /atproto-oauth-callback`;
- `POST /v1/pair`;
- `POST /v1/revoke`;
- `GET /v1/profile`;
- `GET /v1/timeline`;
- `GET /v1/notifications`;
- `POST /v1/post`.

Authenticated API requests use:

```
Authorization: Bearer <bridge-token>
```

Any endpoint or response-shape change must update `docs/BRIDGE_PROTOCOL.md` and the relevant client code in the same change.

---

## 7. Protocol design

The bridge protocol should remain boring.

Prefer:

- HTTP;
- JSON;
- small request/response objects;
- explicit protocol versioning;
- stable error codes;
- bounded responses;
- simple cursor-based pagination.

Avoid exposing raw modern AT Protocol implementation details to the Mac client unless the client genuinely needs them.

If the bridge can translate a modern protocol change into the existing Platinum API, do that instead of forcing a Mac client update.

When adding a capability, consider whether it belongs as a new bridge endpoint or as an extension of an existing response.

Do not make the Mac client understand OAuth, DPoP or PDS-specific authentication flows.

---

## 8. Authentication and secrets

Never commit:

- account passwords;
- app passwords;
- OAuth refresh tokens;
- OAuth access tokens;
- DPoP keys;
- bridge tokens;
- bridge session files;
- `.env` files;
- private test credentials.

Never log authenticated request headers or token values.

The browser is the authentication UI for OAuth. The Mac client should receive only the pairing result.

Token revocation must invalidate the bridge token server-side. Deleting the token locally is not equivalent to revocation.

---

## 9. Native application design

The native application is not yet complete. As screens are added, keep application state separate from rendering and transport.

A useful boundary is:

```
UI / event loop
       |
application state
       |
bridge client
       |
Wolfram transport
```

Do not let rendering code make arbitrary HTTP requests.

Do not let the bridge client own UI state.

Network operations should be represented as explicit application operations/state transitions so that the event loop can remain responsive.

### Planned core surfaces

The first complete native client should provide:

- first-run bridge configuration;
- pairing;
- profile/account information;
- home timeline;
- notifications;
- post composition;
- replies;
- basic profile navigation;
- sign-out/revocation.

Do not implement every modern Bluesky feature before the basic Classic Mac experience is reliable.

---

## 10. Classic Mac UI direction

Platinum should look like a Classic Mac OS application.

Prefer:

- standard Macintosh controls where appropriate;
- clear menu-bar and window conventions;
- readable bitmap-friendly typography;
- visible focus and selection;
- predictable keyboard/mouse navigation;
- compact layouts suitable for 640×480-era displays;
- explicit progress and error states for network operations.

Do not copy a modern Bluesky web interface literally.

Do not introduce a web rendering engine.

The interface should respect Classic Mac interaction patterns even when the underlying service is modern.

---

## 11. Accessibility and robustness

Classic Mac OS 9 does not provide the accessibility stack available on current platforms, so the application must make its own controls understandable.

Design for:

- keyboard navigation;
- clear focus;
- readable text;
- sufficient contrast;
- no colour-only status indicators;
- useful error messages;
- predictable dialogs;
- graceful handling of unavailable network services.

Do not make network availability a prerequisite for launching the application.

Malformed bridge responses must fail safely rather than corrupting application state.

---

## 12. Error handling

Use Wolfram's existing `wf_status` values at the transport boundary.

Do not invent a second incompatible error system for every Mac-side operation.

Translate errors at the UI boundary into useful user-facing messages.

The bridge should return stable JSON error objects and HTTP status codes. Do not expose stack traces, OAuth credentials or internal filesystem paths to the Mac client.

Treat malformed JSON, missing fields, unsupported protocol versions, expired pairing codes, revoked tokens, oversized responses, network failures and HTTP failures as normal error paths.

---

## 13. Build and validation

### Bridge

From `bridge/`:

```sh
npm install
npm run check
npm run build
npm test
```

The current CI runs type checking and the production build on Node 22.

Do not claim tests passed if they were not actually run.

### Mac OS 9 transport

Build the Wolfram transport with:

```sh
cmake -S . -B build-macos9-transport \
  -DWOLFRAM_BUILD_MACOS9_TRANSPORT=ON \
  -DWOLFRAM_MACTLS_ROOT=/path/to/macTLS
cmake --build build-macos9-transport --target wolfram-macos9-transport
```

The native Platinum application will require its intended CodeWarrior-era build environment as its application shell develops.

A successful host build is not proof of Classic Mac OS 9 compatibility.

### Hardware validation

When real Mac OS 9 hardware is available, validate:

- application launch;
- window/menu interaction;
- bridge configuration;
- pairing;
- TLS connection;
- timeline loading;
- pagination;
- posting;
- notification loading;
- sign-out/revocation;
- recovery from network failure;
- recovery from a bridge restart.

Do not describe emulator or modern-host testing as hardware validation.

---

## 14. Documentation

Keep these documents aligned:

- `README.md` — user-facing project overview and setup;
- `AGENTS.md` — engineering and agent guidance;
- `docs/BRIDGE_PROTOCOL.md` — wire protocol and security contract;
- `docs/UI_DESIGN.md` — native Classic Mac UI specification.

When changing an endpoint, request/response shape, pairing, authentication, minimum Wolfram version, Mac OS 9 build requirement or persistent state, update the relevant documentation in the same change.

Do not put generated citations, tool output or internal reasoning into repository Markdown.

---

## 15. Git workflow

Use feature branches and pull requests.

Keep commits atomic and use conventional commit messages, for example:

- `feat: add timeline screen`;
- `fix(macos9): handle truncated bridge responses`;
- `docs: document bridge pairing`;
- `refactor(bridge): isolate token store`.

Do not combine unrelated cleanup with a feature change.

If a change crosses the Platinum/Wolfram boundary, keep the changes independently reviewable where practical.

Before opening or updating a pull request:

1. inspect the relevant diff;
2. run the applicable checks;
3. verify version/dependency changes;
4. update documentation;
5. check for accidental secrets or generated files.

Never claim a check was run when it was not.

---

## 16. Security-sensitive review checklist

Before merging a bridge or authentication change, verify:

- OAuth refresh credentials remain server-side;
- pairing codes are single-use;
- pairing codes expire;
- bridge tokens can be revoked;
- tokens are not logged;
- request bodies have sensible size limits;
- post text and other user-controlled values have appropriate bounds;
- OAuth callback/state validation remains intact;
- production OAuth uses an externally reachable HTTPS URL;
- no credentials or runtime session files are committed.

Before merging a Mac OS 9 transport change, verify:

- no blocking modern networking dependency was introduced;
- TLS remains delegated to macTLS;
- Open Transport remains the networking substrate;
- event-loop yielding is preserved;
- response sizes are bounded;
- malformed network input is handled safely.

---

## 17. Agent workflow

Before editing:

1. Read this file.
2. Read the README.
3. Inspect the relevant source, public headers and callers.
4. Inspect Wolfram when the change crosses the transport boundary.
5. Inspect `docs/BRIDGE_PROTOCOL.md` for protocol changes.
6. Inspect CI and build files when changing build inputs.
7. Check existing branches and pull requests when the task concerns ongoing work.

During editing:

- preserve the two-layer architecture;
- keep changes scoped;
- prefer existing abstractions;
- preserve C89/CodeWarrior compatibility on the Mac side;
- avoid unnecessary dependencies;
- update documentation alongside behavioural changes.

After editing:

- run the relevant checks;
- inspect the final diff;
- check for secrets and generated artifacts;
- update this file when architecture or constraints change.

Do not leave speculative compatibility APIs or dead abstractions behind.

---

## 18. Current state and roadmap

The current branch contains the first working bridge architecture and Mac-side bridge client.

### Completed

- Platinum Bridge Node.js service;
- AT Protocol OAuth integration;
- browser-assisted pairing;
- short-lived pairing codes;
- revocable bridge tokens;
- file-backed bridge state;
- bridge profile/timeline/notifications/post endpoints;
- Mac OS 9 bridge client API;
- persistent Classic Mac configuration;
- account session lifecycle and revocation handling;
- native event-driven application shell;
- initial Classic Mac UI layout and visual specification;
- interactive navigation and timeline selection;
- native TextEdit compose window and bridge-backed post submission;
- live Profile and Notifications windows;
- account Preferences and first-run/re-pairing UI;
- bounded MacRoman/UTF-8 text conversion;
- Wolfram Mac OS 9/Open Transport transport;
- macTLS integration;
- bridge protocol documentation;
- bridge CI.

### In progress

- richer Unicode/emoji editing beyond the MacRoman character repertoire;
- user-facing error handling;
- real hardware validation.

### Later

- richer post/thread interactions;
- media handling appropriate to Classic Mac constraints;
- local caching;
- accessibility and keyboard navigation polish;
- release packaging;
- installation/distribution documentation.

Do not expand the roadmap merely to mirror the modern Bluesky feature set. Prioritise a coherent native Mac OS 9 client.

Keep this section current as milestones are completed.
