# Native notifications

Date: 2026-10-31.

## Sources

- `app.bsky.notification.listNotifications` and
  `app.bsky.notification.updateSeen`:
  <https://github.com/bluesky-social/atproto/tree/main/lexicons/app/bsky/notification>
- AT Protocol API reference:
  <https://docs.bsky.app/docs/api/app-bsky-notification-list-notifications>
- The bridge's existing parser, which runs `@atproto/api` and is pinned by
  tests: `bridge/src/domain/api.ts`, `bridge/src/domain/types.ts`
- Wolfram transport: `include/wolfram/xrpc.h`, `src/transport/xrpc_mac9.c`
- The client's own request layer: `macos9/atproto_client.c`,
  `macos9/atproto_feed.c`

## What the response looks like

`listNotifications` takes `limit` (bounded) and `cursor`, and returns
`{"cursor": "...", "notifications": [...]}`. Each notification carries:

- `uri`, `cid` — the notification record;
- `author` — a profile view with `did`, `handle` and `displayName`;
- `reason` — `like`, `repost`, `follow`, `mention`, `reply`, `quote`,
  `starterpack-joined`, and the other reasons the AppView emits;
- `reasonSubject` — optional, absent for a follow;
- `record` — the subject record, a shape that depends on `reason`;
- `isRead` — a boolean;
- `indexedAt` — an RFC 3339 timestamp;
- `labels` — optional.

`updateSeen` takes `{"seenAt": "<datetime>"}` and returns an empty body. The
client only checks its status.

## What the client uses

The window already shows author, handle, reason, time and an unread marker, so
the native parser reuses `notifications_parse_item` unchanged: it reads exactly
these fields, treats a missing or non-boolean `isRead` as unread, and ignores
the rest. Nothing in the row model needed to change.

Paging keeps the bridge's rules: 20 per request, 40 kept, the cursor copied
exactly or not at all, older pages never move the seen mark, and the seen mark
is the newest loaded item's own `indexedAt`, never the Mac clock.

## Verified

- The request shape and field names are consistent with the bridge parser and
  with the SDK types that parser consumes; the host tests pin the NSIDs, the
  parameter names, the cursor and the seen body.

## Not verified here

- Both lexicon URLs above timed out from this host on 2026-10-31, so the
  documents were not read directly in this session. The field list rests on the
  bridge parser, which is generated from `@atproto/api`, and on its passing
  tests. This is weaker evidence than reading the lexicon, and is recorded as
  such.
- No live request was made. A real `updateSeen` response is not parsed, only
  its status.
- Whether the public AppView and a PDS proxy `app.bsky.notification.*` the same
  way they proxy `getTimeline` is untested; the client sends it to the PDS it
  signed in to, as it does for the timeline.
