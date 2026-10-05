/* sdk-stubs/Files.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_FILES_H
#define PLATINUM_STUB_FILES_H

#include <MacTypes.h>

#define fsRdWrPerm 1
#define fsRdPerm 0

typedef struct StubFSSpec {
    short parID;
    SInt8 nodeName[64];
    SInt8 vRefNum;
} FSSpec;

typedef struct StubParamRecord {
    const FSSpec *paramFileSpec;
    long ioParam;
} ParmBlkRec;

typedef ParmBlkRec *PBRec;

OSStatus PBOpen(const ParmBlkRec *fileParam, SInt16 ioPerm, SInt16 *refNum);
OSStatus PBClose(SInt16 refNum);
OSStatus PBCreate(const ParmBlkRec *newParam, SInt16 ioPerm, SInt16 *refNum);
OSStatus PBGetFSSpec(SInt16 refNum, FSSpec *spec);
void PBGetWParam(const ParmBlkRec *param, short *refNum, OSType *type,
                 long *ioParam);

OSStatus FSpOpenDF(const FSSpec *spec, SInt8 ioPerm, short *refNum);
OSStatus FSpClose(SInt16 refNum);
OSStatus FSpCreate(const FSSpec *spec, FourCharCode creator, FourCharCode type,
                   SInt16 script);
OSStatus FSDelete(const FSSpec *spec);
OSStatus FSpDelete(const FSSpec *spec);

OSStatus FSClose(SInt16 refNum);
OSErr SetEOF(SInt16 refNum, long newLength);
OSErr GetEOF(SInt16 refNum, long *newLength);
OSErr FSWrite(SInt16 refNum, const void *buffer, long *byteCount);
OSErr FSWritErr(SInt16 refNum, long newLength);
OSErr FSRead(SInt16 refNum, void *buffer, long *byteCount);

#endif
