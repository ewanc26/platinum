/*
 * test_image_encode.c -- host-side tests for the image encoder. The cases between
 * the VECTOR_CASES markers are the converter vectors in
 * bridge/test/vectors/image/convert.json (tools/check-image-vectors.py keeps them
 * identical). The hand-derived cases are worked out from docs/IMAGES.md, not
 * from running the encoder, so they can catch an encoder that agrees with
 * itself and not with the format. The TS_SNAPSHOT block is converter output for
 * images that need the median cut; bridge/test/image-encode-snapshots.test.ts
 * checks the converter still produces it. Every output is read back with
 * platinum_image_parse. Not Classic Mac OS 9 validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_blob.h"
#include "image_encode.h"

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

/* VECTOR_CASES_BEGIN */
static const char *kCases[] = {
    "2 2 320 320 4 ff0000ff00ff00ff0000ffffffffffff 504c494d0104000200020002000400000000ff00ff00ff0000ffffff21000300",
    "4 4 2 2 4 000000ff010101ff000000ff000000ff010101ff010101ff000000ff020202ffffffffffffffffff000000ff000000fffffffffffefefeff010101ff010101ff 504c494d010400020002000200020000010101ffffff00001000",
    "1 1 320 320 4 ff000080 504c494d010400010001000200010000ff7f7f0000",
    "1 1 320 320 8 ff000080 504c494d010800010001000200010000ff7f7f0000",
};
/* VECTOR_CASES_END */

static unsigned char pixels[4096];
static unsigned char expected[4096];
static unsigned char out[PLATINUM_IMAGE_MAX_BYTES];
static unsigned char big[PLATINUM_IMAGE_MAX_SIDE * PLATINUM_IMAGE_MAX_SIDE * 4];

/* "name N" into `what`, without sprintf: the labels are only for failure messages. */
static void label(char *what, const char *name, int n)
{
    char digits[12];
    int d = 0;
    int i = 0;

    while (*name != '\0')
        what[i++] = *name++;
    what[i++] = ' ';
    do {
        digits[d++] = (char)('0' + n % 10);
        n /= 10;
    } while (n > 0);
    while (d > 0)
        what[i++] = digits[--d];
    what[i] = '\0';
}

static long unhex(const char *hex, unsigned char *dest)
{
    long n = 0;

    while (hex[0] != '\0' && hex[1] != '\0') {
        unsigned int byte;

        sscanf(hex, "%2x", &byte);
        dest[n++] = (unsigned char)byte;
        hex += 2;
    }
    return n;
}

/* Encode and check the bytes against `hex`, and read the result back. */
static void expect_blob(const char *what, const unsigned char *rgba, long width,
                        long height, int max_w, int max_h, int depth,
                        const char *hex)
{
    long want = unhex(hex, expected);
    long length = -1;
    platinum_image image;
    int status = platinum_image_encode(rgba, width, height, max_w, max_h, depth,
                                       out, (long)sizeof(out), &length);

    check(status == PLATINUM_ENCODE_OK, what);
    check(length == want && memcmp(out, expected, (size_t)want) == 0, what);
    check(platinum_image_parse(out, length, &image) == PLATINUM_IMAGE_OK, what);
}

static void test_vectors(void)
{
    size_t i;
    platinum_image image;

    for (i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
        long width;
        long height;
        int max_w;
        int max_h;
        int depth;
        char rgba_hex[600];
        char blob_hex[600];
        char what[64];
        long rgba_len;
        long want;
        long length = -1;
        int status;

        sscanf(kCases[i], "%ld %ld %d %d %d %599s %599s", &width, &height,
               &max_w, &max_h, &depth, rgba_hex, blob_hex);
        rgba_len = unhex(rgba_hex, pixels);
        want = unhex(blob_hex, expected);
        label(what, "vector", (int)i);
        check(rgba_len == width * height * 4, what);
        status = platinum_image_encode(pixels, width, height, max_w, max_h, depth,
                                       out, (long)sizeof(out), &length);
        check(status == PLATINUM_ENCODE_OK && length == want &&
                  memcmp(out, expected, (size_t)want) == 0,
              what);
        check(platinum_image_parse(out, length, &image) == PLATINUM_IMAGE_OK, what);
    }
}

static void test_hand_derived(void)
{
    /*
     * Uneven ratio. Three grey pixels 0, 1, 2 shrink to two (max width 2). The
     * height is 1, so the scale is 2/3 and the destination is 2 x 1. Destination
     * 0 averages source [0,1) = 0. Destination 1 averages source [1,3) = (1+2+1)/2
     * = 2 (half up). The palette is 0 and 2, so indices are 0 and 1, packed
     * high nibble first in one byte with a zero pad byte to make rowBytes 2.
     *   header: PLIM, version 1, depth 4, 2 x 1, rowBytes 2, 2 colours, reserved 0
     *   palette: 000000 020202
     *   pixels: 0x01 (index 0, then index 1), 0x00 pad
     */
    expect_blob("uneven-ratio shrink of three pixels", (const unsigned char *)
                "\x00\x00\x00\xff\x01\x01\x01\xff\x02\x02\x02\xff",
                3, 1, 2, 320, 4,
                "504c494d" "01" "04" "0002" "0001" "0002" "0002" "0000"
                "000000" "020202" "0100");

    /*
     * Odd width at depth 4. Three pixels 10, 20, 20 with no shrink. The palette
     * is 10 and 20, so indices are 0, 1, 1. Row bytes are 2 (12 bits round up to
     * 2 bytes, already even). Byte 0 holds pixel 0 high and pixel 1 low: 0x01.
     * Byte 1 holds pixel 2 high and a zero low nibble pad: 0x10.
     */
    expect_blob("odd width packs the last nibble and pads with zero",
                (const unsigned char *)
                "\x0a\x0a\x0a\xff\x14\x14\x14\xff\x14\x14\x14\xff",
                3, 1, 320, 320, 4,
                "504c494d" "01" "04" "0003" "0001" "0002" "0002" "0000"
                "0a0a0a" "141414" "0110");
}

static void test_flatten_boundary(void)
{
    /*
     * Flatten rounding at an exact boundary. With c = 1 and a = 128 the sum is
     * 1*128 + 255*127 + 127 = 32640, and 32640 / 255 = 128 exactly, so the +127
     * matters: +126 would give 127. Green and blue (c = 0) are
     * (255*127 + 127) / 255 = 127.5, rounded down to 127.
     */
    expect_blob("flatten rounds half up, exactly at a boundary",
                (const unsigned char *)"\x01\x00\x00\x80", 1, 1, 320, 320, 4,
                "504c494d" "01" "04" "0001" "0001" "0002" "0001" "0000"
                "807f7f" "0000");
}

