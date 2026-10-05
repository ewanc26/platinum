# Parity with Cobalt and Indigo

Generated from [parity.tsv](parity.tsv) by `tools/check-parity.sh --write`. Do
not edit by hand: CI regenerates it and fails on a difference.

The Platinum column is checked against the code. An implemented row names a file
and a string that has to be in it. An open row links the issue that tracks it.
The Cobalt and Indigo columns are read from their READMEs and source trees on
2026-10-05 and are not checked by this repository's CI.

Mac OS 9 gaps are designed for the platform (bounded memory, cooperative event
loop, 640x480, no decoding on the Mac) rather than ported from the consoles.

| Feature | Cobalt | Indigo | Platinum |
| --- | --- | --- | --- |
| OAuth sign-in (browser, paired) | no | yes | implemented (`macos9/bridge_client.c`) |
| App-password sign-in (bridge endpoint) | yes | yes | implemented (`bridge/src/auth/app-password.ts`) |
| App-password sign-in (Mac window) | yes | yes | [#51](https://github.com/ewanc26/platinum/issues/51) |
| Session survives restart | yes | yes | implemented (`macos9/session.c`) |
| Sign out with server-side revocation | yes | yes | implemented (`macos9/session.c`) |
| Home timeline | yes | yes | implemented (`macos9/timeline.c`) |
| Timeline paging | yes | yes | implemented (`macos9/timeline.c`) |
| Notifications | yes | yes | implemented (`macos9/notifications_feed.c`) |
| Notifications paging | yes | yes | implemented (`macos9/notifications_feed.c`) |
| Mark notifications seen | yes | yes | implemented (`macos9/notifications_feed.c`) |
| Thread view | yes | yes | [#27](https://github.com/ewanc26/platinum/issues/27) |
| Post text | yes | yes | implemented (`macos9/application.c`) |
| Reply | yes | yes | [#27](https://github.com/ewanc26/platinum/issues/27) |
| Quote post | yes | yes | [#29](https://github.com/ewanc26/platinum/issues/29) |
| Reply gates | yes | no | [#29](https://github.com/ewanc26/platinum/issues/29) |
| Like and unlike | yes | yes | implemented (`macos9/timeline.c`) |
| Repost and undo | yes | yes | implemented (`bridge/src/domain/api.ts`) |
| Own profile | yes | yes | implemented (`macos9/profile.c`) |
| Other people's profiles | yes | yes | [#31](https://github.com/ewanc26/platinum/issues/31) |
| Follow and unfollow | yes | yes | [#31](https://github.com/ewanc26/platinum/issues/31) |
| Followers and following lists | yes | yes | [#31](https://github.com/ewanc26/platinum/issues/31) |
| Profile posts tab | yes | yes | [#31](https://github.com/ewanc26/platinum/issues/31) |
| Pinned post | yes | yes | [#31](https://github.com/ewanc26/platinum/issues/31) |
| Avatars | yes | yes | [#32](https://github.com/ewanc26/platinum/issues/32) |
| Post images | yes | yes | [#32](https://github.com/ewanc26/platinum/issues/32) |
| Image alt text | yes | yes | [#32](https://github.com/ewanc26/platinum/issues/32) |
| Full-size image viewer | yes | yes | [#32](https://github.com/ewanc26/platinum/issues/32) |
| Link-card previews | yes | yes | [#32](https://github.com/ewanc26/platinum/issues/32) |
| Attach images to a post | yes | yes | [#33](https://github.com/ewanc26/platinum/issues/33) |
| Actor search | yes | yes | [#34](https://github.com/ewanc26/platinum/issues/34) |
| Post search | yes | yes | [#34](https://github.com/ewanc26/platinum/issues/34) |
| Custom feeds | yes | yes | [#34](https://github.com/ewanc26/platinum/issues/34) |
| Lists | yes | yes | [#34](https://github.com/ewanc26/platinum/issues/34) |
| Mute and block | yes | yes | [#35](https://github.com/ewanc26/platinum/issues/35) |
| Muted words | no | yes | [#35](https://github.com/ewanc26/platinum/issues/35) |
| Who liked or reposted | yes | no | [#36](https://github.com/ewanc26/platinum/issues/36) |
| Diagnostics window | yes | no | [#37](https://github.com/ewanc26/platinum/issues/37) |
| Post drafts | no | yes | [#37](https://github.com/ewanc26/platinum/issues/37) |
| Auto-update (bridge, from GitHub releases) | no | no | implemented (`bridge/src/update/install.ts`) |
| Auto-update (client, bridge-served, user-confirmed) | no | no | [#48](https://github.com/ewanc26/platinum/issues/48) |
| Push notifications | no | no | not possible: No push service reaches a homebrew or OS 9 client; Cobalt README "Deliberately not planned". Platinum has no APNs-equivalent and the bridge cannot open a connection to OS 9. |

14 implemented, 26 tracked by an open issue, 1 not possible.
