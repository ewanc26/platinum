/*
 * test_atproto_identity.c -- host tests for native DID/handle resolution.
 *
 * The PLC document fixture is a real public response for bsky.app, copied
 * without the parts this parser does not read. The session and error fixtures
 * are synthetic and contain no account credentials.
 *
 * The tests run from the repository root, which is where CI invokes them.
 */

#include <stdio.h>
#include <string.h>

#include "atproto_identity.h"
#include "atproto_url.h"

#define FIXTURE_DIR "macos9/test/fixtures/atproto/"

static int failures;
static int checks;

static void check_status(wf_status got, wf_status want, const char *what)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s (got %d, want %d)\n", what, (int)got, (int)want);
    }
}

static void check_str(const char *got, const char *want, const char *what)
{
    checks++;
    if (got == NULL || strcmp(got, want) != 0) {
        failures++;
        printf("FAIL: %s (got \"%s\", want \"%s\")\n", what,
               got != NULL ? got : "(null)", want);
    }
}

static int load_fixture(const char *name, char *buffer, size_t capacity)
{
    char path[256];
    FILE *file;
    size_t length;

    strcpy(path, FIXTURE_DIR);
    strcat(path, name);
    file = fopen(path, "rb");
    if (file == NULL) {
        printf("FAIL: cannot open %s (run from the repository root)\n", path);
        failures++;
        checks++;
        return 0;
    }
    length = fread(buffer, 1, capacity - 1, file);
    fclose(file);
    buffer[length] = '\0';
    return 1;
}