static void test_limit(void)
{
    long length = -1;
    platinum_image image;
    long p;
    long k;

    /*
     * 250 x 319 at depth 8, 78 colours: 16 + 3*78 + 319*250 = 80,000 exactly.
     * Colour k is (k, 0, 0), and pixel p is colour p mod 78, so all 78 appear.
     */
    for (p = 0; p < 250L * 319L; p++) {
        k = p % 78;
        big[p * 4] = (unsigned char)k;
        big[p * 4 + 1] = 0;
        big[p * 4 + 2] = 0;
        big[p * 4 + 3] = 255;
    }
    check(platinum_image_encode(big, 250, 319, 320, 320, 8, out, (long)sizeof(out),
                                &length) == PLATINUM_ENCODE_OK &&
              length == 80000 &&
              platinum_image_parse(out, length, &image) == PLATINUM_IMAGE_OK,
          "a blob of exactly 80,000 bytes is accepted");

    /*
     * 250 x 317 at depth 8, 245 colours: 16 + 3*245 + 317*250 = 80,001, one over.
     */
    for (p = 0; p < 250L * 317L; p++) {
        k = p % 245;
        big[p * 4] = (unsigned char)k;
        big[p * 4 + 1] = 0;
        big[p * 4 + 2] = 0;
        big[p * 4 + 3] = 255;
    }
    length = -1;
    check(platinum_image_encode(big, 250, 317, 320, 320, 8, out, (long)sizeof(out),
                                &length) == PLATINUM_ENCODE_TOO_LARGE && length == 0,
          "a blob of 80,001 bytes is refused");
}

static void test_median_hand(void)
{
    /*
     * 27 distinct colours (a, b, c), each channel 0, 1 or 2, in one row. All of
     * them fall in one 5-bit bin, so the median cut has one box with one bin and
     * never splits (pick < 0). Its palette entry is the box mean: each channel
     * sums to 9 * (0 + 1 + 2) = 27 over 27 pixels, and (27 + 13) / 27 = 1. So the
     * palette is the single colour 01 01 01, and every pixel is index 0.
     * Row bytes: 27 pixels at depth 4 take 14 bytes, already even.
     *   header: PLIM, 1, depth 4, width 27, height 1, rowBytes 14, 1 colour, 0
     *   palette: 010101
     *   pixels: 14 zero bytes
     */
    static unsigned char cube[27 * 4];
    int a;
    int b;
    int c;
    int i = 0;

    for (a = 0; a < 3; a++) {
        for (b = 0; b < 3; b++) {
            for (c = 0; c < 3; c++) {
                cube[i * 4] = (unsigned char)a;
                cube[i * 4 + 1] = (unsigned char)b;
                cube[i * 4 + 2] = (unsigned char)c;
                cube[i * 4 + 3] = 255;
                i++;
            }
        }
    }
    expect_blob("27 colours in one bin: the median cut palette is their mean",
                cube, 27, 1, 320, 320, 4,
                "504c494d" "01" "04" "001b" "0001" "000e" "0001" "0000"
                "010101"
                "00000000000000000000000000" "00");
}

/*
 * Snapshots of the TypeScript converter's output (bridge/src/image/convert.ts) for
 * images that need the median cut. They are not in convert.json, so
 * tools/check-image-vectors.py does not see them; bridge/test/image-encode-
 * snapshots.test.ts reads this block and checks convertPixels reproduces each
 * blob, so a change to either side fails CI. Each entry is "SNAP width height
 * maxW maxH depth", then the RGBA hex in chunks, then "=", then the blob hex in
 * chunks, then "END". Chunks are under 400 characters, the C89 literal limit.
 */
