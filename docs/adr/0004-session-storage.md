# 0004: Session storage

- Status: superseded by ADR 0007 on 2026-10-31; the "no Keychain" premise was wrong

## Context

Phase 1 signs in with an app password through `com.atproto.server.createSession`.
That returns an access JWT, a refresh JWT, the DID and the handle. The Mac must
survive a restart without asking for the password again, and it must not leave
the password or a long-lived token where it can be logged or casually read.

Mac OS 9 has no Keychain and no process isolation worth relying on. The
Preferences file is the only practical store, and it is not encrypted.

## Decision

- Persist the **refresh JWT**, the DID, the handle and the PDS base URL in the
  Preferences file, through the existing `prefs_file.c` path.
- Keep the **access JWT in memory only**. On launch, call `refreshSession` with
  the stored refresh token before any other request.
- **Never persist the app password.** It is used for one `createSession`
  request and wiped from the entry buffer and the local copy immediately.
- Refresh when the access token is within 60 seconds of its `exp` claim. The
  claim is read without verifying the signature and is only a scheduling hint;
  a 401 from the server is the real signal, and the client refreshes once
  before treating the session as gone.
- On refresh failure, clear the whole session, wipe the stored tokens and show
  the sign-in path. A refresh reply that does not parse is not a session.
- `platinum_atproto_session_clear` overwrites every byte, and no function logs a
  token, password, refresh material or full DID document.

## Consequences and residual risk

- Anyone with access to the Mac's Preferences folder can read the refresh token
  and act as the account until it is revoked. This is a real limitation, not a
  theoretical one. Mitigations are account-side revocation and a short refresh
  lifetime; a passphrase-wrapped store needs an entropy and UI decision and is
  not planned here.
- The access token never touches disk, so a screenshot of Preferences cannot
  leak it.
- Sign-out must call `deleteSession` and then clear local state even if the
  network call fails, so a lost session does not leave the UI claiming to be
  signed in.
