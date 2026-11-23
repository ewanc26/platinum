#ifndef PLATINUM_TEXT_CODEC_H
#define PLATINUM_TEXT_CODEC_H

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_TEXT_MAX_CODEPOINTS 300
#define PLATINUM_TEXT_UTF8_CAPACITY 901

/* One deliberate glyph for a run of text MacRoman cannot represent. The
   Mac OS 9 Toolbox text stack draws the installed script, not arbitrary
   Unicode, so an emoji, a flag or a CJK word becomes this placeholder rather
   than nothing or a '?' per code point. See docs/LANGUAGE_BOUNDARIES.md. */
#define PLATINUM_TEXT_PLACEHOLDER '?'

long platinum_text_macroman_to_utf8(const char *input,
                                    char *output,
                                    long capacity,
                                    long *lossy);

long platinum_text_utf8_to_macroman(const char *input,
                                    char *output,
                                    long capacity,
                                    long *lossy);

#ifdef __cplusplus
}
#endif

#endif
