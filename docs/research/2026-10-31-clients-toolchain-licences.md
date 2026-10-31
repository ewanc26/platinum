# Reference clients, toolchain and licences

Date: 2026-10-31.

## Sources

- https://github.com/yllan/PlatinumSky
- https://github.com/yllan/Palaeomastodon
- https://github.com/smallsco/macstodon
- https://github.com/autc04/Retro68
- https://github.com/mplsllc/macTLS
- https://github.com/bluesky-social/atproto
- GitHub API licence metadata, 2026-10-31

## Findings

- **PlatinumSky** and **Palaeomastodon** have no licence file detected by
  GitHub. They can be read for behaviour and interface ideas, but no code,
  strings, assets or generated files may be copied. Their patterns (direct
  XRPC, app-password session, Preferences/Keychain storage, cooperative worker,
  paginated timeline, thread navigation) are consistent with this migration.
- **Macstodon** is MIT, but its networking uses an SSL-stripping proxy. That is
  incompatible with native verified TLS; only window/layout ideas are usable.
- **ClassicNet** is Apache-2.0, PowerPC-only, and not adopted (ADR 0001).
- **macTLS** is MIT and is already the TLS implementation under Wolfram's
  transport. Its README states CodeWarrior 8 C89, 16 MB Carbon partition,
  ~50 KB per connection, no threads, and a Pump-based API. That is compatible
  with the project's constraints, but its current revision must be pinned by
  the builder.
- **Retro68** is a GCC cross-toolchain for 68k and PowerPC with the Multiversal
  Interfaces. The repository already enforces `-std=c90`, no extensions,
  `-Werror`, `-Wdeclaration-after-statement`, `-Wstrict-prototypes` and
  `-Wvla` for every Mac source. CI's C89 job is a dialect check, not a native
  CodeWarrior build.
- The Wolfram Mac OS 9 transport is C89 and depends on macTLS + Open Transport.
  It is a separate static library, so the Mac app does not link the C23 SDK.

## ADR impact

- ADR 0001 stands on transport choice, with the PowerPC/68k correction.
- ADR 0006 is confirmed: no third-party code is copied; macTLS/BearSSL stay
  external under their MIT licences; ClassicNet remains unused.
- No new dependency is introduced by this research.

## Not verified

- Actual Retro68 build of the new modules; the toolchain is not installed on
  this host. CI compiles them, but that is not a CodeWarrior build.
- Whether the Multiversal Interfaces include `Keychain.h`; see the storage note.
- Real-hardware performance and memory of any native JSON/HTTP path.
