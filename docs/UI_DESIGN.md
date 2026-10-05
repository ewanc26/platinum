# Platinum UI Design

Platinum should look and behave like a native Classic Mac OS application first and a Bluesky client second. The UI should use Macintosh conventions wherever they remain useful instead of recreating a modern web or mobile social client.

The primary references are Apple's Mac OS 8 Human Interface Guidelines and period software such as Macstodon, Claris Emailer and iCab.

References:

- https://dev.os9.ca/techpubs/mac/HIGOS8Guide/thig-8.html
- https://dev.os9.ca/techpubs/mac/HIGuidelines/HIGuidelines-144.html
- https://dev.os9.ca/techpubs/mac/HIGuidelines/HIGuidelines-121.html
- https://dev.os9.ca/techpubs/mac/HIGOS8Guide/thig-38.html
- https://github.com/smallsco/macstodon

## Design goals

The interface should feel at home beside Finder, Claris applications and other Platinum-era software; remain usable at 640×480; work with a mouse or keyboard; keep network activity visibly separate from input and rendering; make selection and focus obvious; avoid requiring colour to communicate state; stay usable when the bridge is unavailable; make text the primary representation of a post; and avoid allocating large buffers or keeping unnecessary media in memory.

The application should not imitate a modern Bluesky website. Rounded cards, mobile-style bottom navigation, giant icon toolbars and web-style sidebars are explicitly out of scope.

## Main window

The main window is the centre of the application. Its title should describe the current surface, normally Platinum — Home or Platinum — Notifications.

The first usable layout is a compact navigation-and-timeline window:

    +---------------------------------------------------------------+
    | Platinum — Home                              [_] [zoom] [x]   |
    +---------------------------------------------------------------+
    | [Home v]                         [Refresh]    [Post…]          |
    +-------------+--------------------------------+----------------+
    | Home        |                                |                |
    | Notifications|        Timeline                |                |
    | Profile     |                                |                |
    |             |  post                           |                |
    |             |  post                           |                |
    |             |  post                           |                |
    |             |  post                           |                |
    |             |                                |                |
    +-------------+--------------------------------+----------------+
    | Selected post                                                  |
    | Author — timestamp                                             |
    | Post text …                                                   |
    | [Reply…] [Repost] [Like] [More…]                              |
    +---------------------------------------------------------------+

The left column is navigation, not a modern application sidebar. It should behave like a compact list box or folder list.

The timeline is the dominant content area. The lower pane is a selected-post inspector. Selecting a post changes the inspector; it does not navigate away from the timeline.

### Minimum and preferred sizes

The main window should be usable at a minimum of approximately 640×480. A larger default such as 800×600 is preferable when the display allows it.

Layout should calculate the timeline and detail regions from the current content rectangle rather than depending on one fixed resolution.

## Navigation

The first navigation list should contain Home, Notifications and Profile. Future entries may include Search, Bookmarks, custom feeds and Lists.

Navigation should use standard list behaviour: arrow keys move the selection; clicking selects; type selection may be supported; the selected row is visibly highlighted.

This deliberately uses the classic Mac list-box interaction model rather than large custom buttons.

## Timeline

The timeline should use a real scrolling list, not a collection of independently movable post windows.

Each post row should contain, in descending visual importance:

1. Author display name.
2. Handle.
3. Relative or compact timestamp.
4. Post text.
5. A compact metadata/action line.

The initial renderer should be text-first. Avatars and rich media can be added later without making them structural requirements for the row.

A selected row should be visually distinct. Do not rely solely on colour; use inversion, a frame or another standard selection treatment.

Long posts should wrap within the available timeline width. The application should avoid laying out text using assumptions tied to a single system font.

## Selected-post inspector

The bottom pane is deliberately smaller than the timeline. It should show the selected author's name and handle, the full post text when practical, reply/repost/like actions, and a More… control for secondary actions.

This is where richer interaction starts. A user can inspect a post without losing their place in the timeline.

The inspector may eventually show quoted posts, reply context, external links, content-warning state and media metadata. It should not become a second independent timeline.

## Toolbar and controls

The main window should use a compact control strip rather than a large modern toolbar.

Primary controls are a pop-up menu for the current feed, Refresh, and Post…

Standard controls should be used wherever practical. Apple's HIG specifies standard controls such as buttons, pop-up menus, list boxes, scrolling lists and text fields and describes their expected interaction and keyboard behaviour.

Do not replace every label with an icon. Text-labelled buttons are preferable when an icon has no universally obvious classic-Mac meaning.

## Menus

The menu bar is part of the application's navigation model, not decoration.

### File

- New Post…
- Close Window
- Quit

### Edit

Standard Undo, Cut, Copy, Paste and Select All commands as applicable, followed by Preferences… at the bottom.

### View

- Refresh
- Show Detail
- Load Older Posts

Load Older Posts fetches the next page of the timeline. Pressing Down Arrow on the last row does the same; the fetch is never started by scrolling, so it cannot begin in the middle of scroll-bar tracking. The list keeps at most forty posts: loading past that drops the newest ones from the top, and the selection stays on the post it was on.

### Post

- Like or Unlike (Command-L)
- Repost or Undo Repost (Command-E)

Both act on the selected timeline post and flip whatever the row currently shows. The detail pane spells the state out in words ("4 likes (you liked it)"), so it never depends on colour or a glyph. The bridge is idempotent, so a command sent twice does no harm.

### Window

- Timeline
- Notifications
- Profile
- Bring All to Front

### Help

