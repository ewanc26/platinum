#ifndef PLATINUM_SHA256_H
#define PLATINUM_SHA256_H

#include <stddef.h>

/* SHA-256 (FIPS 180-4). Self-contained, no allocation, C89. */

void platinum_sha256(const unsigned char *data, size_t len, unsigned char out[32]);
void platinum_sha256_hex(const unsigned char *data, size_t len, char out_hex[65]);

#endif
