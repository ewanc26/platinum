#ifndef PLATINUM_TEXT_CODEC_H
#define PLATINUM_TEXT_CODEC_H

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_TEXT_MAX_CODEPOINTS 300
#define PLATINUM_TEXT_UTF8_CAPACITY 901

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
