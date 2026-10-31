# 0007: Keychain session storage

- Status: accepted; supersedes ADR 0004
- Date: 2026-10-31

## Context

ADR 0004 concluded that Mac OS 9 had no Keychain and that a refresh JWT could
only live in a plain Preferences file. Research on 2026-10-31 found that this
premise is false. Keychain shipped with Mac OS 8.6, and the Carbon Keychain
Manager (`KCAddGenericPassword`, `KCFindGenericPassword`, `KCDeleteItem`) is
documented for CarbonLib 1.1+ when KeychainLib is present. Mac OS 9.2.2 has
both.

A generic-password item holds arbitrary bytes, so the refresh JWT fits. The
login keychain is normally unlocked at login.

## Decision

- Store the **refresh JWT** as a generic-password Keychain item, keyed by a
  fixed service name and the account DID or handle.
- Keep the **access JWT and the app password in memory only**. Never write
  either to disk.
- Store the DID, handle and PDS URL in the existing Preferences record (they
  are not secrets); never store the refresh token there.
- Use the Keychain item's data as the refresh token and nothing else. Do not
  store the password or a copy of the access token in the item.
- If the Keychain is unavailable at runtime (missing KeychainLib, item access
  denied), keep the session in memory and require sign-in again next launch.
  Do **not** fall back to a plaintext file.
- Clear the Keychain item on sign-out or refresh failure, after clearing local
  state even if the network revocation fails.

## The shim question, decided

A local `Keychain.h` plus a `KeychainLib` link entry would let the code
compile, but not verifiably work. The pinned Retro68 image ships no
`Keychain.h`, no CarbonLib and no KeychainLib (verified 2026-10-31; see
`docs/research/2026-10-31-keychain-surface.md`), so the shim would have to
invent the link surface as well as the declarations, and this repository's
rule is that a plausible-looking but untestable SDK surface is worse than a
documented gap.

**Decision:** keep persistence memory-only until a toolchain with a real
Keychain surface (or a guest/hardware run that can verify a shim) exists. Do
not add a local header or a link stub now. Revisit when the build can compile
and link `KCAddGenericPassword`/`KCFindGenericPassword`/`KCDeleteItem` without
invention.

## Implementation status

The protocol/session layer exposes persistence as a callback so host tests use
a fake store. The Carbon Keychain implementation is **not** in this change.

On 2026-10-31 the pinned Retro68 image
(`ghcr.io/autc04/retro68@sha256:2c2ec5ca...`, the digest CI uses) was inspected
directly: it ships no `Keychain.h`, no `CarbonLib` and no `KeychainLib`. The
build therefore cannot compile or link a Keychain call today, and a
hand-written header would be untestable calls (see
`docs/research/2026-10-31-keychain-surface.md` for the check and the smallest
shim).

Until that shim exists and has been run in a Mac OS 9 guest, the Mac client
keeps the session in memory and asks for the app password again at launch.
That is a known parity gap, not a silent downgrade, and there is no plaintext
fallback.

## Consequences

- A stolen Preferences file no longer yields a usable account credential.
- A stolen Keychain item still does, if the attacker can unlock the keychain.
  Keychain is protection at rest, not against an unlocked machine.
- The 68k build cannot network with macTLS anyway; Keychain persistence is a
  PowerPC path in practice.
- Keychain access may prompt on first use; the UI must allow for that rather
  than treating it as a protocol failure.
