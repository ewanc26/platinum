# Changelog

Release tags are `vX.Y.Z` and must match `bridge/package.json`; the
`release-check` workflow refuses a tag that does not, or that has no section
here.

## [Unreleased]

Releases are paused until Platinum has run on Mac OS 9 (#62).

### Bridge

- Self-update from GitHub releases (`npm run update`), opt-in and confirmed, with SHA-256 verification and rollback.
- Opt-in app-password sign-in (`POST /v1/login/app-password`), separate from OAuth pairing.
- Idempotent like and repost with undo (`POST /v1/like`, `POST /v1/repost`); timeline posts carry `liked` and `reposted`.
- Mark notifications seen (`POST /v1/notifications/seen`), up to a timestamp the client was given.
- `/health` reports the real package version.

### Mac client

- Timeline and notification paging, each keeping at most forty rows.
- Like and repost the selected post from the Post menu.
- Opening or refreshing Notifications marks them seen, up to the newest one shown.
- Windows hilite and deactivate correctly, and a click on a background window brings it forward.
- The application state is no longer on the stack.

### Repository

- Flow checks, a single `CI gate`, the parity matrix, the Wolfram duplication guard, generated logo and icon, and a release dry run on every PR.

## [0.3.1]

The version `bridge/package.json` already carried when releases were first
gated. Earlier history is in git.
