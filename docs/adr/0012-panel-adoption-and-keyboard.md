# 0012: Panel adoption and keyboard conventions

- Status: accepted
- Date: 2026-10-31
- Applies to: every Mac OS 9 window in `macos9/`

## Context

`macos9/state_panel.c` gives the sentences from `screen_state.c` a framed
shape: a title, a wrapped message and at most one action button. Profile used
it first. The other windows still put a bare sentence at a fixed point, often
near the top, and offered no button for the thing the sentence told the reader
to do.

Adopting the panel window by window raised four questions that should not be
answered differently in each window:

1. Who decides that a panel is shown, and who may show a transient message?
2. What happens to the window behind a panel action?
3. What happens when a request fails but rows are already on screen?
4. Which key does what, and in what order does Tab move?

Apple's Human Interface Guidelines describe Return as the default button's key
and Escape (and Command-period) as Cancel. See
[Buttons](https://developer.apple.com/design/human-interface-guidelines/buttons)
and
[Keyboards](https://developer.apple.com/design/human-interface-guidelines/keyboards).
The conventions below apply that guidance to the existing Platinum windows
without redesigning their chrome.

## Decision

1. **A window asks the shared panel directly.** `screen_state.c` remains the
   single decision about whether a screen is served by the native session, the
   bridge, or neither. The adopting window passes that sentence, its own
   loading flag and whether it has content to `platinum_panel_for_screen`, and
   draws the result. `set_status` is for transient messages only: it never
   invents a blocking state and never becomes a second place where session
   state is decided.

2. **A panel action never closes the window that shows it.** Sign In opens the
   app-password window; Pair a Bridge opens the pairing window. The current
   window stays where it is, with its panel, so the reader can come back to it
   after signing in or pairing. Cancel, Close and Command-W still close the
   window they belong to.

3. **Failure keeps content.** When a window already has rows, a failed refresh
   or page keeps those rows and puts one status line at the bottom of the
   window. The panel is only for a window with nothing to show: no rows yet,
   nothing loaded, a blocking session state, or an empty account. This applies
   to the timeline, notifications, search results, threads and feeds. A window
   that has content never swaps it for a panel just because a later request
   failed.

4. **Every window follows the same keys.** Return (and Enter) activates the
   default or primary action. A panel with one action makes that action the
   default. Escape and Command-period activate Cancel or Close. Tab moves focus
   in drawing order; Shift-Tab moves it back where the window has more than one
   focusable control. The existing arrow-key, Page Up/Page Down and Command-key
   shortcuts are unchanged.

The shared helpers in `state_panel.h` implement the parts that must not drift:
`platinum_panel_for_screen` for the content-versus-panel rule,
`platinum_panel_action_at` for hit-testing the same rectangle that was drawn,
and `platinum_panel_key_is_return` / `platinum_panel_key_is_cancel` for the key
mapping. Wrapping uses `platinum_panel_columns`, so a narrow window wraps inside
its panel instead of at the old fixed 48 columns.

## Consequences

- A window with stale content now keeps it and says in one line that the
  refresh failed, rather than replacing the content with a panel. The reader
  does not lose their place.
- A blocking panel can stay on screen while another window is opened on top of
  it. That is intended; the window is not a modal dialog.
- The panel's action button is the only action in the panel. Windows may still
  have their own buttons (Close, Send, Older, Check Again), and those keep their
  existing positions.
- Adding a window means adding one call to `platinum_panel_for_screen` and one
  action hit-test; it does not mean copying the panel geometry.
- `screen_state.c` stays small and host-tested. The panel module owns wording
  for ordinary empty/loading/failure screens, and the window owns only its
  content and its buttons.

## Alternatives considered

- **A panel per window with its own geometry.** Rejected: the audit found five
  copies of the close-button rectangle and several different status baselines.
  A sixth copy would make the drift worse.
- **Closing the window when Sign In or Pair is chosen.** Rejected: the reader
  would lose the screen they were trying to use, and after signing in they
  would have to find it again.
- **Replacing content with a panel on failure.** Rejected: it hides rows that
  are still valid and moves the reader's selection out from under them.
- **A modal dialog for blocking states.** Rejected: it changes the window model,
  blocks the rest of the application, and does not match the existing
  non-modal windows.

## Verification

The classification, wrapping, layout, action hit-testing and key mapping are
host-tested in `macos9/test/test_state_panel.c`. The per-window drawing and
event handling compile as strict C89 and are exercised through the window
models where they are host-testable. None of this has been run on Mac OS 9;
there is no screenshot harness, so the visual result and the actual Tab order
on a real Mac remain unverified.
