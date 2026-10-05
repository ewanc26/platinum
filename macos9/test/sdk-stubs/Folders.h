/* sdk-stubs/Folders.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_FOLDERS_H
#define PLATINUM_STUB_FOLDERS_H

#include <MacTypes.h>
#include <Files.h>

#define kOnSystemDisk 0
#define kSystemFolderType 0x7373
#define kPreferencesFolderType 0x7072
#define kTemporaryFolderType 0x746D
#define kCreateFolder 0

OSStatus FindFolder(short vRefNum, OSType folderID, Boolean create,
                    short *refNum, long *dirID);
OSStatus FSMakeFSSpec(short vRefNum, SInt32 parID, ConstStr255Param name,
                      FSSpec *spec);

#endif
