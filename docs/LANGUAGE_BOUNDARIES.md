# Language boundaries

Platinum contains two deliberately different runtime environments:

- `macos9/` is the native Classic Mac OS 9 application. It must remain strict C89 and must continue to pass the existing C89 CI compile against the SDK stubs. Do not add C++ sources to this target or weaken the dialect checks.
- `bridge/` is the modern Node.js/TypeScript service. Modern protocol, OAuth, and TLS work belongs here when it belongs on the bridge side of the architecture.

C++ support elsewhere in Ewan's repositories does not make C++ suitable for the Mac OS 9 application. If a future host-only C++ tool becomes justified, it must have its own build target outside `macos9/` and must not be linked into or included in the C89 source sweep. Until such a tool exists, Platinum has no C++ compilation target.

This is an intentional platform boundary, not a general prohibition on C++ across the wider project stack.