# 0001: Native transport for the Mac client

- Status: revised 2026-10-31; transport choice stands, TLS/68k claims corrected
- Date: 2026-10-31
- Supersedes: the bridge-only networking rule in `docs/BRIDGE_PROTOCOL.md` (the
  bridge remains for unmigrated screens until parity)

## Context

The Mac client currently reaches the bridge over plain HTTP using Wolfram's
`wolfram-macos9-transport` library. That library already wraps Open Transport
and macTLS (BearSSL), verifies certificates and exposes `wf_xrpc_client_*` and
`wf_http_*` over HTTP/1.1. It is built for both the 68k and PowerPC targets and
is the transport `AGENTS.md` has required since the project began.

The alternative named in the migration brief is [ClassicNet](https://github.com/yllan/ClassicNet):
Apache-2.0, PowerPC Mac OS 8/9 only, TLS 1.2/1.3, HTTP/2 and WebSocket over
Open Transport and mbedTLS, verified on Mac OS 9.2.2 under QEMU.

## Decision

Use Wolfram's XRPC/HTTP client over `wolfram-macos9-transport` and macTLS. Do
not vendor, submodule or link ClassicNet in phase 1.

The native client adds only a policy layer above it: `atproto_url.c` refuses a
non-https service base, so a caller cannot opt out of TLS by accident, and the
session code attaches the bearer token. Certificate verification, redirects,
chunked responses and timeouts stay in Wolfram/macTLS, where they are already
the supported stack.

## Why not ClassicNet

- **68k.** Platinum builds and ships a 68k target. ClassicNet documents itself
  as PowerPC OS 8/9; adopting it would silently drop half the build matrix.
- **One TLS stack.** The repository rule is explicit: no second TLS
  implementation. ClassicNet brings mbedTLS and its own HTTP stack alongside
  BearSSL and Wolfram's HTTP.
- **No new integration risk before a runtime.** CI cannot link Open Transport,
  and no Mac OS 9 image is available here. A second stack could not be verified
  beyond compilation.
- **Licence is not the obstacle.** Apache-2.0 is compatible with Platinum's
  AGPL-3.0, with attribution. The obstacle is platform and duplication.

## Research update (2026-10-31)

macTLS's current README documents TLS 1.3 as well as TLS 1.2: a hand-written
1.3 handshake over BearSSL primitives, with fallback to BearSSL's 1.2 engine,
X25519, ChaCha20-Poly1305 and a 121-anchor Mozilla root bundle. It also
documents `macEntropy` (see ADR 0005). It is explicitly PowerPC/Carbon/
CodeWarrior C89, not a 68k library.

That corrects two claims below: TLS 1.2 is not necessarily the ceiling, and
the 68k build cannot use this transport for networking. The choice of
Wolfram's transport over ClassicNet is unchanged: ClassicNet is also
PowerPC-only, would add a second TLS/HTTP stack, and cannot restore 68k
networking. Whether the Wolfram revision Platinum builds against actually
contains and initialises the TLS 1.3/macEntropy code is unverified.

## Consequences

- TLS 1.2 is available; TLS 1.3 is available only if the linked macTLS revision
  implements it and Wolfram's shim negotiates it. Every PDS must still accept
  at least TLS 1.2.
- The 68k build remains a compile target. It cannot do TLS-backed networking
  with macTLS; a 68k client would need a different transport, which this ADR
  does not choose.
- HTTP/1.1 only; no multiplexing. For one request at a time from a cooperative
  event loop, that is enough.
- The UI must install `wf_macos9_set_yield_callback` so a request pumps the
  event loop instead of freezing the window. The bridge client already relies
  on this; the native path keeps it.
- Revisit this ADR if TLS 1.3 becomes mandatory or if a PowerPC-only build with
  HTTP/2 is justified. If ClassicNet is adopted then, pin a commit in the
  submodule (never a moving branch) and carry its Apache-2.0 `NOTICE`.
