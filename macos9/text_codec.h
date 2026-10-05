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

/*
 * Pack a key event into the KeyMap that the TextEdit control expects.
 *
 * TEKey does not take a character. It takes a KeyMap, which is four longs
 * holding the key code, the modifier state and a repeat of each. Passing a
 * character code instead makes the control read whatever follows it in memory
 * as the key state, so every call site has to go through this.
 */
void platinum_text_key_map(KeyMap key_map,
                           short key_code,
                           unsigned long modifiers);

#ifdef __cplusplus
}
#endif

#endif