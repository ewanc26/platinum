# Contributing to Platinum

Platinum is a Bluesky client for Classic Mac OS 9: a C89 application that wants to look like it belongs in a System Folder, with a small Node bridge doing the parts a 1990s operating system can't. Thanks for wanting to help!

The bridge (no modern TLS, OAuth or DID resolution on Mac OS 9) is **on its way out**: the Mac is getting its own AT Protocol client over Wolfram's verified Open Transport and macTLS stack. See [`docs/architecture/native-client-migration.md`](docs/architecture/native-client-migration.md), which maps every route to its native replacement.

## Getting started

Build the C89 side per the [README](README.md) (CodeWarrior-era C, Open Transport) and the Node bridge separately. CI runs host tests where possible.

## Notes for this project

- **C89 discipline**: no C99+ conveniences no matter how tempting. This code has to belong in the Platinum-look era, and the compiler enforces it anyway.
- Check the migration doc before bridge work: a bridge feature may already be scheduled for native replacement, and new bridge dependencies extend the thing being removed.
- Native AT Protocol work goes through Wolfram (`macos9/atproto_*.c` modules are Wolfram's Open Transport + mactls stack); don't hand-roll TLS or DID resolution.
- UI work should match Mac OS 9 interface conventions genuinely, not approximate them.
- Conventional commits; focused PRs; explain what you verified (bridge-only changes are testable on any host; Mac-side changes need a real or emulated Mac OS 9 environment).
- AI-assisted PRs welcome; stand behind every line and run what you can.

## Licence

See [LICENSE](LICENSE). Contributions accepted under the same terms.