- About Platinum
- Platinum Help

Help remains the final application menu.

Keyboard equivalents should be provided for frequently used commands, including Command-N, Command-R, Command-W and Command-Q.

## Compose window

Post… should open a separate modeless compose window rather than replacing the timeline.

    +-------------------------------------+
    | New Post                         x  |
    +-------------------------------------+
    |                                     |
    |             post text                |
    |                                     |
    |                                     |
    +-------------------------------------+
    | 0 / 300              [Cancel] [Post]|
    +-------------------------------------+

The text field should use standard TextEdit behaviour. The insertion point should be placed in the text field when the window opens. Keyboard navigation must work through controls in a predictable order.

Post submission should visibly enter a pending state. The user must not be left guessing whether the bridge is doing anything.

## Profile window

Profiles should use a conventional document/modeless window containing display name, handle, short biography, basic counts, primary action, and recent posts when available.

Do not reproduce a modern full-width profile webpage.

## Notifications window

Notifications should be a scrollable list using the same visual language as the timeline.

Notification rows can be more compact because their primary purpose is to communicate an event.

Examples include: Ewan liked your post. Aidan replied to your post.

Selecting a notification can populate the selected-item inspector or open the relevant profile/post.

## Preferences window

Preferences should be a conventional modeless dialog/window and should stay small.

Initial preferences should be Bridge URL, whether to restore the last window position, and a text-size choice if it can be supported without introducing application-specific typography complexity.

Do not create a large settings dashboard. Apple explicitly recommends avoiding preference panes for behaviours that users change frequently.

## Authentication and first run

The first-run experience should be a small sequence of classic dialogs rather than a custom welcome screen:

1. Bridge configuration window.
2. Connect… or Set Bridge… dialog.
3. Browser-assisted pairing instructions.
4. Pairing-code entry dialog.
5. Completion alert.
6. Main Timeline window.

The Mac client never asks for a Bluesky password.

The existing backend pairing model remains unchanged: OAuth happens in the modern browser/bridge, while Mac OS 9 receives only the revocable installation token and account metadata.

## Progress and errors

Network actions need explicit state.

Use a progress indicator for operations long enough to notice, short status text when appropriate, and standard alert boxes for failures that require user attention.

Error messages should use plain language such as Platinum could not connect to the bridge rather than exposing WF_ERR_NETWORK.

Technical status values remain internal.

Do not display OAuth credentials, bearer tokens, filesystem paths or stack traces.

## Typography

The UI should use the system font rather than bundling a custom font.

The Apple HIG notes that dialog and control layouts should be designed against Chicago's metrics so that they remain correct when the user's selected System font differs. Platinum should therefore keep its layout tolerant of the system font rather than assuming one specific face.

Use stronger text styles sparingly for window titles, author names and section headings. The post body is ordinary system text.

## Colour and appearance

Platinum Appearance remains the source of truth.

The application should work correctly with different Appearance settings rather than assuming a single screenshot-perfect palette. Apple specifically notes that appearance can change while layout must remain stable.

Do not hardcode a modern colour palette.

Do not use colour as the sole indicator of selected posts, unread notifications, errors or connection state.

## Window model

Use normal Macintosh windows and the Window menu to manage them.

The application should remember sensible positions and sizes where practical, because the Macintosh HIG treats window placement and direct manipulation as part of the user's control over the workspace.

The Timeline is the primary window. Profile, Notifications, Compose and Preferences are separate windows because they represent distinct tasks and fit naturally into the Classic Mac window model.

## Accessibility and keyboard use

Keyboard access is a first-class requirement.

At minimum: Tab navigates focusable dialog controls; arrow keys work in lists; Return activates the default button where appropriate; Escape cancels cancellable dialogs; Command-W closes a modeless window; Command-Q quits the application; Command-R refreshes the active data view; Command-N opens Compose.

Focus must be visible.

The UI must remain understandable without colour, without sound and without relying on tiny icons.

## Implementation order

The UI work should proceed in this order:

1. Menu bar and main window chrome.
2. Reusable geometry/layout module.
3. Standard list-box based navigation.
4. Timeline list renderer.
5. Selected-post inspector.
6. Compose window.
7. Notifications.
8. Profile.
9. Preferences.
10. Keyboard/focus polish.
11. Media and richer post interactions.

The first complete UI milestone should therefore be a convincing static shell populated by representative data before network-backed rendering is added.

## Non-goals

Do not add a web view, HTML/CSS rendering, a mobile-style tab bar, a modern card-based UI, rounded-corner application chrome, giant icon-only navigation, a permanently visible OAuth/browser panel, or a custom widget toolkit when the Macintosh Toolbox can provide the behaviour.

The native Notifications surface is a modeless document window backed by `GET /v1/notifications`. It supports bounded scrolling, Command-R refresh and standard window close/drag behaviour.


The native Preferences surface is a modeless document window showing the paired bridge URL, account DID and installation ID. Its Sign Out action calls the bridge revocation endpoint before clearing the local session; a local sign-out is still completed if remote revocation cannot be confirmed.


Pairing is a reusable modeless window rather than a first-run-only dialog. It opens automatically when no account is configured and can also be invoked from File > Pair Account or Account Preferences after sign-out. OAuth remains in the modern browser; the Classic Mac window only collects the bridge URL and six-character pairing code.


The Timeline and Notifications lists use native Control Manager scroll bars. The scroll thumb is part of the Classic Mac window chrome rather than a custom-drawn web-style widget and shares state with keyboard/page scrolling.
