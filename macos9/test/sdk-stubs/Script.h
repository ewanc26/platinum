/* sdk-stubs/Script.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_SCRIPT_H
#define PLATINUM_STUB_SCRIPT_H

#include <MacTypes.h>

typedef Ptr ScriptPtr;
typedef ScriptPtr Script;
typedef ScriptPtr ScriptHandle;

OSStatus ScriptPutString(ScriptPtr script, const char *theString,
                         short length);

#endif
