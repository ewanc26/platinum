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

## Implementation status

The protocol/session layer exposes persistence as a callback so host tests use
a fake store. The Carbon Keychain implementation is **not** in this change: the
Retro68 header and CarbonLib/KeychainLib link details are unverified, and a
plausible-looking but untestable Keychain call is worse than no persistence.
Until it is implemented and compiled for PowerPC, the Mac client keeps the
session in memory and asks for the app password again at launch. That is a
known parity gap, not a silent downgrade.

## Consequences

- A stolen Preferences file no longer yields a usable account credential.
- A stolen Keychain item still does, if the attacker can unlock the keychain.
  Keychain is protection at rest, not against an unlocked machine.
- The 68k build cannot network with macTLS anyway; Keychain persistence is a
  PowerPC path in practice.
- Keychain access may prompt on first use; the UI must allow for that rather
  than treating it as a protocol failure.
