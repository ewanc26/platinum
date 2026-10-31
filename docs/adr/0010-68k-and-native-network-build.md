# 0010: Native networking is PowerPC-only; the 68k build stays a compile target

- Status: accepted
- Date: 2026-10-31
- Related: ADR 0001 (transport), ADR 0005 (entropy), ADR 0007 (Keychain)

## Context

Platinum has built for both 68k and PowerPC since it was a bridge client. That
was possible because the bridge spoke plain HTTP and Wolfram's Mac OS 9
transport wrapped Open Transport for both CPUs.

The native client reaches PDSes and identity services over HTTPS and relies on
macTLS for certificate verification. macTLS is PowerPC/Carbon/C89 only: its
current implementation is explicitly not a 68k library (ADR 0001, research
note `docs/research/2026-10-31-macos9-transport.md`). A 68k client therefore
cannot make a verified TLS connection on the current transport path.

The 68k build also cannot generate the entropy that a TLS handshake needs
without the weak base source ADR 0005 documents. That is a separate concern,
but it points the same way: a 68k network client is not a configuration this
project can support, verify or ship.

The options considered were:

1. **PowerPC-only networking, 68k kept as a compile target.** Both CPUs keep
   building; only PowerPC is a supported network configuration.
2. **Keep the bridge for 68k only.** 68k would keep plain-HTTP access to a Node
   service. That preserves a second runtime, keeps the bridge alive
   indefinitely, and means the project maintains two protocol paths.
3. **Find a 68k TLS transport.** No licensed, verified 68k TLS stack is
   available here, and adopting one would add a second TLS implementation,
   which the repository rule forbids without a concrete reason.

## Decision

Choose option 1.

- Native AT Protocol networking is supported on PowerPC Mac OS 9 only.
- The 68k application continues to compile and link in CI and in
  `tools/build-macos9.sh 68k`. It remains useful as a source-dialect and
  application-build check for the shared UI.
- The 68k build is not a supported network client. It must not be presented as
  one, and once the bridge is removed it will not be shipped as a working
  client.
- The bridge is not retained "for 68k". There is no second runtime; the bridge
  is deleted when per-screen parity is reached, as the migration plan already
  requires.
- CI continues to build both CPUs. A failure in either is still a failure.

## Consequences

- `tools/build-macos9.sh 68k` keeps working; the output is a build artefact,
  not a supported client.
- README, AGENTS.md and the release notes must say PowerPC-only for networking
  and must not claim 68k connectivity.
- The release script may still package both architectures for the paused
  release process, but the 68k binary is labelled unsupported for networking
  once the bridge is gone.
- Reversing this decision requires a 68k TLS transport that verifies
  certificates and has a credible entropy story. That is a new ADR, not a
  configuration flag.
- This decision does not affect the 68k UI, layout or host tests: those are
  CPU-independent and continue to run for both builds.
