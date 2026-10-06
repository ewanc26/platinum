/* sdk-stubs/Files.h -- see MacTypes.h. Signatures follow the File Manager as
 * described by the Multiversal Interfaces (signatures.tsv). The FSSpec layout
 * here is not the real one. */
#ifndef PLATINUM_STUB_FILES_H
#define PLATINUM_STUB_FILES_H

#include <MacTypes.h>

/* Access permissions, as in Files.h: fsCurPerm 0, fsRdPerm 1, fsWrPerm 2, fsRdWrPerm 3. */
#define fsCurPerm 0
#define fsRdPerm 1
#define fsWrPerm 2
#define fsRdWrPerm 3

typedef struct StubFSSpec {
    short parID;
    SInt8 nodeName[64];
    SInt8 vRefNum;
} FSSpec;

OSErr FSpOpenDF(const FSSpec *spec, SInt8 permission, short *refNum);
OSErr FSpCreate(const FSSpec *spec, OSType creator, OSType fileType,
                ScriptCode scriptTag);
OSErr FSpDelete(const FSSpec *spec);

OSErr FSClose(short refNum);
OSErr SetEOF(short refNum, long logEOF);
OSErr GetEOF(short refNum, long *logEOF);

/* count is a pointer: bytes asked for in, bytes transferred out. It comes before the buffer. */
OSErr FSWrite(short refNum, long *count, const void *buffPtr);
OSErr FSRead(short refNum, long *count, void *buffPtr);

#endif
