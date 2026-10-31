# UI audit

Date: 2026-10-31. Source inspection only; nothing here was run on Mac OS 9.

The audit asks one question: what does each window do today, which helpers does
it share, and how does it say "empty", "loading", "failed" or "this needs a
session"? It is the basis for the shared state panel, not a redesign.

## Helpers every window uses

| Helper | Where | What it gives |
|---|---|---|
| `platinum_window_open` | `drawutil.c` | A visible `documentProc` window with a close box, the same colour and behind settings. |
| `platinum_draw_text` | `drawutil.c` | A C string at a fixed baseline, in the current port. |
| `platinum_draw_button` | `drawutil.c` | A framed button with a centred Pascal-string title. |
| `platinum_scrollbar_*` | `scrollbar.c` | A bound scrollbar for a row list. |
| `platinum_textfield_*` | `textfield.c` | TextEdit creation, editing and a length limit. |
| `platinum_screen_*_state` | `screen_state.c` | Whether a screen can be served by the native session, the bridge, or neither, and the sentence to show. |

There is no shared helper for line height, button metrics, panel layout or
status text. Each window chooses its own coordinates.

## Windows

| Window | Module | Data source | Empty / loading / error | Notes |
|---|---|---|---|---|
| Main window: toolbar, navigation, timeline, detail | `application.c`, `ui.c`, `timeline.c` | native `getTimeline`, bridge fallback | `timeline.status` drawn at a fixed offset; empty is "The timeline is empty."; failure keeps rows | 64-point row height, three toolbar buttons, fixed 82/78/62 widths |
| Thread / author list | `thread.c`, `thread_feed.c` | bridge `getPostThread`, `getAuthorFeed` | `thread.status` drawn at `(220, 20)`; loading, empty, "No posts to show." | Fixed row height 46, indentation by depth, close button bottom-right |
| Profile | `profile.c`, `profile_feed.c` | bridge | `profile.status` drawn at `(16, 104)`; loading, empty, state sentence | Fixed baselines 28..234, counts line, relationship in words, close bottom-right |
| People / followers / likes | `people.c`, `people_feed.c` | bridge | `people.status` at a fixed line; loading, empty | Row list, close bottom-right |
| Search accounts / posts / muted word | `search.c`, `search_query.c` | bridge | `search.status` at `(14, 100)`; no loading state | TextEdit, Cancel and Go/Add; Return runs; Command-W cancels |
| Conversations and messages | `conversations.c`, `conversations_feed.c` | bridge | `conversations.status`; loading list and loading messages are distinct | Two panes, scrollbar, composer |
| Compose | `compose.c`, `compose_model.c` | bridge | `compose.status`; character limit | TextEdit, Post/Cancel |
| Notifications | `notifications.c`, `notifications_feed.c` | native `listNotifications` | `notifications.status`; "No notifications loaded." then "No notifications." | 20-row pages, 40-row cap |
| Preferences | `preferences.c` | local config plus session | `preferences.status` | Account, server, entryway, sign out, pair |
| Connection Status | `diagwin.c`, `diag.c` | bridge | No list; status model | Text lines only |
| Pairing | `pairing.c` | bridge pairing | `pairing.status` | Pairing code field |
| App password sign-in | `apppw.c`, `apppw_input.c` | native bootstrap | `apppw.status` | Handle and password fields, Sign In/Cancel |
| Muted words | `search.c` (`PLATINUM_SEARCH_WORD`) plus `people_feed.c` | bridge | "No muted words." | Add only; removal is a list action elsewhere |

## What is duplicated

1. **The close-button rectangle.** Thread, profile, people, conversations and
   others each rebuild `portRect` minus 78/10/34/10 inline. A change to the
   inset has to be made in every window.
2. **The status line.** Every feed and window has its own `status` buffer, its
   own `strncpy`-with-terminator setter, and its own fixed baseline. Wording is
   not consistent: "No notifications loaded." versus "No notifications.",
   "The timeline is empty." versus "No posts to show.".
3. **The draw/erase/port boilerplate.** Every `*_draw` does `GetPort`, `SetPort`,
   `EraseRect`, draw, `SetPort(old)`.
4. **Row drawing.** Thread and timeline both compute a visible count from a
   hard-coded row height and clip by index.
5. **State handling.** `screen_state.c` is consulted in `application_views.c`
   for the bridge notice, and in `application_post.c` for like/repost, but
   windows only receive the resulting string. A window cannot tell a state
   message from an ordinary error, so it cannot draw it differently.

## Gaps the panel work addresses

- No framed empty state, so a short sentence floats in an otherwise blank
  window and reads like a failure.
- No action on the state: the sentence names `File > Sign In` or
  `File > Pair Account`, but nothing in the window is clickable.
- No loading indication beyond text; no disabled controls.
- No distinction between an empty account, a failed request and a screen that
  has not migrated, other than wording.
- No shared line metrics, so none of the windows survive a different system
  font size without clipping.

## Deliberately unchanged

- The window chrome, the three-pane main window, row heights and the
  bottom-right button placement. This is the existing look; the work is to make
  states consistent inside it.
- Text-only relationship and state wording. No colour-only meaning was found,
  and none should be introduced.
- `screen_state.c` remains the single decision about which session can serve a
  screen; the panel only gives its sentence a shape.

## Verification limits

- No Mac OS 9 run, no QEMU run and no local Retro68 build here. The layout
  arithmetic is host-tested; the drawing calls are declaration-stub checked.
- There is no screenshot or snapshot harness in the repository. None was
  invented for this audit; the gap is recorded here and in the migration doc.
- Keyboard focus and default/cancel button behaviour were read from the event
  handlers, not exercised on a Mac.
