/* The image route's CDN fetch, over HTTPS to a local server with a test certificate. */
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <microhttpd.h>
#include <wolfram/xrpc.h>

#include "../plim/plim.h"
#include "image_cdn.h"
#include "image_route.h"
#include "image_source_wolfram.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *served_ref = "avatar/plain/did:plc:abc123/bafyrei2x2@png";

static unsigned char *png_bytes;
static size_t png_len;

static char *
read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    *len = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(*len);
    if (buf != NULL && fread(buf, 1, *len, f) != *len) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

static char *
read_text(const char *path)
{
    size_t n;
    char *buf = read_all(path, &n);
    char *out;
    if (buf == NULL) return NULL;
    out = malloc(n + 1);
    if (out != NULL) {
        memcpy(out, buf, n);
        out[n] = '\0';
    }
    free(buf);
    return out;
}

static enum MHD_Result
serve(void *cls, struct MHD_Connection *conn, const char *url, const char *method,
      const char *version, const char *upload, size_t *upload_size, void **con_cls)
{
    struct MHD_Response *resp;
    enum MHD_Result ok;
    char want[512];
    (void) cls; (void) method; (void) version; (void) upload; (void) con_cls;
    if (*upload_size != 0) {
        *upload_size = 0;
        return MHD_YES;
    }
    sprintf(want, "/img/%s", served_ref);
    if (strcmp(url, want) == 0) {
        resp = MHD_create_response_from_buffer(png_len, png_bytes, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(resp, "content-type", "image/png");
        ok = MHD_queue_response(conn, MHD_HTTP_OK, resp);
    } else {
        static const char nf[] = "not here";
        resp = MHD_create_response_from_buffer(strlen(nf), (void *) nf, MHD_RESPMEM_PERSISTENT);
        ok = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, resp);
    }
    MHD_destroy_response(resp);
    return ok;
}

int
main(void)
{
    const char *tmp = getenv("TMPDIR");
    char dir[256];
    char cmd[1024];
    char cert_path[320];
    char key_path[320];
    char *cert;
    char *key;
    struct MHD_Daemon *d;
    wf_xrpc_client *client;
    platinum_cdn cdn;
    char base[64];
    unsigned char *blob = NULL;
    long blob_len = 0;
    unsigned char *direct = NULL;
    long direct_len = 0;
    platinum_rgba src;
    int st;

    if (tmp == NULL || tmp[0] == '\0') tmp = "/tmp";
    sprintf(dir, "%s/platinum-cdn-%ld", tmp, (long) getpid());
    mkdir(dir, 0700);
    sprintf(cert_path, "%s/cert.pem", dir);
    sprintf(key_path, "%s/key.pem", dir);
    sprintf(cmd, "openssl req -x509 -newkey rsa:2048 -nodes -keyout %s -out %s -days 1 "
                 "-subj /CN=localhost -addext subjectAltName=DNS:localhost,IP:127.0.0.1 >/dev/null 2>&1",
            key_path, cert_path);
    if (system(cmd) != 0) {
        fprintf(stderr, "openssl could not make the test certificate\n");
        return 1;
    }
    cert = read_text(cert_path);
    key = read_text(key_path);
    CHECK(cert != NULL && key != NULL);
    png_bytes = (unsigned char *) read_all("fixtures/rgba-2x2.png", &png_len);
    CHECK(png_bytes != NULL);
    if (cert == NULL || key == NULL || png_bytes == NULL) return 1;

    d = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_TLS, 0, NULL, NULL, &serve, NULL,
                         MHD_OPTION_HTTPS_MEM_KEY, key,
                         MHD_OPTION_HTTPS_MEM_CERT, cert,
                         MHD_OPTION_END);
    CHECK(d != NULL);
    if (d == NULL) return 1;
    sprintf(base, "https://localhost:%d", (int) MHD_get_daemon_info(d, MHD_DAEMON_INFO_BIND_PORT)->port);

    client = wf_xrpc_client_new(base);
    CHECK(client != NULL);
    wf_xrpc_client_set_ca_bundle(client, cert_path);
    cdn.client = client;
    cdn.base = base;

    /* The route fetches the source over HTTPS and answers with the encoder's blob. */
    st = platinum_route_image(&cdn, platinum_image_source_cdn, served_ref, "16", NULL, &blob, &blob_len);
    CHECK(st == 200);
    CHECK(blob != NULL && blob_len > 0);
    {
        platinum_image_result r = platinum_image_from_bytes(png_bytes, png_len, &src);
        CHECK(r == PLATINUM_IMAGE_OK);
        if (r == PLATINUM_IMAGE_OK) {
            CHECK(plim_encode(src.pixels, src.width, src.height, 16, 320, 16, &direct, &direct_len) == PLIM_OK);
            CHECK(direct != NULL && direct_len == blob_len && blob != NULL && memcmp(blob, direct, (size_t) blob_len) == 0);
            free(src.pixels);
        }
    }
    free(blob);
    free(direct);

    /* A reference the CDN does not have is a failed source: 502. */
    blob = NULL;
    st = platinum_route_image(&cdn, platinum_image_source_cdn,
                              "avatar/plain/did:plc:abc123/missing@png", NULL, NULL, &blob, &blob_len);
    CHECK(st == 502 && blob == NULL);

    /* A plain-HTTP base is refused: the CDN is HTTPS only. */
    {
        wf_xrpc_client *plain = wf_xrpc_client_new("http://127.0.0.1:9");
        platinum_cdn insecure;
        insecure.client = plain;
        insecure.base = "http://127.0.0.1:9";
        blob = NULL;
        st = platinum_route_image(&insecure, platinum_image_source_cdn, served_ref, NULL, NULL, &blob, &blob_len);
        CHECK(st == 502 && blob == NULL);
        wf_xrpc_client_free(plain);
    }

    wf_xrpc_client_free(client);
    MHD_stop_daemon(d);
    free(cert);
    free(key);
    free(png_bytes);
    remove(cert_path);
    remove(key_path);
    rmdir(dir);

    if (failures) {
        fprintf(stderr, "test_image_cdn: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("test_image_cdn: %d checks passed\n", checks);
    return 0;
}
