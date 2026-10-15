# Native bridge plan

The Node bridge exists because the Mac cannot do modern TLS, DPoP OAuth or DID
resolution. The plan is a native C bridge that does the same jobs, built on
Wolfram, so Platinum no longer needs Node. This file lists the parts, which
ones can start now, and what is blocked.

## Parts

| # | Part | Status | Depends on |
|---|------|--------|------------|
| 1 | PLIM encoder (RGBA8 to the bridge's byte-exact PLIM) | can start | bridge/test/vectors/image/convert.json |
| 2 | Config and health (`GET /health`, env, version) | can start | nothing |
| 3 | Token store: CSPRNG, SHA-256 at rest, bounded lookup | can start | #116 (storage design) |
| 4 | Atomic file storage for every stored file | can start | #117 |
| 5 | Mac pairing code and `/v1/pair` | can start | Part 3 |
| 6 | App-password policy, service-URL rules, failure limiter | can start | Part 4 |
| 7 | OAuth glue (confidential client, `/login`, callback, client metadata) | partly blocked | #75, Wolfram gaps in oauth/ |
| 8 | Domain routes (about 28 `/v1/*` routes) | can start, one route family at a time | Wolfram typed headers |
| 9 | Image route `GET /v1/image` with the 2048 px and alpha rules | blocked | #118 |
| 10 | HTTP server with the chosen transport | blocked | #119 (transport decision) |

## Blocked

- **#119, the Mac link transport.** Part 10 cannot be finished until the
  transport is chosen. The choice goes into this file and into
  `docs/BRIDGE_PROTOCOL.md` first.
- **#118, the image source rules.** Part 9 waits on whether the C path matches
  the JS (flatten first, and the same size limit) or the JS changes.
- **#75, the OAuth node.** Part 7 must not start until the node's session
  persistence is settled in Wolfram.

## Rules for the parts

- Each part has byte-exact or behaviour-exact tests against the existing
  vectors and the TypeScript tests, so the two bridges can be compared.
- C89 for anything the Mac build checks; Wolfram's C23 is used only in the
  bridge itself.
- No part adds a second AT Protocol stack. Protocol work goes to Wolfram.
