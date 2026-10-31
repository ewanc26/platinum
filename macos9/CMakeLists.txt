cmake_minimum_required(VERSION 3.16)
project(Platinum C)

# Classic Mac OS application build, for the Retro68 cross-compiler only.
#
# Retro68 (https://github.com/autc04/Retro68) supplies the toolchain files and
# the add_application() command used below. With the prebuilt image
# ghcr.io/autc04/retro68:
#
#   docker run --rm -v "$PWD":/root -v /path/to/wolfram:/root/wolfram \
#     -i ghcr.io/autc04/retro68 tools/build-macos9.sh
#
# or with a locally built toolchain, see tools/build-macos9.sh.
#
# The result is a real Mac application (type APPL, creator PTLM) that runs on
# 68k and PowerPC Macs from System 7.1 through Mac OS 9.2.2. Compiling here
# proves the sources build against real Classic Mac OS interfaces; it does
# not prove the application runs, which needs an emulator or hardware.
#
# Host-side C89 checks do not use this file: CI compiles every source
# directly with clang and gcc against the declaration-only stubs in
# macos9/test/sdk-stubs/ (see .github/workflows/ci.yml).

if(NOT CMAKE_SYSTEM_NAME MATCHES "Retro")
    message(FATAL_ERROR
        "Platinum's macos9 CMake build targets Retro68, but CMAKE_SYSTEM_NAME "
        "is '${CMAKE_SYSTEM_NAME}'. Point CMAKE_TOOLCHAIN_FILE at a Retro68 "
        "toolchain file (retro68.toolchain.cmake or retroppc.toolchain.cmake).")
endif()

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_EXTENSIONS OFF)

# Same dialect rules as the CI C89 job. -Wno-multichar: 'PTLM' and 'PREF' are
# OSType literals, multi-character constants by design on the Mac target.
add_compile_options(
    -Wall -Wextra -Werror
    -Wno-unused-parameter -Wno-multichar
    -Wdeclaration-after-statement -Wstrict-prototypes -Wvla
)

set(PLATINUM_SOURCES
    application.c
    atproto_bootstrap.c
    atproto_client.c
    atproto_feed.c
    atproto_identity.c
    atproto_session.c
    atproto_url.c
        avatar_cache.c
    application_auth.c
    application_menus.c
    application_post.c
    application_views.c
    apppw.c
    apppw_input.c
    bridge_client.c
    compose.c
    compose_model.c
    conversations.c
    conversations_feed.c
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
    muted_words.c
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
    screen_state.c
    state_panel.c
    search.c
    search_query.c
    secret.c
    session.c
    text_codec.c
    textfield.c
    thread.c
    thread_feed.c
    timeline.c
    timeline_native.c
    ui.c
)

# Wolfram's public headers (wolfram/xrpc.h, wolfram/macos9_tls.h) are needed
# at compile time; the transport itself is a separate build.
set(WOLFRAM_INCLUDE_DIR "" CACHE PATH "Wolfram checkout's include/ directory")
if(NOT WOLFRAM_INCLUDE_DIR)
    message(FATAL_ERROR
        "Set WOLFRAM_INCLUDE_DIR to a Wolfram checkout's include/ directory.")
endif()

add_application(Platinum
    TYPE "APPL"
    CREATOR "PTLM"
    ${PLATINUM_SOURCES}
    resources/icon.r
)

target_include_directories(Platinum PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/retro68-includes
    ${WOLFRAM_INCLUDE_DIR}
)

# The real transport is Wolfram's, built separately against Open Transport
# and macTLS (AGENTS.md, "Build and validation"). CI has no Open Transport
# headers (#41), so it links the test's transport stub instead: the same
# wf_* entry points with no network behind them. Such a binary builds,
# links and carries its resources; it cannot reach the bridge, so it must
# never be shipped.
#
# For a real build, point -DPLATINUM_TRANSPORT_LIBS at the transport and
# macTLS libraries built with the Retro68 toolchain, and set
# PLATINUM_LINK_TRANSPORT_STUB=OFF.
set(PLATINUM_TRANSPORT_LIBS ""
    CACHE STRING "Wolfram Mac OS 9 transport libraries, real build only")
set(PLATINUM_LINK_TRANSPORT_STUB ON
    CACHE BOOL "Link the test transport stub (CI); builds, does not network")

if(PLATINUM_LINK_TRANSPORT_STUB AND PLATINUM_TRANSPORT_LIBS)
    message(FATAL_ERROR
        "PLATINUM_LINK_TRANSPORT_STUB and PLATINUM_TRANSPORT_LIBS are "
        "mutually exclusive: choose the stub or the real transport.")
endif()

if(PLATINUM_LINK_TRANSPORT_STUB)
    target_sources(Platinum PRIVATE test/wolfram_stub.c)
elseif("" STREQUAL "${PLATINUM_TRANSPORT_LIBS}")
    message(FATAL_ERROR
        "Without PLATINUM_LINK_TRANSPORT_STUB, set PLATINUM_TRANSPORT_LIBS "
        "to the Wolfram Mac OS 9 transport and macTLS libraries built with "
        "the Retro68 toolchain.")
else()
    target_link_libraries(Platinum ${PLATINUM_TRANSPORT_LIBS})
endif()

# The deterministic fixture application for the headless runtime layer. Off by
# default and never part of a release: it draws compiled-in fixture data through
# the production ui.c and links the transport stub, so it has no token, no
# preferences and no network. See docs/TESTING_MACOS9.md.
option(PLATINUM_BUILD_FIXTURE "Build the deterministic Mac OS 9 UI fixture" OFF)
if(PLATINUM_BUILD_FIXTURE)
    add_application(PlatinumFixture
        TYPE "APPL"
        CREATOR "PTLF"
        fixture.c
        ui.c
        timeline.c
        post_media.c
        text_codec.c
        bridge_client.c
        json_min.c
        drawutil.c
        state_panel.c
        screen_state.c
        test/wolfram_stub.c
        resources/icon.r
    )
    target_include_directories(PlatinumFixture PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/retro68-includes
        ${WOLFRAM_INCLUDE_DIR}
    )
endif()
