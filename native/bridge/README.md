# Native bridge (C)

The C side of the native bridge described in `docs/NATIVE_BRIDGE.md`. Each part is
one module with one header, one `.c` file, and one host test file. Every file is
C89 and must build with:

    cc -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -c <file>

Host tests are built and run with `make -C native/bridge test`.
Nothing here is linked into the Mac build yet.
