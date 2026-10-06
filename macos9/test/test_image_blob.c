/*
 * test_image_blob.c -- host-side tests for reading the bridge's image blob, and
 * for asking the bridge for one. The blobs below are the exact outputs in
 * bridge/test/vectors/image/convert.json (tools/check-image-vectors.py keeps
 * them identical), so the bridge and the Mac are tested against the same bytes.
 * Not Classic Mac OS 9 validation; nothing is drawn here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge_client.h"
#include "image_blob.h"
#include "wolfram_stub.h"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;
    if (!condition) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

/* VECTOR_BLOBS_BEGIN */
static const char *kBlobs[] = {
    "504c494d0104000200020002000400000000ff00ff00ff0000ffffff21000300",
    "504c494d010400020002000200020000010101ffffff00001000",
    "504c494d010400010001000200010000ff7f7f0000",
    "504c494d010800010001000200010000ff7f7f0000",
};
/* VECTOR_BLOBS_END */

static unsigned char buffer[PLATINUM_IMAGE_MAX_BYTES + 200];

static long unhex(const char *hex, unsigned char *out)
{
    long n = 0;

    while (hex[0] != '\0' && hex[1] != '\0') {
        unsigned int byte;

        sscanf(hex, "%2x", &byte);
        out[n++] = (unsigned char)byte;
        hex += 2;
    }
    return n;
}

static void test_vectors(void)
{
    unsigned char blob[200];
    platinum_image image;
    long length;

    length = unhex(kBlobs[0], blob);
    check(platinum_image_parse(blob, length, &image) == PLATINUM_IMAGE_OK &&
              image.depth == 4 && image.width == 2 && image.height == 2 &&
              image.row_bytes == 2 && image.colours == 4,
          "four colours: header fields");
    check(image.palette[0] == 0x00 && image.palette[2] == 0xff && image.palette[3] == 0x00 &&
              image.palette[4] == 0xff && image.palette[6] == 0xff && image.palette[9] == 0xff &&
              image.palette[11] == 0xff,
          "four colours: palette is blue, green, red, white");
    check(image.pixels[0] == 0x21 && image.pixels[2] == 0x03, "four colours: pixels packed high nibble first");

    length = unhex(kBlobs[1], blob);
    check(platinum_image_parse(blob, length, &image) == PLATINUM_IMAGE_OK && image.colours == 2 &&
              image.pixels[2] == 0x10,
          "shrunk image parses");

    length = unhex(kBlobs[2], blob);
    check(platinum_image_parse(blob, length, &image) == PLATINUM_IMAGE_OK && image.width == 1 &&
              image.palette[0] == 0xff && image.palette[1] == 0x7f && image.palette[2] == 0x7f,
          "flattened pixel parses");

    length = unhex(kBlobs[3], blob);
    check(platinum_image_parse(blob, length, &image) == PLATINUM_IMAGE_OK && image.depth == 8 &&
              image.row_bytes == 2,
          "eight bit parses");
}

static void test_refusals(void)
{
    unsigned char blob[200];
    unsigned char copy[200];
    platinum_image image;
    long length;

    length = unhex(kBlobs[0], blob);

    check(platinum_image_parse(blob, 15, &image) == PLATINUM_IMAGE_TOO_SHORT &&
              image.palette == NULL,
          "shorter than a header");
    check(platinum_image_parse(NULL, 40, &image) == PLATINUM_IMAGE_TOO_SHORT &&
              platinum_image_parse(blob, length, NULL) == PLATINUM_IMAGE_TOO_SHORT,
          "NULL arguments");

    memcpy(copy, blob, (size_t)length); copy[0] = 'X';
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_MAGIC, "bad magic");
    memcpy(copy, blob, (size_t)length); copy[4] = 2;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_VERSION, "unknown version");
    memcpy(copy, blob, (size_t)length); copy[5] = 5;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_DEPTH, "depth 5");
    memcpy(copy, blob, (size_t)length); copy[5] = 1;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_DEPTH, "depth 1");
    memcpy(copy, blob, (size_t)length); copy[7] = 0;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_SIZE, "width 0");
    memcpy(copy, blob, (size_t)length); copy[6] = 0x01; copy[7] = 0x41;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_SIZE, "width 321");
    memcpy(copy, blob, (size_t)length); copy[9] = 0;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_SIZE, "height 0");
    memcpy(copy, blob, (size_t)length); copy[11] = 3;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_ROWBYTES, "odd rowBytes");
    memcpy(copy, blob, (size_t)length); copy[11] = 4;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_ROWBYTES, "rowBytes larger than needed");
    memcpy(copy, blob, (size_t)length); copy[13] = 0;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_COLOURS, "no colours");
    memcpy(copy, blob, (size_t)length); copy[13] = 17;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_COLOURS, "17 colours at depth 4");

    check(platinum_image_parse(blob, length - 1, &image) == PLATINUM_IMAGE_LENGTH_MISMATCH,
          "one byte short of what the header declares");
    memcpy(copy, blob, (size_t)length); copy[length] = 0;
    check(platinum_image_parse(copy, length + 1, &image) == PLATINUM_IMAGE_LENGTH_MISMATCH,
          "one byte more than the header declares");

    memcpy(copy, blob, (size_t)length); copy[28] = 0x91;
    check(platinum_image_parse(copy, length, &image) == PLATINUM_IMAGE_BAD_INDEX && image.pixels == NULL,
          "a pixel naming colour 9 of 4");

    check(platinum_image_parse(blob, PLATINUM_IMAGE_MAX_BYTES + 1, &image) == PLATINUM_IMAGE_TOO_BIG,
          "over the size limit, refused without reading");
}

