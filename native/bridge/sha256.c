#include <string.h>

#include "sha256.h"

#define PLATINUM_SHA256_MASK 0xFFFFFFFFUL
#define PLATINUM_SHA256_ROTR(x, n) ((((x) >> (n)) | ((x) << (32 - (n)))) & PLATINUM_SHA256_MASK)

static const unsigned long K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

static const unsigned long H0[8] = {
    0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
    0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL
};

static void compress(unsigned long h[8], const unsigned char *block)
{
    unsigned long w[64];
    unsigned long a, b, c, d, e, f, g, hh;
    unsigned long s0, s1, ch, maj, t1, t2;
    int t;

    for (t = 0; t < 16; t++) {
        w[t] = ((unsigned long)block[4 * t] << 24)
             | ((unsigned long)block[4 * t + 1] << 16)
             | ((unsigned long)block[4 * t + 2] << 8)
             | (unsigned long)block[4 * t + 3];
    }
    for (t = 16; t < 64; t++) {
        s0 = PLATINUM_SHA256_ROTR(w[t - 15], 7) ^ PLATINUM_SHA256_ROTR(w[t - 15], 18) ^ (w[t - 15] >> 3);
        s1 = PLATINUM_SHA256_ROTR(w[t - 2], 17) ^ PLATINUM_SHA256_ROTR(w[t - 2], 19) ^ (w[t - 2] >> 10);
        w[t] = (w[t - 16] + s0 + w[t - 7] + s1) & PLATINUM_SHA256_MASK;
    }

    a = h[0]; b = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; hh = h[7];

    for (t = 0; t < 64; t++) {
        s1 = PLATINUM_SHA256_ROTR(e, 6) ^ PLATINUM_SHA256_ROTR(e, 11) ^ PLATINUM_SHA256_ROTR(e, 25);
        ch = (e & f) ^ ((~e) & g);
        t1 = (hh + s1 + ch + K[t] + w[t]) & PLATINUM_SHA256_MASK;
        s0 = PLATINUM_SHA256_ROTR(a, 2) ^ PLATINUM_SHA256_ROTR(a, 13) ^ PLATINUM_SHA256_ROTR(a, 22);
        maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = (s0 + maj) & PLATINUM_SHA256_MASK;

        hh = g;
        g = f;
        f = e;
        e = (d + t1) & PLATINUM_SHA256_MASK;
        d = c;
        c = b;
        b = a;
        a = (t1 + t2) & PLATINUM_SHA256_MASK;
    }

    h[0] = (h[0] + a) & PLATINUM_SHA256_MASK;
    h[1] = (h[1] + b) & PLATINUM_SHA256_MASK;
    h[2] = (h[2] + c) & PLATINUM_SHA256_MASK;
    h[3] = (h[3] + d) & PLATINUM_SHA256_MASK;
    h[4] = (h[4] + e) & PLATINUM_SHA256_MASK;
    h[5] = (h[5] + f) & PLATINUM_SHA256_MASK;
    h[6] = (h[6] + g) & PLATINUM_SHA256_MASK;
    h[7] = (h[7] + hh) & PLATINUM_SHA256_MASK;
}

void platinum_sha256(const unsigned char *data, size_t len, unsigned char out[32])
{
    unsigned long h[8];
    unsigned char block[64];
    unsigned long bits_lo, bits_hi;
    size_t off, rem;
    int i;

    for (i = 0; i < 8; i++) {
        h[i] = H0[i];
    }

    off = 0;
    while (len - off >= 64) {
        compress(h, data + off);
        off += 64;
    }

    /* Final block(s): remaining bytes, 0x80, zero padding, 64-bit big-endian bit length. */
    rem = len - off;
    memset(block, 0, sizeof(block));
    if (rem > 0) {
        memcpy(block, data + off, rem);
    }
    block[rem] = 0x80;
    if (rem >= 56) {
        compress(h, block);
        memset(block, 0, sizeof(block));
    }

    bits_lo = ((unsigned long)len << 3) & PLATINUM_SHA256_MASK;
    bits_hi = ((unsigned long)len >> 29) & PLATINUM_SHA256_MASK;
    block[56] = (unsigned char)((bits_hi >> 24) & 0xFF);
    block[57] = (unsigned char)((bits_hi >> 16) & 0xFF);
    block[58] = (unsigned char)((bits_hi >> 8) & 0xFF);
    block[59] = (unsigned char)(bits_hi & 0xFF);
    block[60] = (unsigned char)((bits_lo >> 24) & 0xFF);
    block[61] = (unsigned char)((bits_lo >> 16) & 0xFF);
    block[62] = (unsigned char)((bits_lo >> 8) & 0xFF);
    block[63] = (unsigned char)(bits_lo & 0xFF);
    compress(h, block);

    for (i = 0; i < 8; i++) {
        out[4 * i]     = (unsigned char)((h[i] >> 24) & 0xFF);
        out[4 * i + 1] = (unsigned char)((h[i] >> 16) & 0xFF);
        out[4 * i + 2] = (unsigned char)((h[i] >> 8) & 0xFF);
        out[4 * i + 3] = (unsigned char)(h[i] & 0xFF);
    }

    memset(block, 0, sizeof(block));
}

void platinum_sha256_hex(const unsigned char *data, size_t len, char out_hex[65])
{
    static const char digits[] = "0123456789abcdef";
    unsigned char d[32];
    int i;

    platinum_sha256(data, len, d);
    for (i = 0; i < 32; i++) {
        out_hex[2 * i] = digits[(d[i] >> 4) & 0x0F];
        out_hex[2 * i + 1] = digits[d[i] & 0x0F];
    }
    out_hex[64] = '\0';
}