static void test_document_urls(void)
{
    char out[PLATINUM_ATPROTO_URL_MAX];

    check_status(platinum_atproto_did_document_url(
                     "did:plc:z72i7hdynmk6r22z27h6tvur", out, sizeof(out)),
                 WF_OK, "plc document URL");
    check_str(out, "https://plc.directory/did:plc:z72i7hdynmk6r22z27h6tvur",
              "plc document URL value");

    check_status(platinum_atproto_did_document_url("did:web:example.com", out,
                                                   sizeof(out)),
                 WF_OK, "web document URL");
    check_str(out, "https://example.com/.well-known/did.json",
              "web document URL value");

    check_status(platinum_atproto_did_document_url(
                     "did:web:example.com:user:alice", out, sizeof(out)),
                 WF_OK, "web path document URL");
    check_str(out, "https://example.com/user/alice/did.json",
              "web path document URL value");

    check_status(platinum_atproto_did_document_url(
                     "did:web:example.com%3A8443", out, sizeof(out)),
                 WF_OK, "web port document URL");
    check_str(out, "https://example.com:8443/.well-known/did.json",
              "web port document URL value");

    check_status(platinum_atproto_did_document_url(
                     "did:web:example.com%2Fescape", out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "non-port escape refused");
    check_status(platinum_atproto_did_document_url("did:web:example.com:..",
                                                   out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "dot-dot path refused");
    check_status(platinum_atproto_did_document_url(
                     "did:plc:UPPER", out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "uppercase plc id refused");
    check_status(platinum_atproto_did_document_url("did:key:z6Mkabc", out,
                                                   sizeof(out)),
                 WF_ERR_UNSUPPORTED, "unresolved method refused");
    check_status(platinum_atproto_did_document_url("did:plc:", out,
                                                   sizeof(out)),
                 WF_ERR_INVALID_ARG, "empty plc id refused");
}

static void test_resolve_handle_url(void)
{
    char out[PLATINUM_ATPROTO_URL_MAX];

    check_status(platinum_atproto_resolve_handle_url("https://bsky.social",
                                                     "bsky.app", out,
                                                     sizeof(out)),
                 WF_OK, "handle resolution URL");
    check_str(out,
              "https://bsky.social/xrpc/com.atproto.identity.resolveHandle"
              "?handle=bsky.app",
              "handle resolution URL value");

    check_status(platinum_atproto_resolve_handle_url("https://bsky.social",
                                                     "not a handle", out,
                                                     sizeof(out)),
                 WF_ERR_INVALID_ARG, "invalid handle refused");
    check_status(platinum_atproto_resolve_handle_url("http://bsky.social",
                                                     "bsky.app", out,
                                                     sizeof(out)),
                 WF_ERR_INVALID_ARG, "plain http base refused");
}

static void test_handle_wellknown(void)
{
    char out[PLATINUM_ATPROTO_URL_MAX];

    check_status(platinum_atproto_handle_wellknown_url("Example.Test", out,
                                                       sizeof(out)),
                 WF_OK, "well-known handle URL");
    check_str(out, "https://example.test/.well-known/atproto-did",
              "well-known URL lowercases the host");

    check_status(platinum_atproto_handle_wellknown_did(
                     " did:plc:z72i7hdynmk6r22z27h6tvur\r\n", out,
                     sizeof(out)),
                 WF_OK, "well-known DID with surrounding whitespace");
    check_str(out, "did:plc:z72i7hdynmk6r22z27h6tvur",
              "well-known DID value");
    check_status(platinum_atproto_handle_wellknown_did("at://bsky.app", out,
                                                       sizeof(out)),
                 WF_ERR_PARSE, "non-DID body refused");
    check_status(platinum_atproto_handle_wellknown_did("did:plc:a\ndid:plc:b",
                                                       out, sizeof(out)),
                 WF_ERR_PARSE, "two DIDs refused");
    check_status(platinum_atproto_handle_wellknown_did("  \r\n", out,
                                                       sizeof(out)),
                 WF_ERR_PARSE, "empty body refused");
    check_status(platinum_atproto_handle_wellknown_url("not a handle", out,
                                                       sizeof(out)),
                 WF_ERR_INVALID_ARG, "invalid handle refused");
}

static void test_did_document_pds(void)
{
    static char document[4096];
    char out[PLATINUM_ATPROTO_URL_MAX];

    if (!load_fixture("did_plc_bsky_app.json", document, sizeof(document)))
        return;

    check_status(platinum_atproto_did_pds(document,
                                          "did:plc:z72i7hdynmk6r22z27h6tvur",
                                          out, sizeof(out)),
                 WF_OK, "real PLC document PDS");
    check_str(out, "https://puffball.us-east.host.bsky.network",
              "real PLC document PDS value");

    check_status(platinum_atproto_did_pds(document,
                                          "did:plc:someoneelse", out,
                                          sizeof(out)),
                 WF_ERR_DID_DOCUMENT_NOT_FOUND,
                 "document for another DID refused");
    check_str(out, "", "mismatch empties the output");

    check_status(platinum_atproto_did_pds(document, NULL, out, sizeof(out)),
                 WF_OK, "PDS read without an expected DID");

    if (load_fixture("did_no_pds.json", document, sizeof(document))) {
        check_status(platinum_atproto_did_pds(document, NULL, out,
                                              sizeof(out)),
                     WF_ERR_NOT_FOUND, "http PDS endpoint refused");
    }

    if (load_fixture("did_missing_id.json", document, sizeof(document))) {
        check_status(platinum_atproto_did_pds(document, NULL, out,
                                              sizeof(out)),
                     WF_ERR_PARSE, "document without an id refused");
    }

    check_status(platinum_atproto_did_pds("{\"id\":", NULL, out, sizeof(out)),
                 WF_ERR_PARSE, "truncated document refused");
    check_status(platinum_atproto_did_pds(NULL, NULL, out, sizeof(out)),
                 WF_ERR_INVALID_ARG, "NULL document refused");
}

int main(void)
{
    test_document_urls();
    test_resolve_handle_url();
    test_handle_wellknown();
    test_did_document_pds();

    if (failures != 0) {
        printf("test_atproto_identity: %d of %d checks failed\n", failures,
               checks);
        return 1;
    }
    printf("test_atproto_identity: all %d checks passed\n", checks);
    return 0;
}