/* TS_SNAPSHOT_BEGIN */
static const char *kSnapshots[] = {
    "SNAP 40 46 34 320 4",
    "000000ff25000dff4a001aff6f0027ff940034ffb90041ffde004e1203005b2f280068ff4d0075ff7200822d97008f3cbc009cffe100a9ff0600b6ff2b00c3ff5000d0ff7500dd239a00eaffbf00f7ffe40004ff090011122e001eff53002bff7800383a9d0045ffc20052ffe7005fff0c006c77310079ff560086ff7b0093ffa000a0ffc500adffea00baff0f00c7ff3400d4ff5900e1ff7e00eeffa300fbff005b0dff255b1aff4a5b27ff6f5b34ff945b416fb95b4e08de5b5bff035b68ff285b75ff4d5b82ff",
    "725b8fff975b9cc8bc5ba9ffe15bb6ff065bc3672b5bd0ff505bddff755beac39a5bf779bf5b04ffe45b11ff095b1eff2e5b2bff535b38ff785b45ff9d5b52ffc25b5fffe75b6c3b0c5b79ff315b86ff565b93f47b5ba0ffa05badffc55bbaffea5bc7ff0f5bd4ff345be1ff595beeff7e5bfbffa35b089400b61aff25b627ff4ab634ff6fb641ff94b64effb9b65b59deb668ff03b675ff28b682ff4db68fff72b69cff97b6a9ffbcb6b657e1b6c3ff06b6d0ff2bb6ddb350b6eaff75b6f7ff9ab604cdbfb611ff",
    "e4b61eff09b62bff2eb6387553b645ff78b652ff9db65fffc2b66cffe7b679ff0cb686ff31b6936b56b6a0ff7bb6adffa0b6baffc5b6c7ffeab6d4ff0fb6e1ff34b6eeff59b6fbff7eb60875a3b615ff001127ff251134ff4a1141ff6f114eff94115bffb91168ffde1175f7031182ff28118fff4d119cff7211a9ff9711b6ffbc11c3ffe111d0ff0611ddff2b11eaff5011f7ff751104829a1111ffbf111e07e4112bff0911381a2e1145ff531152ff78115fff9d116cffc21179ffe71186ff0c1193ff3111a0ff",
    "5611adc17b11ba7ca011c7ffc511d4a3ea11e1ff0f11eeff3411fbff591108ff7e1115ffa31122ff006c34ff256c41ff4a6c4eff6f6c5bff946c68ffb96c75ffde6c82ff036c8fff286c9cff4d6ca9ff726cb604976cc3ffbc6cd0ffe16cddff066ceaff2b6cf7ff506c04ff756c11ff9a6c1effbf6c2bffe46c38ff096c45ff2e6c52ff536c5fff786c6c899d6c790dc26c86ffe76c93ff0c6ca0ff316cadff566cbaff7b6cc7ffa06cd430c56ce1ffea6ceeff0f6cfbff346c08ff596c15ff7e6c22ffa36c2fff",
    "00c7418e25c74eff4ac75bff6fc768ff94c775ffb9c782ffdec78f1603c79cff28c7a9ff4dc7b6ff72c7c3ff97c7d0ffbcc7ddffe1c7eaff06c7f7ff2bc704ff50c711ff75c71eff9ac72bffbfc738ffe4c745ff09c752ff2ec75fff53c76c2478c779ff9dc786ffc2c793ffe7c7a0ff0cc7adff31c7baf656c7c7ff7bc7d4ffa0c7e1ffc5c7eeffeac7fbff0fc7083e34c715ff59c722ff7ec72fffa3c73cff00224e1025225b174a2268ff6f2275ff942282ffb9228fffde229c8e0322a9ff2822b6174d22c3ff",
    "7222d0609722ddffbc22eaffe122f7ff062204ff2b2211ff50221eff75222b909a2238c2bf2245ffe42252ff09225fff2e226cff532279ff782286539d2293ffc222a0ffe722adff0c22baff3122c7ff5622d4ff7b22e1cba022eeffc522fbffea2208b60f221505342222ff59222fff7e223cffa32249ff007d5bff257d68ff4a7d75ff6f7d82ff947d8fffb97d9cffde7da9ff037db6ff287dc3ff4d7dd0ff727dddff977deaffbc7df7ffe17d0467067d119b2b7d1e53507d2bff757d38ff9a7d45edbf7d5298",
    "e47d5f4f097d6cff2e7d797f537d866e787d93ff9d7da0f2c27dadffe77dbaff0c7dc7ff317dd4ff567de1ff7b7deeffa07dfbffc57d08ffea7d15ff0f7d22ff347d2fff597d3cde7e7d49ffa37d56e000d868ff25d875ff4ad882ff6fd88fff94d89cffb9d8a9ffded8b6ff03d8c3ff28d8d0744dd8ddf272d8eaff97d8f7ffbcd804ffe1d811ff06d81eff2bd82bff50d8387175d845ff9ad85293bfd85fffe4d86cff09d879ff2ed886ff53d8935778d8a0ff9dd8adcbc2d8baffe7d8c7ff0cd8d4ff31d8e1ff",
    "56d8ee487bd8fbffa0d808ffc5d815ffead8222b0fd82fff34d83cff59d849ff7ed85645a3d863ff003375ff253382ff4a338fd86f339cff9433a9ccb933b6ffde33c3ff0333d0ff2833ddff4d33eaff7233f7ff973304d5bc3311ffe1331eff06332bff2b333810503345ff753352ff9a335fffbf336cffe43379ff093386ff2e3393ff5333a0ff7833adff9d33baffc233c768e733d4340c33e1ff3133eeff5633fbff7b330816a033154ec53322aeea332fff0f333cff343349ff593356ff7e3363ffa33370ff",
    "008e821b258e8fff4a8e9cec6f8ea9ff948eb6ffb98ec3ffde8ed0ff038eddff288eea224d8ef7ff728e04ff978e11d9bc8e1e49e18e2bac068e38ff2b8e45ff508e52b0758e5fff9a8e6cffbf8e79ffe48e8676098e93ff2e8ea0af538eadff788ebaff9d8ec7ffc28ed4ffe78ee1140c8eeeff318efbbb568e08ff7b8e15ffa08e22ffc58e2fffea8e3cff0f8e492a348e5603598e63ff7e8e70fca38e7dff00e98fff25e99cff4ae9a9ff6fe9b6ff94e9c3ffb9e9d0ffdee9ddff03e9eaff28e9f7ff4de904ff",
    "72e911ff97e91effbce92bffe1e9386d06e945ff2be9523250e95fff75e96cff9ae979b1bfe986ffe4e993ba09e9a0ff2ee9adff53e9baab78e9c7ff9de9d4ffc2e9e1ffe7e9eeff0ce9fbff31e908ff56e915ff7be922ffa0e92fffc5e93cffeae949bb0fe956ff34e963ff59e970ff7ee97d6ca3e98aff00449cff2544a9ff4a44b6ff6f44c3169444d0ffb944ddffde44eaff0344f7ff284404ff4d4411ff72441eff97442bffbc4438ffe14445ff064452ff2b445fff50446cff754479ff9a4486ffbf4493ff",
    "e444a0ff0944adff2e44baff5344c7ff7844d4ff9d44e17bc244ee58e744fb440c4408ff314415ff564422ff7b442fffa0443c29c54449ffea4456ff0f446350344470ff59447dff7e448affa34497ff009fa9ff259fb6ff4a9fc3ff6f9fd0ff949fddffb99feaffde9ff7ff039f04ff289f112c4d9f1eff729f2bff979f38ffbc9f455ae19f524f069f5f4d2b9f6cff509f79ff759f86fb9a9f93ffbf9fa0ffe49fadff099fbaff2e9fc7ff539fd4ff789fe1159d9feeffc29ffb4ae79f08ff0c9f15f4319f22ff",
    "569f2fff7b9f3cffa09f49c5c59f5627ea9f63ff0f9f70ff349f7dff599f8aff7e9f97ffa39fa41000fab6ff25fac3ff4afad0c46ffadd0194faeaffb9faf7ffdefa04ff03fa111628fa1eff4dfa2bff72fa38ff97fa45ffbcfa52ffe1fa5fff06fa6cff2bfa79ff50fa86ff75fa93ff9afaa0ffbffaadffe4fabaff09fac7ff2efad4ff53fae13978faeeff9dfafb2cc2fa08ffe7fa15ff0cfa22ff31fa2fff56fa3cff7bfa49ffa0fa56ffc5fa63ffeafa70ff0ffa7d3434fa8aff59fa97077efaa4ffa3fab12d",
    "0055c3102555d0ff4a55ddff6f55ea299455f7ffb955046dde55117e03551eff28552bff4d5538ff725545ff975552ffbc555fffe1556cff0655791d2b5586ff505593ff7555a0ff9a55adffbf55baffe455c7ff0955d4ff2e55e1ff5355ee417855fbff9d5508ffc25515ffe75522ff0c552fff31553cff565549ff7b5556ffa05563ffc55570ffea557dff0f558aff345597ff5955a4bb7e55b1ffa355beff00b0d0ff25b0ddff4ab0eaff6fb0f7ff94b004bcb9b011b7deb01eff03b02bff28b038ff4db045ff",
    "72b052ff97b05fffbcb06cffe1b0799606b086ff2bb093ff50b0a0ff75b0adff9ab0ba0fbfb0c7ffe4b0d4ff09b0e1ff2eb0eeff53b0fbff78b008ff9db015ffc2b022ffe7b02fff0cb03c0c31b049ff56b056ff7bb063ffa0b070ffc5b07dffeab08aff0fb097ff34b0a4ff59b0b1ff7eb0be8fa3b0cbff000bddd1250beaff4a0bf7ff6f0b04ff940b1119b90b1e06de0b2bff030b381a280b45ff4d0b52ff720b5fff970b6c4abc0b79f5e10b8611060b93ff2b0ba0ff500badff750bbaff9a0bc7ffbf0bd4ff",
    "e40be1ff090beeff2e0bfbff530b08ff780b15ff9d0b22acc20b2fffe70b3c1f0c0b49ff310b56ff560b63ff7b0b70a5a00b7dffc50b8affea0b97ff0f0ba402340bb1dc590bbeff7e0bcbfaa30bd8070066eaff2566f7ff4a6604316f6611ff94661e5ab9662bffde6638db036645ff286652ff4d665f0e72666cff976679ffbc6686fde16693ff0666a0ff2b66adff5066baff7566c7ff9a66d4ffbf66e1ffe466eeff0966fbff2e6608ff536615ff7866223e9d662fffc2663cffe76649ff0c6656ff316663ff",
    "566670ff7b667dffa0668affc56697ffea66a4ff0f66b1ff3466beff5966cb247e66d8ffa366e5c600c1f7ff25c104ff4ac111906fc11e9c94c12bffb9c138ffdec1459203c152ff28c15ffc4dc16cff72c1795e97c1862cbcc193ffe1c1a06d06c1addb2bc1baff50c1c7ff75c1d4ff9ac1e145bfc1eeffe4c1fbff09c108ff2ec115ff53c1224378c12fff9dc13cffc2c149ffe7c156ff0cc163e531c170ff56c17dff7bc18affa0c197f4c5c1a4ffeac1b1ff0fc1beff34c1cbff59c1d8ff7ec1e5ffa3c1f2ff",
    "001c04ff251c11ff4a1c1e4e6f1c2bff941c38ffb91c45ffde1c52ff031c5fff281c6cff4d1c79ff721c86ff971c93ffbc1ca0ffe11cadff061cbaff2b1cc7ff501cd4ff751ce1ff9a1ceeffbf1cfbffe41c08ff091c15ff2e1c22ff531c2f4a781c3c779d1c49ffc21c56c0e71c63ff0c1c70ff311c7dff561c8aff7b1c9743a01ca4ffc51cb1ffea1cbe810f1ccb5b341cd8ff591ce5ff7e1cf2ffa31cffff0077115125771eff4a772b2d6f7738ff947745ffb9775289de775fff03776cff287779ff4d778607",
    "727793ff9777a0ffbc77adffe177baa20677c75f2b77d4ff5077e19a7577eeff9a77fbffbf7708ffe47715ff097722ff2e772fff53773cff787749ff9d7756ffc27763ffe77770ff0c777dff31778aff567797ff7b77a4ffa077b17fc577beffea77cbff0f77d8953477e5ff5977f2ff7e77ffffa3770cff00d21eff25d22bff4ad238ff6fd245ff94d252ffb9d25fffded26cff03d279ff28d286ff4dd293ff72d2a0ff97d2ad5fbcd2baffe1d2c7ff06d2d4ff2bd2e1ff50d2eeff75d2fbff9ad208ffbfd215ff",
    "e4d222ff09d22fff2ed23cff53d249ff78d256ff9dd263ffc2d270ffe7d27d450cd28aab31d297ff56d2a4ff7bd2b1ffa0d2beffc5d2cbffead2d8ff0fd2e5ff34d2f2ff59d2ffff7ed20cffa3d219b4002d2bff252d38ff4a2d45ff6f2d52ff942d5fd2b92d6cffde2d79ff032d86ff282d93ff4d2da0ff722dadff972dbaffbc2dc7ffe12dd4ff062de1ff2b2deeff502dfbff752d08ff9a2d15ffbf2d22ffe42d2fff092d3c152e2d4980532d561e782d63ff9d2d70ffc22d7dffe72d8a750c2d97ff312da4ff",
    "562db1ff7b2dbea4a02dcb1fc52dd8ffea2de5ff0f2df2ff342dffff592d0cff7e2d1977a32d268e008838ff258845ff4a88525f6f885f3794886cffb98879ffde888651038893ff2888a0ff4d88adff7288baff9788c7ffbc88d4ffe188e1ff0688eeff2b88fbff508808ff758815a69a8822ffbf882fffe4883cff098849ff2e8856ff538863ff788870ff9d887dffc2888affe78897120c88a4ff3188b1ff5688beff7b88cbffa088d8a5c588e5ffea88f2ff0f88ffff34880cff598819ff7e882669a38833ff",
    "00e345ff25e352ff4ae35fff6fe36cff94e379ffb9e386ffdee393ff03e3a0bd28e3ad7e4de3ba8e72e3c7ff97e3d45ebce3e1ffe1e3eeff06e3fbff2be308ff50e315ff75e322ff9ae32fffbfe33cffe4e349ff09e356ff2ee363ff53e370ff78e37dff9de38affc2e397ffe7e3a4ff0ce3b1ff31e3beff56e3cb4c7be3d8ffa0e3e50fc5e3f2ffeae3ffff0fe30c3734e319ff59e326ff7ee333ffa3e340ff003e52ff253e5f914a3e6cff6f3e79ff943e86cab93e93ffde3ea0ff033eadff283ebaff4d3ec7ff",
    "723ed4ff973ee1b8bc3eeeffe13efbff063e08ff2b3e158b503e22ff753e2fff9a3e3cffbf3e49ffe43e56ff093e63a92e3e70ff533e7dff783e8aff9d3e9708c23ea4cde73eb1ff0c3ebeff313ecbff563ed8ff7b3ee549a03ef266c53effffea3e0cff0f3e1999343e26ff593e33ff7e3e40ffa33e4dff00995f0425996cff4a9979506f9986ff949993ffb999a0ffde99ade30399baff2899c7ff4d99d4c27299e1ff9799ee0ebc99fbd6e19908ff069915ff2b9922ff50992fff75993cff9a9949ffbf9956ff",
    "e49963ff099970ff2e997dff53998afa789997ff9d99a4ffc299b1d4e799beff0c99cb793199d8ff5699e5007b99f2ffa099ffffc5990cffea9919680f9926ff349933ff599940ff7e994dffa3995aff00f46cff25f479c24af486ff6ff493ff94f4a0ffb9f4adffdef4ba9203f4c7ff28f4d4ff4df4e1ff72f4eeff97f4fbffbcf408ffe1f415ff06f422782bf42fff50f43cff75f449ff9af456ffbff463ffe4f470ff09f47dff2ef48aff53f497ff78f4a4ff9df4b1ffc2f4beffe7f4cbff0cf4d8ff31f4e5d1",
    "56f4f2ff7bf4ffffa0f40cffc5f419ffeaf426ff0ff433ff34f440ff59f44dff7ef45a2ea3f467b9004f79ff254f86784a4f93ff6f4fa0ff944fadceb94fba27de4fc7a9034fd4ff284fe1ff4d4feeff724ffbff974f08e6bc4f15ffe14f22ff064f2fff2b4f3cff504f49ff754f56ff9a4f63ffbf4f70ffe44f7dff094f8aff2e4f97ff534fa4ff784fb1ff9d4fbeffc24fcbffe74fd8ff0c4fe5ff314ff2ff564fffff7b4f0cffa04f19ffc54f26ffea4f33ff0f4f40ff344f4dff594f5a237e4f67ffa34f74ff",
    "00aa86ff25aa933e4aaaa0ff6faaadff94aabaffb9aac7ffdeaad4ff03aae1ff28aaee034daafbff72aa08ff97aa15ffbcaa22ffe1aa2fff06aa3cff2baa49ff50aa569b75aa63ff9aaa70ffbfaa7dc8e4aa8aff09aa97ff2eaaa4ff53aab1ff78aabe309daacb11c2aad81de7aae5ff0caaf2ff31aaffff56aa0cff7baa1938a0aa264bc5aa33ffeaaa40ff0faa4dac34aa5aff59aa67147eaa74efa3aa81ff000593ff2505a0ff4a05adff6f05baff9405c7ffb905d4ffde05e1ff0305eeff2805fbff4d0508ff",
    "720515c2970522ffbc052fffe1053cff060549ff2b0556ff500563ff750570ff9a057dffbf058affe40597ff0905a43e2e05b1ff5305beff7805cbff9d05d8ffc205e5ffe705f2ff0c05ffff31050cff560519ff7b0526ffa00533ffc50540a9ea054dff0f055aff340567ff590574857e0581ffa3058eff0060a0ff2560adff4a60baff6f60c7ff9460d441b960e1ffde60eeff0360fbff286008ff4d6015ff726022ff97602fffbc603cffe16049ff066056252b6063ff5060708275607d8d9a608affbf6097ff",
    "e460a4ff0960b17c2e60beff5360cb617860d8ff9d60e55bc260f238e760ffeb0c600cff316019ff566026ab7b603312a06040ffc5604dffea605aff0f6067ff3460742d596081777e608effa3609bff00bbadff25bbbaff4abbc7bd6fbbd4ff94bbe141b9bbeededebbfbff03bb08ff28bb15d24dbb22ff72bb2fff97bb3c2bbcbb49ffe1bb569106bb63912bbb709750bb7dc175bb8aff9abb97ffbfbba4ffe4bbb1ff09bbbeff2ebbcbff53bbd8ff78bbe5769dbbf2ffc2bbffffe7bb0cff0cbb19ff31bb26ff",
    "56bb33ff7bbb40ffa0bb4de0c5bb5affeabb67ff0fbb74ff34bb81ff59bb8e8b7ebb9b5da3bba8ff0016baff2516c7ff4a16d4346f16e1ff9416eeffb916fb97de1608ff031615de281622b44d162fff72163cff971649ffbc1656ffe11663ff061670f72b167dff50168aff751697ff9a16a4ffbf16b1a4e416bef20916cbff2e16d8ff5316e5ff7816f2ff9d16ffffc2160cffe71619ff0c1626ff311633ff5616402a7b164d17a0165affc516676dea1674ff0f1681ff34168eff59169bff7e16a8ffa316b5ff",
    "0071c7ff2571d4ff4a71e1ff6f71ee659471fbffb97108ffde7115ff037122ff28712fff4d713cff727149ff977156ffbc716344e171706f06717dff2b718ab1507197ff7571a4ff9a71b152bf71beffe471cbff0971d8ff2e71e5ff5371f2ff7871ffff9d710cffc2711935e771261a0c7133ff317140ff56714dff7b715affa07167ffc57174ffea7181ff0f718e0134719bff5971a89c7e71b5ffa371c2ff00ccd4ff25cce1ff4acceeff6fccfbb494cc08ffb9cc15ffdecc22ff03cc2fff28cc3cff4dcc49ff",
    "72cc56ff97cc63e1bccc70ffe1cc7dff06cc8aff2bcc97ff50cca4ff75ccb1179accbeffbfcccbffe4ccd8ff09cce5ff2eccf2ff53ccffbd78cc0cb99dcc19ffc2cc26ffe7cc33ff0ccc405831cc4dff56cc5aff7bcc67ffa0cc744ec5cc81ffeacc8eff0fcc9bff34cca8ff59ccb5ff7eccc2ffa3cccfff0027e1ff2527eeff4a27fbff6f27089c942715ffb92722ffde272fff03273cff282749ff4d2756ff722763ff97277028bc277d55e1278aff062797ff2b27a4d75027b1ff7527be129a27cbffbf27d8ff",
    "e427e5ff0927f2bc2e27ffff53270cff782719ee9d2726ffc22733ffe72740ff0c274dff31275aff562767ff7b2774ffa02781ffc5278effea279b6d0f27a8263427b5ff5927c2ff7e27cfffa327dcff0082eeff2582fbff4a8208ff6f821560948222ffb9822f74de823cff038249ff288256ff4d8263ff728270ff97827dffbc828a16e18297ff0682a4ff2b82b1ff5082beff7582cbff9a82d8ffbf82e5ffe482f2ff0982ffff2e820cee538219f3788226ff9d8233ffc28240d6e7824dff0c825aff318267ff",
    "568274ff7b8281ffa0828effc5829beaea82a8ff0f82b5ff3482c2ff5982cfff7e82dcffa382e9ff00ddfbff25dd08ff4add15ff6fdd22ff94dd2fffb9dd3cffdedd49ff03dd56ff28dd63dc4ddd70ff72dd7dff97dd8affbcdd97ffe1dda4ff06ddb10e2bddbeff50ddcbff75ddd8009adde5ffbfddf2ffe4ddff8109dd0c4f2edd19ff53dd26ff78dd33519ddd40ffc2dd4dffe7dd5aff0cdd67ff31dd74ff56dd81ff7bdd8effa0dd9bffc5dda8ffeaddb5fb0fddc2ff34ddcfff59dddcff7edde9ffa3ddf6ff",
    "003808ff253815ff4a3822ff6f382f1994383cffb93849ffde3856ff033863b5283870ff4d387dff72388aff973897ffbc38a4ffe138b1ff0638beff2b38cbff5038d81d7538e5e49a38f2ffbf38ffffe4380cd3093819ff2e3826ff533833ff783840ff9d384dffc2385affe73867520c3874ff313881ff56388ee27b389bffa038a8ffc538b5ffea38c2ff0f38cfff3438dcff5938e9ff7e38f6ffa33803ff009315ff259322ff4a932fff6f933cff949349ffb99356ffde9363ff039370ff28937dff4d938ae5",
    "729397ff9793a4ffbc93b1ffe193beff0693cb712b93d8c25093e5ff7593f2ff9a93ffffbf930cffe49319fa099326ff2e9333ff539340ff78934dff9d935affc29367ffe79374ff0c9381ff31938eff56939bff7b93a8ffa093b5ffc593c2faea93cfee0f93dc0b3493e99e5993f6ff7e9303ffa39310ff00ee22d425ee2fff4aee3cff6fee49e194ee56e5b9ee63ffdeee70f803ee7dff28ee8aff4dee97f172eea4ff97eeb1ffbceebeffe1eecbff06eed8ff2beee5ff50eef2ff75eeffff9aee0cfdbfee19a9",
    "e4ee26ff09ee33ff2eee40ff53ee4dff78ee5aff9dee67ffc2ee74ffe7ee81ff0cee8e9d31ee9bff56eea8ff7beeb5ffa0eec2ffc5eecfb6eaeedcff0feee9ff34eef6ff59ee03787eee10ffa3ee1dff00492fff25493cff4a4949ff6f4956ff944963ffb94970ffde497dff03498a12284997ff4d49a43f7249b1ff9749beffbc49cbffe149d8ff0649e5ff2b49f27e5049ffff75490cff9a4919ffbf4926ffe44933ff094940ff2e494dff53495aff784967ff9d4974ffc24981ffe7498eff0c499bff3149a841",
    "5649b5ff7b49c2ffa049cfffc549dcffea49e9ff0f49f6ff344903ff594910ff7e491dffa3492aff00a43ce725a449bb4aa456ff6fa463ff94a470ffb9a47dffdea48aa203a497ff28a4a4ff4da4b1ff72a4beff97a4cb67bca4d8ffe1a4e5ff06a4f2ff2ba4ffff50a40cff75a419ff9aa426ffbfa433ffe4a440ff09a44dff2ea45ac053a4670d78a474ff9da4819bc2a48effe7a49bff0ca4a8ff31a4b5b256a4c2ff7ba4cfffa0a4dcffc5a4e9ffeaa4f6ff0fa403ff34a410ff59a41d657ea42affa3a437ff",
    "00ff49aa25ff56d64aff63ff6fff70ff94ff7dffb9ff8affdeff97ff03ffa4ff28ffb1ff4dffbeff72ffcb4c97ffd8ffbcffe5aae1fff21f06ffffff2bff0cff50ff19ff75ff26ff9aff33ffbfff40ffe4ff4dff09ff5aff2eff67ff53ff74ff78ff81ff9dff8effc2ff9bffe7ffa8b50cffb5ff31ffc2ff56ffcfb77bffdcaba0ffe9ffc5fff6ffeaff03ff0fff10ff34ff1dff59ff2aff7eff37ffa3ff44ff",
    "=",
    "504c494d010400220027001200100000299a6c32962833452335d84a373f6437bbc2393fc2aad5dfabd145af41c6af8958b3342eb4d29cb73572b78fc5ede5ea222bbef44ffd666f9bf22fbde44d996669002222ce4444e9766eab222aaf446999666e0011188c0055cf5757881c388c075cef5558002244dd446699666aabf44ddd469e99662b00114aaa066fee6622ba144eee6669ee622a007c0aae0767ee011aaa00ecee666eeef11a00000aae556eee0c1aaf077aee556eaa11aa00333ccc57577833c8cc33",
    "7ccf55f78f333c004466996666bb2f44dd4469ef666feb244d00f055ee5f51ac11caaf055eef5511aaff0a0055557f5538883f38cf55777f53388c333c000667ee0a11aa000aee666e7c111aca500e00557f7cf33888333ccf55f7c833388cf3f700f66f9c2224ddf4699966f9bb2244dd46e9005557881138ac0055fe555188f338ac0557006662fef444fe466699662bbf4449ddf66e0050ca8a00ccee555eee01acaa000aee557e0022cbbb444dd966699b227edd444f9e766900c1f1aa00faee767eab111aaa",
    "000eee766a00333388335cfc55578833388f555c7f5558002244dd446699666bbbfefdde666ee9662e0011cfac005eee551aaa000aaf055eee511a00000ace55ee7e0311aa000ace55e7fac11a00f070ee5575fa111aaa000eee75feac111a00353ccf555778c3388c335ccf555788333c004e66ef6669bb2244dd466999666bbb24fd000f55ee5f518810cacc055fff551fca50fc006666e9642bbbe4dddd7699e9424add4eed005575ff1318fccccccc55577811388800770066f69d242bbd444d996669db22ff",
    "dd4469006667eb111aaf056eee666eaf114aaaf0ee005557883338cc055f7f5578887338cc555700666abb2444fe666f99662bbb444ddef669005018880000cc75577f511c8a000cce555700222fbd0444d966f99b222bdf446999666d001111aa005eee755eaa111aaa005eeef75a00333888335ccf55578833388c55577f55c8002244ddf6f6996762bb2444dd676999622b003333cc55577f53388833cccc55577c338800",
    "END",
    "SNAP 33 2 19 320 4",
    "502800ff785028ff50c850ffc8f0a04ec850c8ff00f0a0ffa0a050ffc80050fff028c8ffa07878ffc85028ff28c8c8ff00a028ff0050c8ffc85078ff002850b378a078ffa00028ff005000ff2800c8ff282850ff00c8c8ff5078c8ffa02878ff50f0a0ff50a000ff50f028d80078007200a0508aa02850ff50c878ff505078ff7800a0ffa0a050ff782800ffc87850bcc828c8ff002800ff78a0f098287828e32828c8ff78c8a0ffc87828fff02828ad78a0f0ff0050a0ff505000ff285028fb7800c87d5078a0ba",
    "a0c8c8ffa028f06f000050ff78a0f0ffa0c8a0ff78c8f0ffc8c8c860c8c828fc78a078ff78f0f0ffc8c878ff787828efa0a078ff50c878ff2850a0ffa0f028d9",
    "=",
    "504c494d010400130001000a0010000000786414008c5096aa51515b58dbcb68658072f19d74546a78be517f6e33808aa7866e7a88908193b6779595c7aa709099f47fc03ab12e86dc50",
    "END",
    "SNAP 39 7 320 320 8",
    "fdfd3bfffcfa99fffcfd94fff8ffebfffcfe15fffafe08fffffc42fffcf902fff9f8d3fffff84d3df8fc26fff8fab2fffafc8afffdfdbbfffbfe5bfffdf8b8fffdfb2efffaff24fffdfcf34ff8fc69fffaf894fffcf802fff8fcbafffffba8fffefda4fffafc9afff8fcd365f8f84dfffeff03fffbfec535fdfe44fffafb80fffbfa39fff9fcbafff8fb3dfffcfbf5fffefdfafffff828fff9f8dbfffefa95a0f9fe6753f8fc7358faf975fff9fe6cfff8f8bcfff8f8b0fffefa5bfffafe2b6afbfbe0ffffffdbff",
    "fef8befffaf94ffff8fe19fff8fb51fffbfd97fffdfdba99fbfd60fff8fc97fffaf994fffafb03e1fbf811fffff9a5fffbf93afffefc55fffbfee6f9fffcf4fef9ff2ffffefeb3fffff84fd1fdfce0fffdfa19fff9fd1c2bfdf96bfff8fcbee1fdfdef5bfafc23fffdfa00fffef84cfffbf8c6cffff8d0ffffff2cfffefef2fff8fa5919fdf971fffafd2dfff8fa03fffefa9bfffaf9d0fffaf82a2ffafa18fff8f97cb4fdfeb6f3f8f80decfffe26d0f9fab9fffafa93c5faff30fff8ff69fff8f8c2fff8ff80ff",
    "fefa69fffbff08fff8f90cfffffb31a9fcfbb2fffffb18ffffff0bfffffee55ef8fd90fff8f97ffffff947fffefeea93feff16fff8fdae56f8fdb8fffcfa57fffbff6ffffff8f2fff9fedbaefcf89cfffefcfbfffefea0fffefaddfffffff9fffdffc218f8fefcfff9fdd0fffefabefffff85ffffffde5fff9f85dfffcfbaefffafc7efffcfdb6fffbff49fffcfcf9fffffdc0fffbffa609fcfe6ffffafc75fffeffc0ffffff820efaf950fffdfee7fffcf825fffffb3efff9f877fffcffbdfffbfcf15bfffc64ff",
    "fbff70fffdf8e58bfcfedffffff94cfffbfdc4fff9f9f2fff8fd0afff8ff95ffffffe3fffcfee6fffdfdb629fefa0409fffbbffff9fef6fffafe4bfff8fe6cfffcfb72fffffab9f7fdfbbafffcfc4ffffffe19ebf9f89efffaf96ffffbfa51fffbfa39d0f8fe8ffff8f8fdfffafac4fffefef9ffffff58d8fef9ddfffffb190efff86efffffe10fffdfe3afff8f871fffdf99decf9fec0fffafe4357fefaaffffff87efffbfe6fa5fcfad032fcfe564bfdfb7ffff8fc1252fdfb77fffcf9eefffdf88efffbf8acff",
    "fef874fff8ff6cfffdf9c9fffcfa0bfffaf9b37df9fcaefff8fd9b36fcff05bffaf89dfffbfc1afffff837fff9fc6835f9f8d0fffafe53fffcfd92fffffcaefffffa1e6cffff3efffbffa09ffbfdc5fffbf8f5c4fefdb4fff9fd52fffafa44fffafec6fff8fbbafffbff3ffff8fb6dfffcf842e0fffb30fffef873fffffd46fff9fd39fff9fbfcfff9fd62fffefb6bfff9fdc8fffff887fffbf91efffefbdd7bfcfe75c4fafa7afffffae7fffefe18fffcfa08fff8fd78fff9ffcbfffaf91bfffafc55fff9fe7fff",
    "f8ff97fffafb16fffdfdbc9bfbfa75fffdffc9fffafc0634fbfd7efffdfd7dfffafe1cfffcff61ffffff9cebfbfb67fff8fa58fff8fab1fffcf857fff9f984fffdfc0dfff9f934fff9fb88fff9fc60fff8f833fffdfd5cfffbff43ff",
    "=",
    "504c494d010800270007002800200000fafb8cfafc24fafd95fbf99cfbfa33fbfb1afbfb53fbfb7dfbfb83fbfc0afbfc6cfbfcb3fbfd62fbfdc3fcfa02fcfa5cfcfa74fcfb4dfcfbadfcfbbbfcfbd2fcfc14fcfc3cfcfecbfdfcdbfdfd2cfdfd44fdfda3fdfde4fdfdf4fdfdfafdfeed1603021f15091a0e1414010b00130f1319011e0a020e131b1b031f110e1d1a071613161d1e191800131717100a130b0f1b1c181311050602140c020201151b16061c1d190b0a1c05180a0d1e010e11001414191d1f10190e",
    "031418051b1305111312190a0d070a0909100b05091d00071a1d151c13060a001d1c031e1b181e1e1e14130f1c0f12070b111e0d1e0a100d1e061c011610131e0c0a1d18110d1d0009021c1c1d1d131d110a1013131119030a060f001e0d1e10181d0a1516101b0d0d12071b1d1707000b101f0012100a170918121f1a0305041c140602121b160d0d1d0b061a0d13160a0f19101a161e000c0a1708051f02071c050907170506070215141017170707050c1b0a0f0b06080904000c040f1a00",
    "END",
};
/* TS_SNAPSHOT_END */

