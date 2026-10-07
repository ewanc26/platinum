<p align="center">
  <img src="docs/logo.svg" alt="Platinum" width="420">
</p>

<p align="center">
  <a href="https://github.com/ewanc26/platinum/actions/workflows/ci.yml"><img src="https://github.com/ewanc26/platinum/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/ewanc26/platinum/releases/latest"><img src="https://img.shields.io/github/v/release/ewanc26/platinum?sort=semver" alt="Latest release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/ewanc26/platinum?label=licence" alt="Licence"></a>
  <a href="https://github.com/sponsors/ewanc26"><img src="https://img.shields.io/github/sponsors/ewanc26?logo=githubsponsors&logoColor=white&label=sponsors" alt="Sponsor"></a>
</p>

# Platinum

A Bluesky client for Classic Mac OS 9, with a small Node bridge doing the parts a 1990s operating system can't.

## What it is

Platinum is the Mac OS 9 member of my stack of AT Protocol clients: [Wolfram](https://github.com/ewanc26/wolfram) underneath, then [Cobalt](https://github.com/ewanc26/cobalt) on the Wii U and [Indigo](https://github.com/ewanc26/indigo) on the 3DS. It is not a browser pointed at a web app. It is a C89 application that wants to look like it belongs in a System Folder, and it is named after the Platinum appearance, which I am not sorry about.

Mac OS 9 can't do modern TLS, OAuth with DPoP, or DID resolution, so I stopped asking it to. The Platinum Bridge, a Node service, does all of that and gives the Mac a deliberately boring HTTP and JSON API plus a revocable token. The Mac side keeps the window, the input, the storage and the drawing.

```
Classic Mac OS 9  --HTTP + JSON-->  Platinum Bridge  --AT Protocol-->  PDS / AppView
```

## Status

Early, and I'd rather say so plainly.

- **The bridge works.** It does OAuth pairing, issues and revokes tokens, and serves a profile, a timeline, notifications and posting. App-password sign-in exists but is off unless you turn it on, and the Mac client has a window for it. The bridge has tests and they run in CI.
- **The Mac client is written but unproven.** It has a window, menus, a timeline, threads, profiles, people lists, search, notifications, compose (with replies, quotes and reply limits) and preferences. CI compiles every source as strict C89 against declaration-only stand-ins for the Mac SDK, and runs the pairing and JSON tests. That checks dialect and types. It is not a CodeWarrior build, and nothing in this repository records it running on a real Mac or an emulator.
- **Much is missing.** The timeline and notifications page back now. You can like, repost, reply, quote, follow, mute and block. Images, deleting your own posts and a diagnostics window are still to come. [docs/PARITY.md](docs/PARITY.md) lists every gap against Cobalt and Indigo with an issue for each, and CI checks that table against the code so it can't claim more than exists.

## Install

There is no packaged release of the Mac application yet. The bridge can be run from this repository (see below) or from a release archive, and it can update itself from this repository's releases when you ask it to: [docs/AUTO_UPDATE.md](docs/AUTO_UPDATE.md) says how, what the checksum does and doesn't protect against, and why the Mac client can't update itself yet.

Requirements for the bridge: Node.js 22 or newer, and a public HTTPS address if you want OAuth outside your own machine. The Mac client is aimed at CodeWarrior-era tools, [Wolfram](https://github.com/ewanc26/wolfram) 0.26.0 or newer (CI builds against 0.34.0) with its Open Transport transport, and [macTLS](https://github.com/mplsllc/macTLS). The build is not finished; see below.

## Use

Run the bridge:

```sh
cd bridge
cp .env.example .env
npm install
npm run build
npm start
```

It listens on `127.0.0.1:8787`. For OAuth, set `PLATINUM_BRIDGE_PUBLIC_URL` to the externally reachable HTTPS address; the client metadata and callback are derived from it. Don't put the bridge on an untrusted network without HTTPS and some access control in front of it.

Pairing is done in a browser, once:

1. Open `/login?handle=you.example.com` on the bridge.
2. Complete the OAuth flow.
3. The bridge shows a six-character code, good for ten minutes and one use.
4. Type it into Platinum. The Mac stores a bridge token, your DID and an installation ID.

Your OAuth refresh credentials never leave the bridge. Tokens are revocable with `POST /v1/revoke`; treat them as credentials and keep them out of logs and bug reports. Signing in with an app password instead is possible if you set `PLATINUM_BRIDGE_ALLOW_APP_PASSWORD=1`, but read the warning in [docs/BRIDGE_PROTOCOL.md](docs/BRIDGE_PROTOCOL.md) first: over plain HTTP the password crosses your network in the clear. The Mac client has no window for it yet.

The wire protocol is in [docs/BRIDGE_PROTOCOL.md](docs/BRIDGE_PROTOCOL.md). The window and menu design is in [docs/UI_DESIGN.md](docs/UI_DESIGN.md).

## Build

Bridge:

```sh
cd bridge
npm run check
npm test
npm run build
```

Mac client: build Wolfram's Mac OS 9 transport against macTLS first.

```sh
cmake -S . -B build-macos9-transport \
  -DWOLFRAM_BUILD_MACOS9_TRANSPORT=ON \
  -DWOLFRAM_MACTLS_ROOT=/path/to/macTLS
cmake --build build-macos9-transport --target wolfram-macos9-transport
```

I don't have a CodeWarrior project or a Retro68 build for the application itself yet; the first is waiting on the UI settling and the second on a legal way for CI to get Apple's Open Transport headers (see [#41](https://github.com/ewanc26/platinum/issues/41)). The application icon is generated, not drawn: `tools/gen-art.py` writes `docs/logo.svg` and `macos9/resources/icon.r` from one shape, and CI checks they are current. Nothing builds the `.r` into an application yet, so I haven't seen the icon in a Finder.

CI's C89 job is the same check you can run locally against a Wolfram checkout; the flags are in [`ci.yml`](.github/workflows/ci.yml) and [`macos9/test/sdk-stubs/README.md`](macos9/test/sdk-stubs/README.md) explains exactly what a clean run does and doesn't prove. A host build is not Mac OS 9 compatibility.

## Contributing

[CONTRIBUTING.md](CONTRIBUTING.md) has the flow: branch, small conventional commits, a pull request, green CI, merge by rebase. [AGENTS.md](AGENTS.md) has the engineering rules, including the Mac OS 9 constraints (strict C89, cooperative networking, bounded memory, no desktop libraries). If a capability can live in the bridge, it shouldn't be reimplemented on the Mac.

## Licence

AGPL-3.0. See [LICENSE](LICENSE).
