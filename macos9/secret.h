#ifndef PLATINUM_SECRET_H
#define PLATINUM_SECRET_H

#ifdef __cplusplus
extern "C" {
#endif

/* An app password is 19 characters; this leaves room. The bridge accepts 256. */
#define PLATINUM_SECRET_MAX 128

/*
 * A masked entry buffer for a password. It is not a TextEdit record, so the
 * characters are never drawn and never sit in a TextEdit handle, the scrap or an
 * undo buffer. It is static storage in the owner, and it is wiped when read out
 * for sending, on cancel and on close.
 */
typedef struct platinum_secret {
    char data[PLATINUM_SECRET_MAX + 1];
    long length;
} platinum_secret;

void platinum_secret_init(platinum_secret *secret);
/* Handle one typed character (MacRoman). Backspace (8) and forward delete (127)
 * remove the last one; other control characters are ignored; a full buffer
 * ignores more. Returns 1 if the contents changed. */
int platinum_secret_key(platinum_secret *secret, unsigned char ch);
long platinum_secret_length(const platinum_secret *secret);
/* One bullet (MacRoman 0xA5) per character, NUL-terminated; -1 if it does not
 * fit in `capacity`. */
long platinum_secret_mask(const platinum_secret *secret, char *out,
                          long capacity);
/* The contents as UTF-8 into `out`; -1 if empty or it does not fit. The caller
 * wipes `out` (platinum_bridge_wipe) when done. */
long platinum_secret_utf8(const platinum_secret *secret, char *out,
                          long capacity);
/* Overwrite and empty. */
void platinum_secret_wipe(platinum_secret *secret);

#ifdef __cplusplus
}
#endif

#endif
