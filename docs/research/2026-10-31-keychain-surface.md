# The Keychain surface in the pinned Retro68 image

Date: 2026-10-31.

## What was checked

The image CI builds the Mac target with:

```
ghcr.io/autc04/retro68@sha256:2c2ec5ca764c98b7460522be7327426e3b4d039a317c22d6ccdcc64e5247f21a
```

Pulled on this machine and inspected with:

```sh
docker run --rm --entrypoint /bin/sh \
  ghcr.io/autc04/retro68@sha256:2c2ec5ca... \
  -c 'find / -iname "Keychain*"; find / -name "CarbonLib*"'
```

The image carries the Retro68 toolchain (`/Retro68/bin`, `/Retro68/toolchain`)
and the Universal Interfaces. It does **not** carry `Keychain.h` under any
name, and it does **not** carry `CarbonLib` or `KeychainLib` stubs. Platinum's
own `macos9/retro68-includes/` holds only `Controls.h`, `Folders.h` and
`Script.h`, so nothing local supplies the missing header either.

## What this means for ADR 0007

The Carbon Keychain Manager may well exist on Mac OS 9.2.2 — that is not what
this check tested — but the **build** cannot see it: there is no declaration to
compile against and no library to link. The current PowerPC build would fail at
compile or link time, and CI would catch it on the next run.

So ADR 0007's implementation status is unchanged for a factual reason, not a
scheduling one: the pinned toolchain ships no Keychain surface.

## Smallest shim that would not invent an untestable API

1. Add `macos9/retro68-includes/Keychain.h` containing the three documented
   Carbon prototypes (`KCAddGenericPassword`, `KCFindGenericPassword`,
   `KCDeleteItem`) copied from Apple's Carbon Keychain Manager reference, not
   from memory, with `SecKeychainItemRef` as an opaque pointer.
2. Add `KeychainLib` to the link line **only** for the PowerPC target, behind a
   CMake option that defaults off, so a missing library is a configure error
   rather than a mystery link failure.
3. Verify in a Mac OS 9.2.2 guest (or on hardware) that the item is created,
   read and deleted. Until that run exists, the shim is unverified calls.

That shim is deliberately **not** in this change. It cannot be compiled here,
and the repository's rule is that a plausible-looking but untestable SDK
surface is worse than a documented gap. The refresh token stays memory-only:
the client asks for the app password again at launch, and it never writes the
token to a Preferences file.

## What remains unverified

- Whether `KCFindGenericPassword` prompts on first use under OS 9.2.2, and how
  that interacts with the `WaitNextEvent` loop.
- Whether the login keychain is unlocked in the QEMU guest, where no login
  item exists.
- The exact `OSStatus`/`OSErr` mapping on Mac OS 9 as opposed to CarbonLib on
  Mac OS X.
