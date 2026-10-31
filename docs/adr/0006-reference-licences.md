# 0006: Reference projects and licences

- Status: accepted
- Date: 2026-10-31
- Scope: research only; no third-party code was copied into Platinum in this
  change

## Findings

| Project | Licence (checked 2026-10-31) | Use in Platinum |
|---|---|---|
| [PlatinumSky](https://github.com/yllan/PlatinumSky) | No licence file detected by GitHub | Patterns only: direct XRPC, app-password sign-in, session persistence, refresh, cooperative worker. No code copied. |
| [Palaeomastodon](https://github.com/yllan/Palaeomastodon) | No licence file detected by GitHub | Patterns only: timeline rendering, status actions, media, pagination, thread navigation. No code copied. |
| [ClassicNet](https://github.com/yllan/ClassicNet) | Apache-2.0 | Evaluated and not adopted (ADR 0001). No code vendored, no submodule, no external linkage. |
| [Macstodon](https://github.com/smallsco/macstodon) | MIT | UI and window-organisation reference only. Its networking model (an SSL-stripping proxy) is explicitly not adopted. No code copied. |
| [macTLS](https://github.com/mplsllc/macTLS) | MIT | Already an external dependency of Wolfram's Mac OS 9 transport; not modified here. |
| mbedTLS | Apache-2.0 | Only relevant to the ClassicNet path, which was not adopted. |
| BearSSL | MIT | Inside macTLS; unchanged. |

## Decision

A project with no licence grants no reuse rights. PlatinumSky and
Palaeomastodon may be read for behaviour and interface ideas, but no source,
strings, assets or generated files are copied from them. If that changes, the
repository must first obtain permission or a licence and record it here.

ClassicNet's Apache-2.0 is compatible with Platinum's AGPL-3.0, so licence was
not the reason to decline it. If a later ADR adopts it, Platinum must pin an
exact commit (not a branch), keep the upstream `NOTICE`, and state the
modifications in the About or release notes.

Macstodon's MIT licence would permit reuse, but its proxy-based networking is
incompatible with the requirement that the Mac do its own verified TLS, so only
layout and interaction ideas are used.

## Consequences

- No third-party source enters the Mac target in phase 1.
- The Wolfram dependency remains the only linked protocol/transport code; its
  own licence and the macTLS licence travel with it.
