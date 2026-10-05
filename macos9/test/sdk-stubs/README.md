# Classic Mac OS SDK stubs

These headers declare the Classic Mac OS SDK surface that Platinum's sources
use. They are **not** a Classic Mac SDK. They are declaration-only: no sizes, no
struct layout, no semantics, no implementation.

## Why they exist

A CI runner has no Classic Mac SDK, so the `Mac OS 9 sources (C89)` job could
only compile the handful of sources that include nothing from the SDK. That is
why it missed every defect in the UI half of the client: a header that does not
exist, `NewCWindow` called with the wrong number of arguments, `FSWrite` with
its arguments swapped. None of those can reach a compiler that never sees the
file.

With these stubs the job compiles **every** source under `macos9/`. That is the
point: a change that would not build for CodeWarrior fails CI instead of
reaching main.

## What a green run does and does not mean

A clean compile against these stubs means:

- every Mac source is strict C89, warning-free under `-Werror`;
- declarations are used consistently across translation units;
- no source reaches for a header the Mac target does not have.

It does **not** mean:

- the client builds with CodeWarrior or Retro68;
- the struct layouts and pointer sizes match the real 68k SDK — `Rect`, `RectPtr`
  and `Handle` here are nothing like the originals;
- anything links against real QuickDraw, Open Transport or macTLS;
- the client runs, on Classic Mac OS 9 or anywhere else.

Anything behavioural still needs the intended CodeWarrior-era toolchain and
real hardware. See AGENTS.md, "Mac OS 9 transport" and "Hardware validation".

## Rules for editing these

Signatures are taken from the real Classic Mac OS 9 SDK, deliberately, including
the awkward ones. If a source stops compiling because a call does not match the
real signature, **the source is wrong** — fix the source, not the stub.

Do not add a declaration just to make an error go away. A stub that lies is
worse than a missing stub, because it turns the check green without the code
being real. If the real SDK does not have a symbol, do not declare it.

Watch for name collisions with real SDK macros. `cmdKey`, `optionKey` and
`shiftKey` are genuine Classic Mac macros, so a stub parameter must not be
called `cmdKey` or the declaration silently becomes `short 0x0100`. That
happened once while writing these.
