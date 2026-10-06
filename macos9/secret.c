#include "secret.h"
#include "bridge_client.h"
#include "text_codec.h"

#include <string.h>

void platinum_secret_init(platinum_secret *secret)
{
    if (secret != NULL) {
        platinum_bridge_wipe(secret, sizeof(*secret));
        secret->length = 0;
    }
}

int platinum_secret_key(platinum_secret *secret, unsigned char ch)
{
    if (secret == NULL)
        return 0;
    if (ch == 8 || ch == 127) {
        if (secret->length == 0)
            return 0;
        secret->length--;
        secret->data[secret->length] = '\0';
        return 1;
    }
    if (ch < 32)
        return 0;
    if (secret->length >= PLATINUM_SECRET_MAX)
        return 0;
    secret->data[secret->length++] = (char)ch;
    secret->data[secret->length] = '\0';
    return 1;
}

long platinum_secret_length(const platinum_secret *secret)
{
    return secret == NULL ? 0 : secret->length;
}

long platinum_secret_mask(const platinum_secret *secret, char *out,
                          long capacity)
{
    long i;

    if (secret == NULL || out == NULL || capacity <= secret->length)
        return -1;
    for (i = 0; i < secret->length; ++i)
        out[i] = (char)0xA5;
    out[secret->length] = '\0';
    return secret->length;
}

long platinum_secret_utf8(const platinum_secret *secret, char *out,
                          long capacity)
{
    if (secret == NULL || out == NULL || secret->length == 0)
        return -1;
    return platinum_text_macroman_to_utf8(secret->data, out, capacity, NULL);
}

void platinum_secret_wipe(platinum_secret *secret)
{
    if (secret != NULL) {
        platinum_bridge_wipe(secret->data, sizeof(secret->data));
        secret->length = 0;
    }
}
