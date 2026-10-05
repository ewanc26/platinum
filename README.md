# Platinum

A Bluesky client for Classic Mac OS 9.

Platinum is a native Mac OS 9 client for the AT Protocol / Bluesky ecosystem. It is designed around the constraints of Classic Mac OS rather than treating Mac OS 9 as a small browser target: the application stays lightweight and delegates modern protocol and authentication work to a separate Platinum Bridge.

**Status: early development.**

The repository currently contains the bridge service, the Mac OS 9 bridge client, and the protocol boundary between them. The native Mac OS 9 application shell and user-facing Bluesky screens are still being built.

## Architecture

```
Classic Mac OS 9
    Platinum
       |
   HTTP + JSON
       |
Platinum Bridge
       |
 AT Protocol
       |
   PDS / AppView
```

The Mac application does not need to implement modern OAuth, DPoP, DID resolution, or a complete AT Protocol client.

The bridge owns:

- AT Protocol OAuth;
- OAuth session and refresh credentials;
- modern HTTPS/TLS;
- DID and identity handling;
- communication with the PDS and AppView;
- the small JSON API exposed to Platinum.

The Mac application owns:

- the native Mac OS 9 user interface;
- bridge configuration and pairing;
- the revocable bridge token;
- rendering and input;
- presentation of timeline, profile and notification data;
- posting through the bridge.

This keeps the Classic Mac side small enough to remain practical on period hardware.

## Current implementation

The repository currently contains:

- a Node.js Platinum Bridge under `bridge/`;
- AT Protocol OAuth using `@atproto/oauth-client-node`;
- short-lived, single-use browser pairing codes;
- revocable long-lived bridge tokens;
- file-backed bridge session and token storage;
- compact, stable timeline and notification response contracts;
- a Classic Mac OS 9 bridge client under `macos9/`;
- persistent Classic Mac configuration and account-session state;
- a native event-driven application shell;
- Wolfram XRPC/HTTP transport integration;
- a documented HTTP/JSON bridge protocol under `docs/BRIDGE_PROTOCOL.md`;
- CI for the bridge TypeScript build and type checking.

The bridge currently exposes:

- `GET /health`
- `GET /client-metadata.json`
- `GET /login?handle=...`
- `GET /atproto-oauth-callback`
- `POST /v1/pair`
- `POST /v1/revoke`
- `GET /v1/profile`
- `GET /v1/timeline`
- `GET /v1/notifications`
- `POST /v1/post`

See [docs/BRIDGE_PROTOCOL.md](docs/BRIDGE_PROTOCOL.md) for the protocol contract.

The bridge can update itself from this repository's releases, but only when you ask it to, and it checks a SHA-256 first. The Mac client can't update itself yet; [docs/AUTO_UPDATE.md](docs/AUTO_UPDATE.md) says what is realistic and what I haven't checked.

What Cobalt and Indigo do that I haven't done yet is listed, with an issue for each gap, in [docs/PARITY.md](docs/PARITY.md). It is checked against the code by CI, so it won't claim more than exists.

## Requirements

### Platinum Bridge

The bridge requires:

- Node.js 22 or newer;
- npm;
- a public HTTPS endpoint for production OAuth deployments;
- an AT Protocol account that supports OAuth.

The bridge is intentionally framework-free and uses Node's native HTTP server.

### Classic Mac OS 9 client

The native client targets period-compatible development environments rather than modern macOS APIs.

The current transport boundary is built around:

