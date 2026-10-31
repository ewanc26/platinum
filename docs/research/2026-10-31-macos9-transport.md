# Mac OS 9 transport and TLS

Date: 2026-10-31.

## Sources

- https://github.com/mplsllc/macTLS (README, fetched)
- https://github.com/yllan/ClassicNet (README, fetched earlier)
- https://bearssl.org/ (TLS 1.2 only)
- https://github.com/autc04/Retro68
- Wolfram `include/wolfram/macos9_tls.h` and `src/transport/macos9_tls.c`
  (local checkout, v0.44.2)

## Findings

- macTLS now documents **TLS 1.3 and TLS 1.2**, with a hand-written 1.3
  handshake over BearSSL primitives, fallback to BearSSL for 1.2, X25519,
  ChaCha20-Poly1305, and a 121-anchor Mozilla root bundle built in. It says the
  chain, hostname and expiry are checked.
- macTLS is explicitly **PowerPC / Carbon / CodeWarrior 8 C89**. It is not a
  68k library. Platinum's 68k build can keep compiling, but it cannot do
  TLS-backed AT Protocol networking with macTLS. This is a real platform
  limitation, not a build flag.
- macTLS's README says it now ships `macEntropy`: a SHA-256 pool over the
  high-resolution clock, mouse, stack noise, packet timing and optional live
  input, plus a seed file in Preferences, feeding BearSSL's HMAC-DRBG. It
  reports hardware validation on a G3. See the entropy note.
- Wolfram's `wolfram-macos9-transport` is a thin XRPC/HTTP layer over
  `OSTLS_*`; it treats `kOSTLSAsync_EntropyFail` as a failure, so an entropy
  failure is surfaced rather than ignored.
- The local Wolfram checkout does not pin a macTLS commit. Its CMake asks for
  `WOLFRAM_MACTLS_ROOT`; the macTLS version is whatever the builder supplies.
  CI checks only compile the transport against stubs.
- ClassicNet is Apache-2.0, PowerPC-only, TLS 1.2/1.3, HTTP/2, and verified on
  OS 9.2.2 under QEMU. It is a full second stack (mbedTLS + HTTP), and adopting
  it would not restore 68k networking.

## Not verified

- Whether the macTLS revision with TLS 1.3/`macEntropy` is what Wolfram v0.36.2
  (the CI pin) expects. The public header uses the async `OSTLS_*` API, which
  the README says is stable, but no build has been made here.
- No live PDS handshake. The Mozilla anchor bundle should cover current
  Let's Encrypt chains, but a chain served by a particular PDS, and the guest's
  clock, are untested.
- No QEMU or physical Mac OS 9 run. The harness has no owner-supplied image.

## ADR impact

- ADR 0001 is revised: the transport choice stands, but "TLS 1.2 ceiling" is
  wrong and 68k networking is not available with macTLS.
- ADR 0005 is revised: macEntropy is a real, documented seeding subsystem; the
  OAuth block is now about integration and verification, not the absence of a
  source.
