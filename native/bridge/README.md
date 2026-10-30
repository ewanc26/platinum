# Native bridge (C)

The C side of the native bridge described in `docs/NATIVE_BRIDGE.md`. Each part is
one module with one header, one `.c` file, and one host test file. The route and
policy modules are C89 and must build with:

    cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -c <file>

The adapters that call Wolfram (`*_wolfram.c`, `*_live.c`) are built apart, as C2x
against Wolfram's headers and linked to libwolfram.

Host tests are built and run with `make -C native/bridge test`; the Wolfram-backed
parts (timeline, notifications, profile, publishing, images) are driven against a
mock PDS with `make -C native/bridge live`.

`make -C native/bridge platinum-bridge` builds the runnable binary. It reads the
same `PLATINUM_BRIDGE_*` variables as the Node service, including
`PLATINUM_BRIDGE_OAUTH_NODE` (default `https://oauth.croft.click`), and keeps its
token store and session files under `PLATINUM_BRIDGE_DATA_DIR`. `--check` loads
the configuration and store and exits without listening.

This is the destination for the whole bridge: every route the Node service serves
is being moved here, and no feature is added to Node alone. The native binary now
serves sign-in through the shared hosted OAuth node, so the Node service is the
reference rather than the only runnable bridge. One limitation follows from the
node: it keeps OAuth sessions in memory, so restarting the node invalidates every
installation that signed in through it until the user pairs again. Wolfram session
persistence is still the blocker for making that durable. `docs/NATIVE_BRIDGE.md`
tracks the remaining parts. Nothing here is linked into the Mac build yet.