- CodeWarrior-era C89-compatible C;
- [Wolfram](https://github.com/ewanc26/wolfram) 0.26.0 or newer;
- Wolfram's Classic Mac OS 9/Open Transport transport;
- [macTLS](https://github.com/mplsllc/macTLS) for TLS;
- a small bounded JSON helper in `macos9/json_min.c` rather than a JSON library,
  because the bridge protocol only needs a few fields read out of a response.

Do not add modern POSIX, libcurl, OpenSSL, pthreads, or other desktop-only networking dependencies to the Mac client.

## Running the bridge

From the repository root:

```sh
cd bridge
cp .env.example .env
npm install
npm run check
npm run build
npm start
```

The development server is available with:

```sh
cd bridge
npm run dev
```

The bridge defaults to `127.0.0.1:8787`.

Signing in with an app password instead of the browser is off unless you set `PLATINUM_BRIDGE_ALLOW_APP_PASSWORD=1`. Read the warning in [docs/BRIDGE_PROTOCOL.md](docs/BRIDGE_PROTOCOL.md) first: over plain HTTP the password crosses your network in the clear. The Mac client has no window for it yet.

For OAuth, set `PLATINUM_BRIDGE_PUBLIC_URL` to the externally reachable HTTPS URL of the bridge. The OAuth client metadata and callback URL are derived from that value.

Do not expose the bridge directly to an untrusted network without an appropriate HTTPS and access-control boundary.

## Pairing

Pairing is intentionally browser-assisted:

1. Open the bridge's `/login?handle=...` URL in a modern browser.
2. Complete AT Protocol OAuth.
3. The bridge displays a short-lived pairing code.
4. Platinum sends the code to `POST /v1/pair`.
5. The bridge returns protocol version 1, a bridge token and the account DID.
6. Platinum stores the bridge token, account DID and installation ID and uses the token for subsequent requests.

The pairing code expires after ten minutes and is single-use.

OAuth refresh credentials never leave the bridge. Platinum only receives the revocable bridge token.

## Security

The bridge is a trusted component. It holds the OAuth session and can act on behalf of the paired account.

The Classic Mac client deliberately does not receive the account's normal password or OAuth refresh credentials.

Bridge tokens are revocable through `POST /v1/revoke`. Treat them as credentials:

- never commit them;
- never print them to logs;
- never put them in bug reports;
- never share a token between installations unless that is an explicit operational decision.

The initial bridge implementation uses local JSON files under `.platinum-bridge/` for OAuth state, sessions and bridge tokens. These files are runtime state and must not be committed.

## Building the Classic Mac client

The Mac OS 9 client is a native application rather than a web wrapper. The current native shell opens a standard Classic Mac window, runs a `WaitNextEvent` event loop, and loads/saves the bridge session from the System Folder's Preferences folder.

Build the Wolfram Mac OS 9 transport first, using the macTLS dependency:

```sh
cmake -S . -B build-macos9-transport \
  -DWOLFRAM_BUILD_MACOS9_TRANSPORT=ON \
  -DWOLFRAM_MACTLS_ROOT=/path/to/macTLS
cmake --build build-macos9-transport --target wolfram-macos9-transport
```

The exact CodeWarrior project/resource layout is still under development. The checked-in C sources are the portable application layer; the final project file and resource fork will be added with the first UI milestone.

## Repository layout

```
platinum/
├── AGENTS.md
├── README.md
├── docs/
│   └── BRIDGE_PROTOCOL.md
├── bridge/
│   ├── src/
│   ├── .env.example
│   ├── package.json
│   └── tsconfig.json
├── macos9/
│   ├── application.c
│   ├── application.h
│   ├── bridge_client.c
│   ├── bridge_client.h
│   ├── compose.c
│   ├── compose.h
│   ├── pairing.c
│   ├── pairing.h
│   ├── config.c
│   ├── config.h
│   ├── json_min.c
│   ├── json_min.h
│   ├── main.c
│   ├── session.c
│   ├── session.h
│   ├── test/
│   │   ├── test_bridge_client.c
│   │   └── test_json_min.c
│   ├── ui.c
│   └── ui.h
└── .github/
    └── workflows/
        └── ci.yml
```

The layout will grow as the native application shell, UI, persistent settings and account surfaces are implemented.

## UI design

The native UI specification lives in [docs/UI_DESIGN.md](docs/UI_DESIGN.md). It defines the Classic Mac window model, menu structure, layout, typography, keyboard behaviour and interaction priorities for the client.

The native shell now also has live Profile and Notifications navigation with separate modeless document windows. Refresh loads up to 20 posts from `GET /v1/timeline`, parses only the bridge-owned response contract, and updates the existing Classic Mac selection and detail UI. The native compose window can also submit posts through `POST /v1/post`; the current release accepts up to 300 MacRoman characters and converts them to UTF-8 for the bridge.

The native shell is implemented under `macos9/ui.c` and `macos9/ui.h`; Profile and Notifications are separate native modules alongside the Timeline and compose surfaces. The shell now has navigation selection, live timeline selection, keyboard scrolling, Classic Mac menu/input handling, a native TextEdit compose window, bridge-backed post submission, a collapsible detail pane, live Profile and Notifications windows, account Preferences with bridge-token revocation/sign-out, and a bounded MacRoman/UTF-8 text codec.

## Development

Read [AGENTS.md](AGENTS.md) before making changes. It contains the deeper architecture, platform constraints, security boundaries, testing expectations and workflow rules for this repository.

Keep the two sides of the project separate. If a capability can live in the modern bridge, do not reproduce the modern implementation on Mac OS 9.

Use focused commits and update documentation when the protocol, build requirements or architectural boundaries change.

## Testing

CI runs two jobs.

The bridge job checks TypeScript, runs the bridge tests and produces the
production build:

```sh
cd bridge
npm run check
npm test
npm run build
```

The `Mac OS 9 sources (C89)` job compiles **every** source under `macos9/` as
strict C89 against the released Wolfram public headers, and runs the Mac-side
tests. A CI runner has no Classic Mac SDK, so the job compiles against the
declaration-only SDK headers in [`macos9/test/sdk-stubs/`](macos9/test/sdk-stubs/);
read that directory's README for exactly what a clean run does and does not
prove.

The same check runs locally against a Wolfram checkout:

```sh
clang -std=c89 -pedantic-errors -Wall -Wextra -Wno-unused-parameter -Werror \
  -Wdeclaration-after-statement -Wstrict-prototypes -Wvla \
  -I macos9 -I macos9/test/sdk-stubs -I ../wolfram/include \
  -o /tmp/test_bridge_client \
  macos9/bridge_client.c macos9/json_min.c macos9/test/test_bridge_client.c
/tmp/test_bridge_client
```

`test_bridge_client` stubs the Wolfram transport entry points, so it covers the
pairing path and the malformed-response paths without a network or a Mac.

A successful modern-host build does not prove Classic Mac OS 9 compatibility. Native client changes must be validated with the intended CodeWarrior/Open Transport/macTLS environment when available.

The Mac client uses `macos9/json_min.c` for JSON rather than cJSON, which is
C99 and not part of the CodeWarrior-era target. No Mac source reaches for cJSON,
libcurl, OpenSSL, pthreads, `<stdint.h>`, `<stdbool.h>` or `<strings.h>`, and
because CI compiles every source those are compile errors rather than
conventions to remember.

## Licence

See [LICENSE](LICENSE).
