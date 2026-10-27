/*
 * Script.h -- Multiversal Interfaces wrapper for the Retro68 build.
 *
 * Retro68's Multiversal Interfaces declare the Script Manager in
 * Multiverse.h rather than in a Script.h, so this header supplies the
 * standard name. With Apple's Universal Interfaces instead, the real
 * Script.h is used.
 */
#if __has_include(<Multiverse.h>)
#include <Multiverse.h>
#else
#include_next <Script.h>
#endif