static unsigned char snap_rgba[70000];
static unsigned char snap_expected[80000];

static void test_snapshots(void)
{
    static char rgba_hex[140000];
    static char blob_hex[160000];
    size_t i = 0;
    size_t n = sizeof(kSnapshots) / sizeof(kSnapshots[0]);
    int snapshot_index = -1;

    while (i < n) {
        long width;
        long height;
        int max_w;
        int max_h;
        int depth;
        char what[64];
        long rgba_len;
        long want;
        long length = -1;
        platinum_image image;
        int status;

        snapshot_index++;
        if (sscanf(kSnapshots[i], "SNAP %ld %ld %d %d %d", &width, &height,
                   &max_w, &max_h, &depth) != 5) {
            check(0, "snapshot header");
            break;
        }
        i++;
        rgba_hex[0] = '\0';
        while (i < n && strcmp(kSnapshots[i], "=") != 0)
            strcat(rgba_hex, kSnapshots[i++]);
        i++;
        blob_hex[0] = '\0';
        while (i < n && strcmp(kSnapshots[i], "END") != 0)
            strcat(blob_hex, kSnapshots[i++]);
        i++;

        rgba_len = unhex(rgba_hex, snap_rgba);
        want = unhex(blob_hex, snap_expected);
        label(what, "snapshot", (int)snapshot_index);
        strcat(what, " matches the TypeScript converter");
        check(rgba_len == width * height * 4, what);
        status = platinum_image_encode(snap_rgba, width, height, max_w, max_h, depth,
                                       out, (long)sizeof(out), &length);
        check(status == PLATINUM_ENCODE_OK && length == want &&
                  memcmp(out, snap_expected, (size_t)want) == 0,
              what);
        check(platinum_image_parse(out, length, &image) == PLATINUM_IMAGE_OK, what);
    }
}

