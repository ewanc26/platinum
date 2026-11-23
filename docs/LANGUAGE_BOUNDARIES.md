# Language boundaries

Platinum contains two deliberately different runtime environments:

- `macos9/` is the native Classic Mac OS 9 application. It must remain strict C89 and must continue to pass the existing C89 CI compile against the SDK stubs. Do not add C++ sources to this target or weaken the dialect checks.
- `bridge/` is the modern Node.js/TypeScript service. It is transitional: protocol work for a screen that has not migrated belongs here, but new native AT Protocol work belongs in `macos9/atproto_*.c` as strict C89, and no feature is added to the bridge alone. See `docs/architecture/native-client-migration.md`.

C++ support elsewhere in Ewan's repositories does not make C++ suitable for the Mac OS 9 application. If a future host-only C++ tool becomes justified, it must have its own build target outside `macos9/` and must not be linked into or included in the C89 source sweep. Until such a tool exists, Platinum has no C++ compilation target.

This is an intentional platform boundary, not a general prohibition on C++ across the wider project stack.

## Text and Unicode on the Mac side

The wire is UTF-8 end to end. The bridge protocol carries UTF-8, the native XRPC calls carry UTF-8, and a composed post is converted from the MacRoman TextEdit buffer to UTF-8 before it is sent. Conversion is a Mac-side display concern only; nothing upstream stores MacRoman.

Mac OS 9 draws through the Toolbox text stack, which renders the installed script rather than arbitrary Unicode. `macos9/text_codec.c` therefore converts UTF-8 to MacRoman for drawing, and a run of code points MacRoman cannot represent collapses to one deliberate placeholder, `PLATINUM_TEXT_PLACEHOLDER` (`?`). Emoji (including zero-width-joiner sequences, skin-tone modifiers and flags) and scripts the installed fonts do not carry, such as Japanese, draw as that single placeholder: not as nothing, and not as one question mark per code point. A representable character ends the run, so `😀?😀` keeps its literal question mark. The `lossy` count reports how many code points were replaced.

The richer route is deliberately not implemented yet. The Text Encoding Conversion Manager could convert into an installed script where one exists (Shift-JIS for Japanese, for example), and a bundled Unicode bitmap font could cover what remains. Both are larger decisions than the placeholder: a TEC conversion needs the Toolbox at runtime, so the host tests here cannot exercise it, and a bundled font adds size and a licence obligation to every release. Until that work is done, the placeholder is the recorded route, and `macos9/test/test_text_codec.c` is what holds it in place.
