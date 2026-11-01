# Duplication audit (#39)

The bridge and the Mac client keep only platform UI, input and storage.
Shared logic belongs in Wolfram, which Cobalt and Indigo already use.
This note records the per-file decisions; `tools/ownership.txt` is the
enforced table and `tools/check-flow.sh duplication` fails a source
missing from it.

Audited against a read-only Wolfram checkout (2026-10-05).

1. `macos9/json_min.c` stays. Bounded C89 reader for the bridge
   protocol Platinum owns; Wolfram's `json.h` sits on cJSON (C99),
   unusable on the Mac target.

2. `macos9/text_codec.c` stays. MacRoman and UTF-8 tied to the Mac
   script codepage; Wolfram has no text-encoding module.

3. `macos9/timeline.c`, `notifications.c`, `profile.c` and the other
   `*_feed.c` view models stay. They are bounded display models of
   shapes the bridge defines; the wire contract lives in
   `docs/BRIDGE_PROTOCOL.md`.

4. `bridge/src/domain/api.ts` is a port. Post and notification shaping
   and code-point truncation duplicate the intent of Wolfram's
   `post_display.h` ("thin clients must not parse app.bsky record
   JSON themselves"). The bridge is Node and cannot call C, so the
   shaping stays as a tested port until Wolfram publishes the
   display-model contract (field list plus vectors), the same decision
   as the update module (wolfram#106).

5. `bridge/src/auth` overlaps Wolfram's hosted OAuth node
   (`docs/oauth-node.md`, `oauth_node.h`) and waits on the pairing
   contract (wolfram#101). Pairing codes and bridge tokens are
   Platinum's own and stay.

6. Muted words and the update manifest are Wolfram candidates
   (wolfram#106 for updates). `macos9/muted_words.c` is a C89 port
   because Wolfram's header needs C99.

Guard: `tools/check-flow.sh duplication` rejects a new file under
`macos9/` or `bridge/src/` not listed in `tools/ownership.txt`,
a listed file that no longer exists, an unknown owner, a `port`
without a Wolfram issue or header, and an AT Protocol method string
outside an `atproto` module. That is the drift check this issue
asked for, kept under `tools/` because it runs in CI.
