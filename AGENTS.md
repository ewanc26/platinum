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
