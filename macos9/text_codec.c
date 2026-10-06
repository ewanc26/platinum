#include <MacTypes.h>
#include "text_codec.h"

#include <string.h>

static const unsigned long kMacRomanUnicode[128] = {
    0x00C4UL, 0x00C5UL, 0x00C7UL, 0x00C9UL, 0x00D1UL, 0x00D6UL, 0x00DCUL,
    0x00E1UL, 0x00E0UL, 0x00E2UL, 0x00E4UL, 0x00E3UL, 0x00E5UL, 0x00E7UL,
    0x00E9UL, 0x00E8UL, 0x00EAUL, 0x00EBUL, 0x00EDUL, 0x00ECUL, 0x00EEUL,
    0x00EFUL, 0x00F1UL, 0x00F3UL, 0x00F2UL, 0x00F4UL, 0x00F6UL, 0x00F5UL,
    0x00FAUL, 0x00F9UL, 0x00FBUL, 0x00FCUL, 0x2020UL, 0x00B0UL, 0x00A2UL,
    0x00A3UL, 0x00A7UL, 0x2022UL, 0x00B6UL, 0x00DFUL, 0x00AEUL, 0x00A9UL,
    0x2122UL, 0x00B4UL, 0x00A8UL, 0x2260UL, 0x00C6UL, 0x00D8UL, 0x221EUL,
    0x00B1UL, 0x2264UL, 0x2265UL, 0x00A5UL, 0x03BCUL, 0x2202UL, 0x03A3UL,
    0x03A0UL, 0x03C0UL, 0x222BUL, 0x00AAUL, 0x00BAUL, 0x03A9UL, 0x00E6UL,
    0x00F8UL, 0x00BFUL, 0x00A1UL, 0x00ACUL, 0x221AUL, 0x0192UL, 0x2248UL,
    0x2206UL, 0x00ABUL, 0x00BBUL, 0x2026UL, 0x00A0UL, 0x00C0UL, 0x00C3UL,
    0x00D5UL, 0x0152UL, 0x0153UL, 0x2013UL, 0x2014UL, 0x201CUL, 0x201DUL,
    0x2018UL, 0x2019UL, 0x00F7UL, 0x25CAUL, 0x00FFUL, 0x0178UL, 0x2044UL,
    0x20ACUL, 0x2039UL, 0x203AUL, 0xFB01UL, 0xFB02UL, 0x2021UL, 0x00B7UL,
    0x201AUL, 0x201EUL, 0x2030UL, 0x00C2UL, 0x00CAUL, 0x00C1UL, 0x00CBUL,
    0x00C8UL, 0x00CDUL, 0x00CEUL, 0x00CFUL, 0x00CCUL, 0x00D3UL, 0x00D4UL,
    0xF8FFUL, 0x00D2UL, 0x00DAUL, 0x00DBUL, 0x00D9UL, 0x0131UL, 0x02C6UL,
    0x02DCUL, 0x00AFUL, 0x02D8UL, 0x02D9UL, 0x02DAUL, 0x00B8UL, 0x02DDUL,
    0x02DBUL, 0x02C7UL
};

static long codec_write_utf8(unsigned long codepoint,
                             char *output,
                             long capacity,
                             long offset)
{
    if (codepoint <= 0x7FUL) {
        if (offset + 1 >= capacity)
            return -1;
        output[offset++] = (char)codepoint;
    } else if (codepoint <= 0x7FFUL) {
        if (offset + 2 >= capacity)
            return -1;
        output[offset++] = (char)(0xC0UL | (codepoint >> 6));
        output[offset++] = (char)(0x80UL | (codepoint & 0x3FUL));
    } else if (codepoint <= 0xFFFFUL) {
        if (offset + 3 >= capacity)
            return -1;
        output[offset++] = (char)(0xE0UL | (codepoint >> 12));
        output[offset++] = (char)(0x80UL |
                                  ((codepoint >> 6) & 0x3FUL));
        output[offset++] = (char)(0x80UL |
                                  (codepoint & 0x3FUL));
    } else if (codepoint <= 0x10FFFFUL) {
        if (offset + 4 >= capacity)
            return -1;
        output[offset++] = (char)(0xF0UL | (codepoint >> 18));
        output[offset++] = (char)(0x80UL |
                                  ((codepoint >> 12) & 0x3FUL));
        output[offset++] = (char)(0x80UL |
                                  ((codepoint >> 6) & 0x3FUL));
        output[offset++] = (char)(0x80UL |
                                  (codepoint & 0x3FUL));
    } else {
        return -1;
    }

    return offset;
}

