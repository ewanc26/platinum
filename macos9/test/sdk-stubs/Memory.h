/* sdk-stubs/Memory.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_MEMORY_H
#define PLATINUM_STUB_MEMORY_H

#include <MacTypes.h>


Ptr NewPtr(Size byteCount);
Handle NewHandle(Size byteCount);
void HUnlock(Handle h);
void DisposePtr(Ptr p);
void DisposeHandle(Handle h);
SInt32 GetHandleSize(Handle h);
/* Free bytes in the application heap. The Multiversal definition's only
 * argument is a trap-word bit that selects the FreeMemSys variant. */
SInt32 FreeMem(void);

#endif
