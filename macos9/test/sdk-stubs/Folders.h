/* sdk-stubs/Folders.h -- see MacTypes.h. Signatures follow the Folder Manager
 * and File Manager as described by the Multiversal Interfaces and Apple's
 * Folders.h constants. */
#ifndef PLATINUM_STUB_FOLDERS_H
#define PLATINUM_STUB_FOLDERS_H

#include <MacTypes.h>
#include <Files.h>

/* vRefNum meaning "the startup volume", then four-character folder types. */
#define kOnSystemDisk (-32768)
#define kSystemFolderType 0x6D616373L      /* 'macs' */
#define kPreferencesFolderType 0x70726566L /* 'pref' */
#define kTemporaryFolderType 0x74656D70L   /* 'temp' */
#define kCreateFolder 1

OSErr FindFolder(short vRefNum, OSType folderType, Boolean createFolder,
                 short *foundVRefNum, long *foundDirID);
OSErr FSMakeFSSpec(short vRefNum, long dirID, ConstStr255Param fileName,
                   FSSpec *spec);

#endif
