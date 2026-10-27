# retro68-includes

Headers that exist in Apple's Universal Interfaces but not as separate files
in Retro68's Multiversal Interfaces, which instead declare those managers in
`Multiverse.h`. These wrappers give the sources the standard header names,
with `#include_next` falling back to the real headers when the build uses
Apple's Universal Interfaces.

Constants the Multiversal Interfaces do not define (`scrollBarProc`,
`kOnSystemDisk`, `kCreateFolder`) are defined here next to the manager they
belong to; the values match Apple's headers and the host stubs in
[`macos9/test/sdk-stubs/`](../test/sdk-stubs/).

Only the Retro68 build adds this directory to its include path; the host C89
check keeps using the sdk-stubs.
