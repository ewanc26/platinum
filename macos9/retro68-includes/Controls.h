/*
 * Controls.h -- Multiversal Interfaces wrapper for the Retro68 build.
 *
 * Retro68's Multiversal Interfaces declare the Control Manager in
 * Multiverse.h rather than in a Controls.h, so this header supplies the
 * standard name. With Apple's Universal Interfaces instead, the real
 * Controls.h is used.
 */
#if __has_include(<Multiverse.h>)
#include <Multiverse.h>
/* The standard scroll bar control definition ID; not in the Multiversal
 * Interfaces. */
#define scrollBarProc 16
#else
#include_next <Controls.h>
#endif