static int codec_decode_utf8(const unsigned char *input,
                             long remaining,
                             unsigned long *codepoint,
                             long *consumed)
{
    unsigned long value;
    unsigned char first;

    if (input == NULL || codepoint == NULL || consumed == NULL ||
        remaining <= 0)
        return 0;

    first = input[0];

    if (first < 0x80U) {
        *codepoint = first;
        *consumed = 1;
        return 1;
    }

    if ((first & 0xE0U) == 0xC0U) {
        if (remaining < 2 || (input[1] & 0xC0U) != 0x80U)
            return 0;
        value = ((unsigned long)(first & 0x1FU) << 6) |
                (unsigned long)(input[1] & 0x3FU);
        if (value < 0x80UL)
            return 0;
        *codepoint = value;
        *consumed = 2;
        return 1;
    }

    if ((first & 0xF0U) == 0xE0U) {
        if (remaining < 3 ||
            (input[1] & 0xC0U) != 0x80U ||
            (input[2] & 0xC0U) != 0x80U)
            return 0;
        value = ((unsigned long)(first & 0x0FU) << 12) |
                ((unsigned long)(input[1] & 0x3FU) << 6) |
                (unsigned long)(input[2] & 0x3FU);
        if (value < 0x800UL ||
            (value >= 0xD800UL && value <= 0xDFFFUL))
            return 0;
        *codepoint = value;
        *consumed = 3;
        return 1;
    }

    if ((first & 0xF8U) == 0xF0U) {
        if (remaining < 4 ||
            (input[1] & 0xC0U) != 0x80U ||
            (input[2] & 0xC0U) != 0x80U ||
            (input[3] & 0xC0U) != 0x80U)
            return 0;
        value = ((unsigned long)(first & 0x07U) << 18) |
                ((unsigned long)(input[1] & 0x3FU) << 12) |
                ((unsigned long)(input[2] & 0x3FU) << 6) |
                (unsigned long)(input[3] & 0x3FU);
        if (value < 0x10000UL || value > 0x10FFFFUL)
            return 0;
        *codepoint = value;
        *consumed = 4;
        return 1;
    }

    return 0;
}

static int codec_macroman_byte(unsigned long codepoint,
                               unsigned char *value)
{
    int index;

    if (codepoint < 0x80UL) {
        *value = (unsigned char)codepoint;
        return 1;
    }

    for (index = 0; index < 128; ++index) {
        if (kMacRomanUnicode[index] == codepoint) {
            *value = (unsigned char)(index + 0x80);
            return 1;
        }
    }

    return 0;
}

long platinum_text_macroman_to_utf8(const char *input,
                                    char *output,
                                    long capacity,
                                    long *lossy)
{
    long input_length;
    long index;
    long offset;
    long next;
    unsigned char value;

    if (input == NULL || output == NULL || capacity <= 0)
        return -1;

    if (lossy != NULL)
        *lossy = 0;

    input_length = (long)strlen(input);
    offset = 0;

    for (index = 0; index < input_length; ++index) {
        value = (unsigned char)input[index];
        if (value < 0x80U)
            next = codec_write_utf8((unsigned long)value,
                                    output,
                                    capacity,
                                    offset);
        else
            next = codec_write_utf8(kMacRomanUnicode[value - 0x80U],
                                    output,
                                    capacity,
                                    offset);

        if (next < 0)
            return -1;
        offset = next;
    }

    output[offset] = '\0';
    return offset;
}

long platinum_text_utf8_to_macroman(const char *input,
                                    char *output,
                                    long capacity,
                                    long *lossy)
{
    long input_length;
    long index;
    long consumed;
    long offset;
    long ignored;
    unsigned long codepoint;
    unsigned char value;

    if (input == NULL || output == NULL || capacity <= 0)
        return -1;

    if (lossy != NULL)
        *lossy = 0;

    input_length = (long)strlen(input);
    index = 0;
    offset = 0;
    ignored = 0;

    while (index < input_length) {
        if (!codec_decode_utf8((const unsigned char *)(input + index),
                               input_length - index,
                               &codepoint,
                               &consumed))
            return -1;

        if (!codec_macroman_byte(codepoint, &value)) {
            value = '?';
            ++ignored;
        }

        if (offset + 1 >= capacity)
            return -1;

        output[offset++] = (char)value;
        index += consumed;
    }

    output[offset] = '\0';
    if (lossy != NULL)
        *lossy = ignored;
    return offset;
}