static void test_colour_cap(void)
{
    static unsigned char seventeen[17 * 4];
    static unsigned char sixteen[16 * 4];
    long length = -1;
    platinum_image image;
    int i;

    for (i = 0; i < 17; i++) {
        seventeen[i * 4] = (unsigned char)(i * 15);
        seventeen[i * 4 + 1] = 0;
        seventeen[i * 4 + 2] = 0;
        seventeen[i * 4 + 3] = 255;
    }
    /* More colours than the palette holds: the median cut reduces them to at most 16. */
    check(platinum_image_encode(seventeen, 17, 1, 320, 320, 4, out,
                                (long)sizeof(out), &length) == PLATINUM_ENCODE_OK &&
              length > 0 && platinum_image_parse(out, length, &image) ==
                                PLATINUM_IMAGE_OK && image.colours <= 16,
          "17 colours at depth 4 are reduced by the median cut to at most 16");
    check(platinum_image_encode(seventeen, 17, 1, 320, 320, 8, out,
                                (long)sizeof(out), &length) == PLATINUM_ENCODE_OK,
          "17 colours at depth 8 fits");

    for (i = 0; i < 16; i++) {
        sixteen[i * 4] = (unsigned char)(i * 15);
        sixteen[i * 4 + 1] = 0;
        sixteen[i * 4 + 2] = 0;
        sixteen[i * 4 + 3] = 255;
    }
    length = -1;
    check(platinum_image_encode(sixteen, 16, 1, 320, 320, 4, out,
                                (long)sizeof(out), &length) == PLATINUM_ENCODE_OK &&
              length == 16 + 48 + 8 &&
              platinum_image_parse(out, length, &image) == PLATINUM_IMAGE_OK &&
              image.colours == 16,
          "16 colours at depth 4 fits exactly");
}

