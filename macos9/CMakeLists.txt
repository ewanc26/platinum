cmake_minimum_required(VERSION 3.16)
project(Platinum C)

# Classic Mac OS 9 application build for Retro68.
#
# Retro68 is a GCC-based cross-compiler for Classic Mac OS (68k and PowerPC).
# Install it from https://github.com/autc04/Retro68, then:
#
#   # 68k build (runs on any 68k Mac, System 7.1 through Mac OS 9.2.2):
#   cmake -S macos9 -B build-macos9-68k \
#     -DCMAKE_TOOLCHAIN_FILE=<retro68>/share/retro68/cmake/Toolchain-retro68-68k.cmake
#   cmake --build build-macos9-68k
#
#   # PowerPC build (runs on PowerPC Macs, Mac OS 7.1.2 through Mac OS 9.2.2):
#   cmake -S macos9 -B build-macos9-ppc \
#     -DCMAKE_TOOLCHAIN_FILE=<retro68>/share/retro68/cmake/Toolchain-retro68-powerpc.cmake
#   cmake --build build-macos9-ppc
#
# Produces a Mac OS 9 application file (type APPL, creator PTLM) in the build
# directory. Copy it to a Mac volume (real hardware, BasiliskII, SheepShaver or
# Mini vMac) and double-click it.

set(PLATINUM_SOURCES
    application.c
    application_auth.c
    application_menus.c
    application_post.c
    application_views.c
    bridge_client.c
    compose.c
    compose_model.c
    config.c
    diag.c
    diagwin.c
    draft.c
    drawutil.c
    image_blob.c
    image_encode.c
    image_pix.c
    json_min.c
    main.c
    notifications.c
    notifications_feed.c
    pairing.c
    people.c
    people_feed.c
    post_media.c
    preferences.c
    prefs_file.c
    profile.c
    profile_feed.c
    scrollbar.c
    secret.c
    search.c
    search_query.c
    session.c
    text_codec.c
    textfield.c
    thread.c
    thread_feed.c
    timeline.c
    ui.c
)

set(PLATINUM_HEADERS
    application.h
    application_internal.h
    bridge_client.h
    compose.h
    compose_model.h
    config.h
    diag.h
    diagwin.h
    draft.h
    drawutil.h
    image_blob.h
    image_encode.h
    image_pix.h
    json_min.h
    pairing.h
    people.h
    post_media.h
    preferences.h
    prefs_file.h
    profile.h
    scrollbar.h
    secret.h
    search.h
    session.h
    text_codec.h
    textfield.h
    thread.h
    timeline.h
    ui.h
)

set(PLATINUM_RESOURCES
    resources/icon.r
)

# Strict C89 for the CodeWarrior-era target. Retro68's toolchain sets the
# cross-compiler and Mac OS system root.
set(CMAKE_C_STANDARD 89)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Retro68")
    message(WARNING
        "Platinum's Mac OS 9 application build targets Retro68. The detected "
        "compiler is ${CMAKE_C_COMPILER_ID}. Set CMAKE_TOOLCHAIN_FILE to a "
        "Retro68 toolchain file for a real Classic Mac OS build. Without "
        "Retro68, this CMakeLists.txt still checks that every source compiles "
        "as strict C89.")
endif()

# Warning flags mirror the host C89 CI job: every source must be clean under
# -Werror, with no C99 constructs or POSIX conveniences.
add_compile_options(
    -Wall -Wextra -Werror
    -Wdeclaration-after-statement -Wstrict-prototypes -Wvla
)

add_executable(Platinum
    ${PLATINUM_SOURCES}
    ${PLATINUM_HEADERS}
    ${PLATINUM_RESOURCES}
)

target_include_directories(Platinum PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Wolfram public headers (xrpc.h, macos9_tls.h, update.h) are needed at
# compile time. The Wolfram checkout is expected at a sibling path or passed
# via -DWOLFRAM_INCLUDE_DIR.
if(WOLFRAM_INCLUDE_DIR)
    target_include_directories(Platinum PRIVATE ${WOLFRAM_INCLUDE_DIR})
endif()

# Retro68 embeds the resource fork (icon.r: icon family, 'BNDL', 'FREF',
# signature 'PTLM', file type 'APPL') into the application file at link time.
# The SIZE resource (32-bit clean, multiFinder aware) comes from the link step.

# Tests are not built for the Mac target. They run on a modern host against
# the SDK stubs in macos9/test/sdk-stubs/ instead. See ci.yml.
