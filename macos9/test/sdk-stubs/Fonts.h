/* sdk-stubs/Fonts.h -- see MacTypes.h. */
#ifndef PLATINUM_STUB_FONTS_H
#define PLATINUM_STUB_FONTS_H

#include <MacTypes.h>
#include <TextEdit.h>

void GetFontName(long fontID, Str255 name);
short GetFontFamily(StringPtr name);
long GetDefaultFont(long fontNum);

#endif