static void test_sizes(void)
{
    long length = -1;
    int x;
    int y;

    for (y = 0; y < 320; y++) {
        for (x = 0; x < 320; x++) {
            unsigned char *p = big + (y * 320 + x) * 4;

            p[0] = (unsigned char)((x & 3) * 85);
            p[1] = (unsigned char)((y & 3) * 85);
            p[2] = 0;
            p[3] = 255;
        }
    }
    check(platinum_image_encode(big, 320, 320, 320, 320, 4, out, (long)sizeof(out),
                                &length) == PLATINUM_ENCODE_OK &&
              length == 51264,
          "320 x 320 at depth 4 with 16 colours is exactly 51,264 bytes");

    for (y = 0; y < 320; y++) {
        for (x = 0; x < 320; x++) {
            unsigned char *p = big + (y * 320 + x) * 4;

            p[0] = (unsigned char)((x & 15) * 16);
            p[1] = (unsigned char)((y & 15) * 16);
            p[2] = 0;
            p[3] = 255;
        }
    }
    length = -1;
    check(platinum_image_encode(big, 320, 320, 320, 320, 8, out, (long)sizeof(out),
                                &length) == PLATINUM_ENCODE_TOO_LARGE && length == 0,
          "320 x 320 at depth 8 with 256 colours is over the limit");

}

