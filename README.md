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
- cJSON for the small amount of JSON parsing required by the bridge protocol.

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
│   ├── config.c
│   ├── config.h
│   ├── main.c
│   ├── session.c
│   ├── session.h
│   ├── ui.c
│   └── ui.h
└── .github/
    └── workflows/
        └── ci.yml
```

The layout will grow as the native application shell, UI, persistent settings and account surfaces are implemented.

## UI design

The native UI specification lives in [docs/UI_DESIGN.md](docs/UI_DESIGN.md). It defines the Classic Mac window model, menu structure, layout, typography, keyboard behaviour and interaction priorities for the client.

The native shell now also has live Profile navigation and a separate Profile document window. Refresh loads up to 20 posts from `GET /v1/timeline`, parses only the bridge-owned response contract, and updates the existing Classic Mac selection and detail UI. The native compose window can also submit posts through `POST /v1/post`; the current release accepts up to 300 ASCII characters while the MacRoman-to-UTF-8 conversion path is still being designed.

The first UI shell is implemented under `macos9/ui.c` and `macos9/ui.h`. The shell now has navigation selection, live timeline selection, keyboard scrolling, Classic Mac menu/input handling, a native TextEdit compose window, bridge-backed post submission, a collapsible detail pane and a separate live Profile window.

## Development

Read [AGENTS.md](AGENTS.md) before making changes. It contains the deeper architecture, platform constraints, security boundaries, testing expectations and workflow rules for this repository.

Keep the two sides of the project separate. If a capability can live in the modern bridge, do not reproduce the modern implementation on Mac OS 9.

Use focused commits and update documentation when the protocol, build requirements or architectural boundaries change.

## Testing

The bridge's current CI checks TypeScript compilation and produces the production build:

```sh
cd bridge
npm run check
npm run build
```

The bridge also has a test command:

```sh
npm test
```

Run it when bridge tests are present in the working tree.

A successful modern-host build does not prove Classic Mac OS 9 compatibility. Native client changes must be validated with the intended CodeWarrior/Open Transport/macTLS environment when available.

## Licence

See [LICENSE](LICENSE).
