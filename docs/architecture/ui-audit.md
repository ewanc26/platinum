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

## Windows after panel adoption

The rule is the same in every row: a window with content keeps it and shows one
status line at the bottom; a window with nothing to show asks
`platinum_panel_for_screen` and draws the shared panel. See ADR 0012.

| Window | Module | Data source | Empty / loading / error | Keys | Notes |
|---|---|---|---|---|---|
| Main window: toolbar, navigation, timeline, detail | `application.c`, `ui.c`, `timeline.c` | native `getTimeline`, bridge fallback | Panel in the timeline when there are no rows: sign-in when unpaired, loading, empty, or failed. Rows stay on a failed refresh and the status sits at the bottom of the timeline. | Return on a panel acts on it; otherwise Return refreshes. Tab moves through Refresh and Post. | 64-point row height, three toolbar buttons, fixed 82/78/62 widths; 40-row cap notice |
| Thread / author list | `thread.c`, `thread_feed.c` | bridge `getPostThread`, `getAuthorFeed` | Panel when there is no post: sign-in, bridge, loading, empty or failed. Loaded posts keep a bottom status. | Return on the panel; Escape and Command-period close; Tab moves through Close and the panel action. | Fixed row height 46, indentation by depth, close button bottom-right |
| Profile | `profile.c`, `profile_feed.c` | bridge | Panel for sign-in/bridge when there is no profile; a loaded profile keeps a bottom status. | Return on the panel; Escape and Command-period close; Tab moves through Close and the panel action. | Fixed baselines 28..234, counts line, relationship in words, close bottom-right |
| People / followers / likes | `people.c`, `people_feed.c` | bridge | Panel when the list is empty: sign-in, bridge, loading, empty or failed. Loaded rows keep a bottom status. | Return on the panel; Escape and Command-period close; Tab moves through Close and the panel action. | Row list, close bottom-right |
| Search accounts / posts / muted word | `search.c`, `search_query.c` | bridge | A blocking sign-in or bridge notice replaces the field with the panel. Ordinary failures keep the field and show a bottom status. | Return runs the default (or the panel action); Escape and Command-period cancel; Tab moves field, Cancel, Go/Add. | TextEdit, Cancel and Go/Add; Return runs |
| Conversations and messages | `conversations.c`, `conversations_feed.c` | bridge | A blocking notice or an empty conversation list shows the panel over the window. A loaded list with a failed refresh keeps its rows and composer, with a bottom status. | Return sends when the composer is focused, otherwise opens the selection or acts on the panel; Escape and Command-period close; Tab moves list, composer, Older, Send, Close. | Two panes, scrollbar, composer |
| Compose | `compose.c`, `compose_model.c` | bridge | `compose.status` is already a single transient line; no blocking panel. | Return posts; Escape and Command-period cancel. | TextEdit, Post/Cancel |
| Notifications | `notifications.c`, `notifications_feed.c` | native `listNotifications` | Panel when there are no rows: sign-in, loading, empty or failed. Rows stay on a failed refresh and the status sits at the bottom. | Return on the panel; Escape and Command-period close; Tab moves through Close and the panel action. | 20-row pages, 40-row cap notice |
| Preferences | `preferences.c` | local config plus session | `preferences.status` | Existing buttons; Escape and Command-period close. | Account, server, entryway, sign out, pair |
| Connection Status | `diagwin.c`, `diag.c` | bridge | Panel when no bridge is configured, with Pair a Bridge or Sign In. The diagnostics lines show once the screen can run. | Return on the panel; Escape and Command-period close; Tab moves through Close, Check Again and the panel action. | Text lines only |
| Pairing | `pairing.c` | bridge pairing | `pairing.status` | Existing Pair/Cancel behaviour. | Pairing code field |
| App password sign-in | `apppw.c`, `apppw_input.c` | native bootstrap | A loading panel while the account is resolved; failures keep the fields and show a bottom status; success becomes a signed-in view with the resolved entryway. | Return signs in (or activates the focused button); Escape and Command-period cancel; Tab moves handle, password, Cancel, Sign In. | Handle and password fields, Sign In/Cancel |
| Muted words | `search.c` (`PLATINUM_SEARCH_WORD`) plus `people_feed.c` | bridge | "No muted words." becomes the empty panel; a blocking notice replaces the field. | As Search. | Add only; removal is a list action elsewhere |

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

## What the adoption changed

- Thread, People, Conversations, Search, Notifications, the main timeline and
  Connection Status now call the shared panel instead of drawing a bare state
  sentence. Profile keeps its earlier adoption.
- A failure with rows no longer takes the rows away. The status moves to the
  bottom of the window, and the panel is used only when there is nothing to
  show.
- The panel's action button is hit-tested with the same wrapping and rectangle
  arithmetic that drew it, through `platinum_panel_action_at`.
- The timeline and notifications say when the 40-row cap is in effect, rather
  than silently dropping the oldest rows.
- Return, Escape, Command-period and Tab follow ADR 0012 in the adopted
  windows. The sign-in window now has a focus order that includes both buttons.
- Narrow windows wrap the panel message inside the panel; the old fixed 48
  columns could draw past the frame.

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