static void test_buffer(void)
{
    long length = -1;
    long want;
    static const char *vector0 =
        "504c494d0104000200020002000400000000ff00ff00ff0000ffffff21000300";
    unsigned char rgba[16];
    int status;

    want = unhex(vector0, expected);
    memset(rgba, 0, sizeof(rgba));
    rgba[0] = 0xff; rgba[3] = 0xff;
    rgba[4] = 0x00; rgba[5] = 0xff; rgba[6] = 0x00; rgba[7] = 0xff;
    rgba[8] = 0x00; rgba[9] = 0x00; rgba[10] = 0xff; rgba[11] = 0xff;
    rgba[12] = 0xff; rgba[13] = 0xff; rgba[14] = 0xff; rgba[15] = 0xff;

    status = platinum_image_encode(rgba, 2, 2, 320, 320, 4, out, want - 1, &length);
    check(status == PLATINUM_ENCODE_BUFFER_SMALL && length == want,
          "a short buffer reports the length the blob needs");
    length = -1;
    status = platinum_image_encode(rgba, 2, 2, 320, 320, 4, out, want, &length);
    check(status == PLATINUM_ENCODE_OK && length == want &&
              memcmp(out, expected, (size_t)want) == 0,
          "a buffer of exactly the blob length is enough");
}

