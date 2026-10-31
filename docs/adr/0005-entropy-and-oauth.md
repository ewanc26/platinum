# 0005: Entropy, DPoP and OAuth

- Status: revised 2026-10-31; OAuth/DPoP still blocked, for integration and review reasons
- Date: 2026-10-31
- Release gate: no OAuth or DPoP code may ship until this ADR is revised

## Context

Classic Mac OS has no hardware random number generator. When this ADR was
first written, the available note was that macTLS seeded BearSSL from a weak
platform source. Research on 2026-10-31 found that current macTLS ships
`macEntropy`: a SHA-256 pool fed by timer jitter, mouse movement, stack noise,
packet-arrival timing and optional live input, with a Preferences seed file and
an HMAC-DRBG, reported as hardware-validated on a G3. That is better evidence
than the original premise, but it is a vendor claim about a revision Platinum
has not yet pinned, built or run.

AT Protocol OAuth with DPoP needs more: a long-lived P-256 private key created
on the device and used to sign every token request. A weak key is not a
temporary exposure; it is the account's proof of possession for as long as it
exists.

## Decision

1. Phase 1 uses an app password and `createSession`/`refreshSession`. It creates
   no long-lived keys and does no DPoP.
2. **Do not implement OAuth, PAR, PKCE or DPoP key generation** until this ADR
   is replaced with a measured entropy design.
3. A mouse/keyboard timing pool hashed into a DRBG is a candidate, but it is not
   accepted on its own. It needs a written design, an implementation with a
   vetted DRBG (not a hand-rolled mixer), a way to gather enough events before
   the first key, and a test that the output is not predictable across boots on
   the same hardware.
4. TLS still matters in phase 1. The handshake's entropy comes from macTLS;
   the linked revision must include `macEntropy`, and the seed path must be
   exercised on real PowerPC hardware (including a cold boot and an
   `EntropyFail` test) before the client is trusted with a real account. This is
   a release checklist item, not something this change can prove on a host.
5. A passphrase-derived key is not an acceptable substitute for a hardware or
   well-seeded DRBG just because it is available; the KDF and the seed still
   need the same review.

## Consequences

- No OAuth pairing flow on the Mac. The bridge's browser pairing disappears
  when the bridge does; app-password sign-in is the only phase 1 path.
- The 68k and PowerPC builds share the same limitation; a fix has to work on
  both.
- DPoP-based features and any "sign in with your PDS account in a browser" flow
  remain blocked. That is a deliberate account-safety choice, not an oversight.
