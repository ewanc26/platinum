/* sdk-stubs/Memory.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_MEMORY_H
#define PLATINUM_STUB_MEMORY_H

#include <MacTypes.h>

void BlockCopy(const void *src, void *dst, long byteCount);
void BlockZero(void *dst, long byteCount);
void MemMove(void *src, void *dst, long byteCount);

Ptr NewPtr(long byteCount);
Ptr NewHandle(long byteCount);
void DisposePtr(Ptr p);
void DisposeHandle(Handle h);
Ptr SetPtr(Handle h, long byteCount);
SInt32 MemPtrSize(Ptr p);
SInt32 GetHandleSize(Handle h);

#endif