static void test_refusals(void)
{
    static unsigned char one[4];
    long length = 99;

    one[0] = 0; one[1] = 0; one[2] = 0; one[3] = 255;
    check(platinum_image_encode(NULL, 1, 1, 320, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG && length == 0,
          "NULL pixels");
    check(platinum_image_encode(one, 1, 1, 320, 320, 4, NULL, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "NULL output");
    check(platinum_image_encode(one, 1, 1, 320, 320, 4, out, 80000L, NULL) ==
              PLATINUM_ENCODE_BAD_ARG,
          "NULL length");
    check(platinum_image_encode(one, 0, 1, 320, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "zero width");
    check(platinum_image_encode(one, 1, 0, 320, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "zero height");
    check(platinum_image_encode(one, 3000, 4001, 320, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "more than 12 million source pixels, refused before any read");
    check(platinum_image_encode(one, 1, 1, 320, 320, 5, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "depth 5");
    check(platinum_image_encode(one, 1, 1, 320, 320, 1, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "depth 1");
    check(platinum_image_encode(one, 1, 1, 0, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "max width 0");
    check(platinum_image_encode(one, 1, 1, 321, 320, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "max width 321");
    check(platinum_image_encode(one, 1, 1, 320, 0, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "max height 0");
    check(platinum_image_encode(one, 1, 1, 320, 321, 4, out, 80000L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "max height 321");
    check(platinum_image_encode(one, 1, 1, 320, 320, 4, out, -1L, &length) ==
              PLATINUM_ENCODE_BAD_ARG,
          "negative buffer size");
}

int main(void)
{
    test_vectors();
    test_hand_derived();
    test_flatten_boundary();
    test_median_hand();
    test_snapshots();
    test_colour_cap();
    test_limit();
    test_sizes();
    test_buffer();
    test_refusals();
    if (failures != 0) {
        printf("test_image_encode: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_image_encode: all %d checks passed\n", checks);
    return 0;
}
