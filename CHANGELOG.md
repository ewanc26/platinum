# Changelog

Release tags are `vX.Y.Z` and must match `bridge/package.json`; the
`release-check` workflow refuses a tag that does not, or that has no section
here.

## [Unreleased]

- Bridge self-update from GitHub releases (`npm run update`), with SHA-256 verification and rollback.

## [0.3.1]

The version `bridge/package.json` already carried when releases were first
gated. Earlier history is in git.