static void test_biggest(void)
{
    platinum_image image;
    long colours = 16;
    long rows = 320;
    long row_bytes = 160;
    long total = 16 + colours * 3 + rows * row_bytes;

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, "PLIM", 4);
    buffer[4] = 1; buffer[5] = 4;
    buffer[6] = 0x01; buffer[7] = 0x40;
    buffer[8] = 0x01; buffer[9] = 0x40;
    buffer[10] = 0; buffer[11] = 160;
    buffer[12] = 0; buffer[13] = 16;
    check(total == 51264 && platinum_image_parse(buffer, total, &image) == PLATINUM_IMAGE_OK &&
              image.width == 320 && image.height == 320,
          "the largest allowed image, 320 x 320 at depth 4, parses");

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, "PLIM", 4);
    buffer[4] = 1; buffer[5] = 8;
    buffer[6] = 0x01; buffer[7] = 0x40;
    buffer[8] = 0x01; buffer[9] = 0x40;
    buffer[10] = 0x01; buffer[11] = 0x40;
    buffer[12] = 0x01; buffer[13] = 0x00;
    check(platinum_image_parse(buffer, 16 + 768 + 320L * 320L, &image) == PLATINUM_IMAGE_TOO_BIG,
          "320 x 320 at depth 8 is over the limit");
}

static void test_fetch(void)
{
    platinum_bridge_client *client = platinum_bridge_client_new("https://bridge.example");
    unsigned char *blob = NULL;
    long length = 0;
    static unsigned char with_nuls[40];

    memset(with_nuls, 0, sizeof(with_nuls));
    memcpy(with_nuls, "PLIM", 4);
    with_nuls[4] = 1;
    fake_status = WF_OK;
    fake_http_status = 200;
    fake_body = (const char *)with_nuls;
    fake_body_len = 32;
    last_url[0] = '\0';
    check(platinum_bridge_get_image(client, "feed_thumbnail/plain/did:plc:abc/bafy@jpeg", 160, 4,
                                    &blob, &length) == WF_OK &&
              length == 32 && blob != NULL && memcmp(blob, "PLIM", 4) == 0 && blob[10] == 0,
          "a blob with NUL bytes comes back whole");
    check(strstr(last_url, "/v1/image?ref=feed_thumbnail%2Fplain%2Fdid%3Aplc%3Aabc%2Fbafy%40jpeg&w=160&depth=4") != NULL,
          "the ref is percent-encoded and the size and depth are sent");
    free(blob);

    blob = (unsigned char *)1;
    check(platinum_bridge_get_image(client, "", 160, 4, &blob, &length) == WF_ERR_INVALID_ARG && blob == NULL,
          "an empty ref is refused");
    last_url[0] = '\0';
    check(platinum_bridge_get_image(client, "a", 15, 4, &blob, &length) == WF_ERR_INVALID_ARG &&
              platinum_bridge_get_image(client, "a", 321, 4, &blob, &length) == WF_ERR_INVALID_ARG &&
              platinum_bridge_get_image(client, "a", 100, 5, &blob, &length) == WF_ERR_INVALID_ARG &&
              last_url[0] == '\0',
          "a width or depth out of range is refused with no request");

    fake_body_len = 0;
    fake_body = "";
    check(platinum_bridge_get_image(client, "a", 100, 4, &blob, &length) == WF_ERR_PARSE && blob == NULL,
          "an empty answer is not an image");

    fake_status = WF_ERR_HTTP;
    fake_http_status = 502;
    fake_body = "{\"error\":\"image_unavailable\"}";
    check(platinum_bridge_get_image(client, "a", 100, 4, &blob, &length) == WF_ERR_HTTP && blob == NULL,
          "a bridge error is reported and no blob is kept");

    fake_status = WF_OK;
    fake_http_status = 200;
    fake_body = (const char *)buffer;
    memset(buffer, 1, sizeof(buffer));
    fake_body_len = (size_t)PLATINUM_IMAGE_FETCH_MAX + 1;
    check(platinum_bridge_get_image(client, "a", 100, 4, &blob, &length) == WF_ERR_PARSE && blob == NULL,
          "an answer over the limit is refused");
    fake_body_len = 0;
    platinum_bridge_client_free(client);
}

int main(void)
{
    test_vectors();
    test_refusals();
    test_biggest();
    test_fetch();
    if (failures != 0) {
        printf("test_image_blob: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_image_blob: all %d checks passed\n", checks);
    return 0;
}
