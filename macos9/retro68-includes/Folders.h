/*
 * Folders.h -- Multiversal Interfaces wrapper for the Retro68 build.
 *
 * Retro68's Multiversal Interfaces declare the Folder Manager in
 * Multiverse.h rather than in a Folders.h, so this header supplies the
 * standard name. With Apple's Universal Interfaces instead, the real
 * Folders.h is used.
 */
#if __has_include(<Multiverse.h>)
#include <Multiverse.h>
/* vRefNum meaning "the startup volume", and FindFolder's createFolder
 * value; neither constant is in the Multiversal Interfaces. */
#define kOnSystemDisk (-32768)
#define kCreateFolder 1
#else
#include_next <Folders.h>
#endif
