# AGENTS.md

Guidance for AI coding agents working in Platinum.

## Project overview

Platinum is a Mac OS 9 Bluesky client. The repository is split between the
classic Mac application and the modern Platinum Bridge.

- Classic client: Mac OS 9 compatible C/CodeWarrior-era code.
- Bridge: TypeScript running on Node.js 22 or newer.
- Protocol: small HTTP/JSON API between Platinum and the bridge.
- Authentication: AT Protocol OAuth is handled by the bridge, never by the
  classic client.

## Working rules

- Inspect the README, manifests, CI workflows, and nearby code before editing.
- Preserve existing architecture, naming, formatting, and error-handling
  conventions.
- Keep the Mac client independent from modern AT Protocol authentication.
- Do not add TLS, DPoP, OAuth token handling, or DID resolution to the Mac
  client when the bridge can handle it.
- Keep the bridge protocol documented whenever an endpoint changes.
- Use project scripts for validation; never claim checks that were not run.
- Keep changes scoped and update tests or documentation when behaviour changes.
- Use feature branches and pull requests.
- Never commit OAuth credentials, bridge tokens, session stores, .env files,
  generated build output, or deployment secrets.
- Prefer atomic commits with conventional commit messages.
- Do not introduce generated files unless the repository explicitly requires
  them.

## Bridge security

The bridge is a trusted backend. OAuth refresh credentials stay on the bridge.
The Mac client receives only a bridge token.

Pairing codes must remain short-lived and single-use. Do not log OAuth tokens,
bridge tokens, authorisation codes, or session contents.


## Classic Mac OS 9 client

- Keep Mac OS 9 code strict C89 and compatible with CodeWarrior-era headers and libraries.
- Use Wolfram's `wolfram-macos9-transport` for HTTP/TLS rather than adding libcurl, OpenSSL, pthreads, or modern POSIX networking code to Platinum.
- The Mac client talks only to the Platinum Bridge. OAuth, DPoP, DID resolution, and modern AT Protocol API details stay on the bridge.
- Network operations must yield through Wolfram's Mac OS 9 transport callback so the cooperative event loop remains responsive.
- Keep bridge tokens in application-owned persistent storage; never persist PDS OAuth credentials or refresh tokens on the Mac.
