#include "prefs_file.h"

#include <Files.h>
#include <Folders.h>
#include <Script.h>
#include <stddef.h>

/*
 * FSMakeFSSpec reports fnfErr for a file that does not exist yet, but still
 * fills in a spec that is good for creating it. Treating that as failure would
 * stop anything being saved the first time, so it is not.
 */
static OSErr prefs_spec(const unsigned char *name, FSSpec *spec)
{
    short vRefNum;
    long dirID;
    OSErr err;

    if (name == NULL || spec == NULL)
        return paramErr;

    err = FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder,
                     &vRefNum, &dirID);
    if (err != noErr)
        return err;

    err = FSMakeFSSpec(vRefNum, dirID, (ConstStr255Param)name, spec);
    return err == fnfErr ? noErr : err;
}

OSErr platinum_prefs_write(const unsigned char *name, OSType creator,
                           OSType file_type, const char *data, long length)
{
    FSSpec spec;
    short refNum;
    long count;
    OSErr err;

    if (data == NULL || length < 0)
        return paramErr;
    err = prefs_spec(name, &spec);
    if (err != noErr)
        return err;

    err = FSpOpenDF(&spec, fsRdWrPerm, &refNum);
    if (err == fnfErr) {
        err = FSpCreate(&spec, creator, file_type, smSystemScript);
        if (err == noErr)
            err = FSpOpenDF(&spec, fsRdWrPerm, &refNum);
    }
    if (err != noErr)
        return err;

    err = SetEOF(refNum, 0);
    if (err == noErr && length > 0) {
        count = length;
        err = FSWrite(refNum, &count, data);
        if (err == noErr && count != length)
            err = ioErr;
    }
    FSClose(refNum);
    return err;
}

OSErr platinum_prefs_read(const unsigned char *name, char *buffer,
                          long capacity, long *length)
{
    FSSpec spec;
    short refNum;
    long size;
    long count;
    OSErr err;

    if (length != NULL)
        *length = 0;
    if (buffer == NULL || capacity <= 0)
        return paramErr;
    buffer[0] = '\0';
    err = prefs_spec(name, &spec);
    if (err != noErr)
        return err;

    err = FSpOpenDF(&spec, fsRdPerm, &refNum);
    if (err != noErr)
        return err;
    err = GetEOF(refNum, &size);
    if (err != noErr) {
        FSClose(refNum);
        return err;
    }
    if (size < 0 || size >= capacity) {
        FSClose(refNum);
        return paramErr;
    }

    count = size;
    if (size > 0)
        err = FSRead(refNum, &count, buffer);
    FSClose(refNum);
    if (err != noErr)
        return err;
    if (count != size)
        return eofErr;

    buffer[size] = '\0';
    if (length != NULL)
        *length = size;
    return noErr;
}

OSErr platinum_prefs_delete(const unsigned char *name)
{
    FSSpec spec;
    OSErr err;

    err = prefs_spec(name, &spec);
    if (err != noErr)
        return err;
    err = FSpDelete(&spec);
    return err == fnfErr ? noErr : err;
}
