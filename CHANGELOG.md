# Changelog

Release tags are `vX.Y.Z` and must match `bridge/package.json`; the
`release-check` workflow refuses a tag that does not, or that has no section
here. Unreleased entries are grouped as Added, Changed, Fixed, Removed and
Security, plus Repository for changes only contributors see, and each links its
pull request.

## [Unreleased]

Releases are paused until I've run Platinum on Mac OS 9 ([#62](https://github.com/ewanc26/platinum/issues/62)). Every pull request that changes something you'd notice adds a line here.

### Added

- Search Accounts and Search Posts (View menu; Command-F searches posts). Results open in the People window or the posts list, forty at a time. [#86](https://github.com/ewanc26/platinum/pull/86)
- Show Author's Posts, Saved Feeds and My Lists: an author's own posts, your saved feeds and your lists, all clickable and paged, with accounts in any list opening their profile. [#81](https://github.com/ewanc26/platinum/pull/81)
- The bridge can search accounts and posts, list your saved feeds and lists, and return a feed's posts or a list's members. The Mac side isn't wired up yet. [#80](https://github.com/ewanc26/platinum/pull/80)
- Who Liked This, Who Reposted This, Show Followers and Show Following (Post menu) open a People window listing the accounts, forty at a time. The bridge can list who liked or reposted a post. [#79](https://github.com/ewanc26/platinum/pull/79)
- Show Author's Profile (Post menu, Command-I) opens someone else's profile with their counts, whether you follow each other and their pinned post, and Follow or Unfollow (Command-Y) acts on it. [#78](https://github.com/ewanc26/platinum/pull/78)
- Show Thread (View menu, Command-T) opens the conversation around the selected post: what it replied to above, and the replies below, indented. I keep forty posts at most and say when some are left out. [#77](https://github.com/ewanc26/platinum/pull/77)
- The bridge can show anyone's profile (with whether you follow them, and their pinned post), list who someone follows and who follows them, return their own posts, and follow or unfollow, all idempotently. The Mac side isn't wired up yet. [#73](https://github.com/ewanc26/platinum/pull/74)
- Reply to the selected post from the Post menu (Command-J). The compose window says who you're replying to. [#71](https://github.com/ewanc26/platinum/pull/72)
- The bridge can return a post's thread as one flat list and post a reply to a specific post, working out the thread root itself. The Mac side isn't wired up yet. [#70](https://github.com/ewanc26/platinum/pull/70)
- The bridge can update itself from this repository's releases, when you ask it to: it checks the SHA-256 first, keeps the previous version for rollback, and never restarts itself. [#50](https://github.com/ewanc26/platinum/pull/50)
- You can sign in to the bridge with an app password instead of the browser, if the operator turns it on. Off by default, and the password is never stored or logged. [#55](https://github.com/ewanc26/platinum/pull/55)
- The timeline pages back: View > Load Older Posts, or Down Arrow on the last post. I keep forty posts at most. [#59](https://github.com/ewanc26/platinum/pull/59)
- Like, unlike, repost and undo a repost from the new Post menu (Command-L, Command-E). The bridge side is idempotent, so pressing twice does no harm. [#60](https://github.com/ewanc26/platinum/pull/60), [#61](https://github.com/ewanc26/platinum/pull/61)
- Opening Notifications marks them seen, up to the newest one shown, and the list pages back the same way the timeline does. [#64](https://github.com/ewanc26/platinum/pull/64)
- A logo, and a Mac icon family generated from the same shape (not yet built into an application). [#57](https://github.com/ewanc26/platinum/pull/57)

### Changed

- The Mac client no longer depends on cJSON; it reads the bridge's JSON with a small bounded C89 reader, and every Mac source compiles as strict C89 in CI. [#21](https://github.com/ewanc26/platinum/pull/21), [#23](https://github.com/ewanc26/platinum/pull/23)
- `/health` reports the bridge's real version instead of a stale one. [#50](https://github.com/ewanc26/platinum/pull/50)
- The README follows the layout the rest of the stack uses. [#57](https://github.com/ewanc26/platinum/pull/57)

### Fixed

- The compose and pairing windows now call TextEdit the way the Toolbox defines it. They had been written against calls that don't exist (a nine-argument `TENew`, a `TEKey` that took a key map), so the 300-character and six-character limits couldn't have worked and the text read back included whatever sat after it in the handle. The limits are now enforced when you type. [#85](https://github.com/ewanc26/platinum/pull/85)
- Saving and loading the preferences file passed `FSWrite` and `FSRead` the buffer and the byte count the wrong way round, which on a real Mac would have used the count as the buffer. The Control Manager calls use the real names, and the stand-in constants for window parts, permissions and folder types now match the real ones. [#84](https://github.com/ewanc26/platinum/pull/84)
- Windows hilite and dim properly when they gain and lose focus, text fields stop blinking in background windows, and clicking a background window brings it forward instead of pressing whatever was under the pointer. [#53](https://github.com/ewanc26/platinum/pull/53)
- The application state no longer lives on the stack, which on a Classic Mac is small and fixed at launch. [#59](https://github.com/ewanc26/platinum/pull/59)

### Repository

- A check that the Mac SDK stand-ins match the real Toolbox signatures (from the Multiversal Interfaces), with the 60-odd that don't listed and tracked in #82. [#83](https://github.com/ewanc26/platinum/pull/83)
- A Node port of Wolfram's OAuth pairing contract, verified against Wolfram's shared vectors. It isn't used yet; wiring the bridge to a Wolfram OAuth node is tracked in #75. [#76](https://github.com/ewanc26/platinum/pull/76)
- Pull requests are checked by Wolfram's shared flow workflow (conventions, house style, drift), and the PR template and the agent flow rules are Wolfram's, byte for byte. The README's licence badge says licence. [#69](https://github.com/ewanc26/platinum/pull/69)
- The bridge's updater runs Wolfram's own version, checksum and manifest test vectors, and follows Wolfram's manifest rules exactly: notes are optional, upper-case hashes are accepted, and versions may carry build metadata. [#68](https://github.com/ewanc26/platinum/pull/68)
- The contribution flow is enforced in CI: conventional commits, a single `CI gate`, rebase-only merges, and a check that every Mac source is compiled. [#24](https://github.com/ewanc26/platinum/pull/24), [#54](https://github.com/ewanc26/platinum/pull/54)
- A parity table against Cobalt and Indigo, built from the code and checked in CI. [#44](https://github.com/ewanc26/platinum/pull/44)
- A guard that stops logic Wolfram owns being copied into the bridge or the Mac client. [#58](https://github.com/ewanc26/platinum/pull/58)
- A release dry run on every pull request, with real releases blocked while they're paused. [#63](https://github.com/ewanc26/platinum/pull/63)
- Labels and repository metadata as code, checked against Wolfram's. [#65](https://github.com/ewanc26/platinum/pull/65)

## [0.3.1]

The version `bridge/package.json` already carried when releases were first
gated. Earlier history is in git.
