/*
 * test_atproto_bootstrap.c -- host tests for the handle-to-PDS sequence.
 *
 * The PLC document fixture is a real public response for bsky.app. The
 * well-known bodies, the did:web document and the resolveHandle answer are
 * synthetic, contain no account credentials and are inline. The test runs
 * from the repository root, which is where CI invokes it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atproto_bootstrap.h"
#include "atproto_url.h"

#define FIXTURE_DIR "macos9/test/fixtures/atproto/"
#define MAX_STEPS 8

typedef struct {
    long status;
    const char *body;
} fetch_step;

static fetch_step steps[MAX_STEPS];
static int step_count;
static int step_index;
static int fetch_calls;
static char fetch_urls[MAX_STEPS][PLATINUM_ATPROTO_URL_MAX];
static size_t fetch_max[MAX_STEPS];

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

static void check_int(long got, long want, const char *what)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s (got %ld, want %ld)\n", what, got, want);
    }
}

static void reset_fetch(void)
{
    step_count = 0;
    step_index = 0;
    fetch_calls = 0;
    memset(fetch_urls, 0, sizeof(fetch_urls));
    memset(fetch_max, 0, sizeof(fetch_max));
}

static void add_step(long status, const char *body)
{
    if (step_count >= MAX_STEPS)
        return;
    steps[step_count].status = status;
    steps[step_count].body = body;
    step_count++;
}

static wf_status test_fetch(void *userdata, const char *url, size_t max_bytes,
                            wf_response *out)
{
    int index;
    size_t length;

    (void)userdata;
    memset(out, 0, sizeof(*out));

    index = step_index;
    fetch_calls++;
    if (index >= step_count)
        return WF_ERR_NETWORK;

    strncpy(fetch_urls[index], url, sizeof(fetch_urls[index]) - 1);
    fetch_urls[index][sizeof(fetch_urls[index]) - 1] = '\0';
    fetch_max[index] = max_bytes;
    step_index++;

    out->status = steps[index].status;
    if (steps[index].body != NULL) {
        length = strlen(steps[index].body);
        out->body = (char *)malloc(length + 1);
        if (out->body != NULL) {
            memcpy(out->body, steps[index].body, length + 1);
            out->body_len = length;
        }
    }

    if (out->status >= 200 && out->status < 300)
        return WF_OK;
    return WF_ERR_HTTP;
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

static const char wellknown_did[] =
    "did:plc:z72i7hdynmk6r22z27h6tvur\n";

static const char web_document[] =
    "{\"id\":\"did:web:example.com:user:alice\","
    "\"alsoKnownAs\":[\"at://example.com\"],"
    "\"service\":[{\"id\":\"#atproto_pds\","
    "\"type\":\"AtprotoPersonalDataServer\","
    "\"serviceEndpoint\":\"https://pds.example.com\"}]}";

static void run_bootstrap(const char *handle, const char *fallback,
                          char *did, char *pds, wf_status *status)
{
    *status = platinum_atproto_bootstrap(test_fetch, NULL, handle, fallback, did,
                                         PLATINUM_ATPROTO_DID_MAX + 1, pds,
                                         PLATINUM_ATPROTO_URL_MAX);
}

static void test_wellknown_path(void)
{
    static char document[8192];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    if (!load_fixture("did_plc_bsky_app.json", document, sizeof(document)))
        return;

    reset_fetch();
    add_step(200, wellknown_did);
    add_step(200, document);
    run_bootstrap("bsky.app", NULL, did, pds, &status);

    check_status(status, WF_OK, "well-known bootstrap succeeds");
    check_str(did, "did:plc:z72i7hdynmk6r22z27h6tvur", "well-known DID");
    check_str(pds, "https://puffball.us-east.host.bsky.network",
              "well-known PDS");
    check_int(fetch_calls, 2, "well-known path makes two requests");
    check_str(fetch_urls[0], "https://bsky.app/.well-known/atproto-did",
              "well-known URL");
    check_str(fetch_urls[1],
              "https://plc.directory/did:plc:z72i7hdynmk6r22z27h6tvur",
              "PLC document URL");
    check_int((long)fetch_max[0], PLATINUM_ATPROTO_WELLKNOWN_MAX,
              "well-known body is capped");
    check_int((long)fetch_max[1], PLATINUM_ATPROTO_DOCUMENT_MAX,
              "DID document body is capped");
}

static void test_entryway_reporting(void)
{
    static char document[8192];
    static char resolve[1024];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    platinum_atproto_entryway entryway;
    wf_status status;

    if (!load_fixture("did_plc_bsky_app.json", document, sizeof(document)))
        return;
    if (!load_fixture("resolve_handle.json", resolve, sizeof(resolve)))
        return;

    reset_fetch();
    add_step(200, wellknown_did);
    add_step(200, document);
    entryway = PLATINUM_ATPROTO_ENTRY_NONE;
    status = platinum_atproto_bootstrap_ex(
        test_fetch, NULL, "bsky.app", NULL, did, sizeof(did), pds,
        sizeof(pds), &entryway);
    check_status(status, WF_OK, "entryway bootstrap succeeds");
    check_int((long)entryway, (long)PLATINUM_ATPROTO_ENTRY_WELLKNOWN,
              "the well-known document is reported");

    reset_fetch();
    add_step(404, "");
    add_step(200, resolve);
    add_step(200, document);
    entryway = PLATINUM_ATPROTO_ENTRY_NONE;
    status = platinum_atproto_bootstrap_ex(
        test_fetch, NULL, "bsky.app", "https://bsky.social", did, sizeof(did),
        pds, sizeof(pds), &entryway);
    check_status(status, WF_OK, "fallback entryway bootstrap succeeds");
    check_int((long)entryway, (long)PLATINUM_ATPROTO_ENTRY_RESOLVE_HANDLE,
              "resolveHandle is reported");

    reset_fetch();
    add_step(404, "");
    entryway = PLATINUM_ATPROTO_ENTRY_WELLKNOWN;
    status = platinum_atproto_bootstrap_ex(
        test_fetch, NULL, "bsky.app", NULL, did, sizeof(did), pds,
        sizeof(pds), &entryway);
    check_status(status, WF_ERR_HANDLE_DOCUMENT_NOT_FOUND,
                 "entryway bootstrap can fail");
    check_int((long)entryway, (long)PLATINUM_ATPROTO_ENTRY_NONE,
              "a failed bootstrap reports no entryway");
}

static void test_resolve_handle_fallback(void)
{
    static char document[8192];
    static char resolve[1024];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    if (!load_fixture("did_plc_bsky_app.json", document, sizeof(document)))
        return;
    if (!load_fixture("resolve_handle.json", resolve, sizeof(resolve)))
        return;

    reset_fetch();
    add_step(404, "");
    add_step(200, resolve);
    add_step(200, document);
    run_bootstrap("bsky.app", "https://bsky.social", did, pds, &status);

    check_status(status, WF_OK, "resolveHandle fallback succeeds");
    check_str(did, "did:plc:z72i7hdynmk6r22z27h6tvur", "fallback DID");
    check_str(pds, "https://puffball.us-east.host.bsky.network",
              "fallback PDS");
    check_int(fetch_calls, 3, "fallback makes three requests");
    check_str(fetch_urls[1],
              "https://bsky.social/xrpc/com.atproto.identity.resolveHandle"
              "?handle=bsky.app",
              "resolveHandle URL");
}

static void test_malformed_wellknown_falls_back(void)
{
    static char document[8192];
    static char resolve[1024];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    if (!load_fixture("did_plc_bsky_app.json", document, sizeof(document)))
        return;
    if (!load_fixture("resolve_handle.json", resolve, sizeof(resolve)))
        return;

    reset_fetch();
    add_step(200, "not a DID\n");
    add_step(200, resolve);
    add_step(200, document);
    run_bootstrap("bsky.app", "https://bsky.social", did, pds, &status);

    check_status(status, WF_OK, "malformed well-known falls back");
    check_int(fetch_calls, 3, "malformed well-known makes three requests");
    check_str(did, "did:plc:z72i7hdynmk6r22z27h6tvur",
              "fallback DID after a malformed document");
}

static void test_no_fallback(void)
{
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    reset_fetch();
    add_step(404, "");
    run_bootstrap("bsky.app", NULL, did, pds, &status);

    check_status(status, WF_ERR_HANDLE_DOCUMENT_NOT_FOUND,
                 "missing well-known without a fallback");
    check_int(fetch_calls, 1, "no fallback means one request");
    check_str(did, "", "failed bootstrap leaves the DID empty");
    check_str(pds, "", "failed bootstrap leaves the PDS empty");

    reset_fetch();
    add_step(404, "");
    run_bootstrap("bsky.app", "", did, pds, &status);
    check_status(status, WF_ERR_HANDLE_DOCUMENT_NOT_FOUND,
                 "empty fallback is treated as none");
}

static void test_document_does_not_claim_handle(void)
{
    static char document[8192];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    if (!load_fixture("did_plc_bsky_app.json", document, sizeof(document)))
        return;

    reset_fetch();
    add_step(200, wellknown_did);
    add_step(200, document);
    run_bootstrap("example.com", NULL, did, pds, &status);

    check_status(status, WF_ERR_HANDLE_RESOLVE,
                 "document for another handle is refused");
    check_str(did, "", "refused document leaves the DID empty");
    check_str(pds, "", "refused document leaves the PDS empty");
}

static void test_document_id_mismatch(void)
{
    static char document[8192];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    if (!load_fixture("did_plc_bsky_app.json", document, sizeof(document)))
        return;

    reset_fetch();
    add_step(200, "did:plc:someoneelse\n");
    add_step(200, document);
    run_bootstrap("bsky.app", NULL, did, pds, &status);

    check_status(status, WF_ERR_DID_DOCUMENT_NOT_FOUND,
                 "a document for another DID is refused");
}

static void test_did_web(void)
{
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    reset_fetch();
    add_step(200, "did:web:example.com:user:alice");
    add_step(200, web_document);
    run_bootstrap("example.com", NULL, did, pds, &status);

    check_status(status, WF_OK, "did:web bootstrap succeeds");
    check_str(did, "did:web:example.com:user:alice", "did:web DID");
    check_str(pds, "https://pds.example.com", "did:web PDS");
    check_str(fetch_urls[1], "https://example.com/user/alice/did.json",
              "did:web document URL");
}

static void test_document_missing(void)
{
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    reset_fetch();
    add_step(200, wellknown_did);
    add_step(404, "");
    run_bootstrap("bsky.app", NULL, did, pds, &status);

    check_status(status, WF_ERR_DID_DOCUMENT_NOT_FOUND,
                 "missing DID document is reported");
}

static void test_fallback_failure(void)
{
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    reset_fetch();
    add_step(404, "");
    add_step(500, "");
    run_bootstrap("bsky.app", "https://bsky.social", did, pds, &status);

    check_status(status, WF_ERR_HTTP, "fallback server failure is reported");
    check_int(fetch_calls, 2, "failed fallback makes two requests");
}

static void test_invalid_input(void)
{
    static char document[8192];
    char did[PLATINUM_ATPROTO_DID_MAX + 1];
    char pds[PLATINUM_ATPROTO_URL_MAX];
    wf_status status;

    reset_fetch();
    run_bootstrap("not a handle", "https://bsky.social", did, pds, &status);
    check_status(status, WF_ERR_INVALID_ARG, "invalid handle is refused");
    check_int(fetch_calls, 0, "invalid handle makes no request");

    reset_fetch();
    status = platinum_atproto_bootstrap(NULL, NULL, "bsky.app", NULL, did,
                                        sizeof(did), pds, sizeof(pds));
    check_status(status, WF_ERR_INVALID_ARG, "missing fetch is refused");

    if (load_fixture("did_plc_bsky_app.json", document, sizeof(document))) {
        reset_fetch();
        add_step(200, wellknown_did);
        add_step(200, document);
        status = platinum_atproto_bootstrap(test_fetch, NULL, "bsky.app", NULL,
                                            did, 4, pds, sizeof(pds));
        check_status(status, WF_ERR_ALLOC,
                     "a too-small DID output is refused");
        check_str(did, "", "too-small DID output is emptied");
    }
}

int main(void)
{
    test_wellknown_path();
    test_entryway_reporting();
    test_resolve_handle_fallback();
    test_malformed_wellknown_falls_back();
    test_no_fallback();
    test_document_does_not_claim_handle();
    test_document_id_mismatch();
    test_did_web();
    test_document_missing();
    test_fallback_failure();
    test_invalid_input();

    if (failures != 0) {
        printf("test_atproto_bootstrap: %d of %d checks failed\n", failures,
               checks);
        return 1;
    }
    printf("test_atproto_bootstrap: all %d checks passed\n", checks);
    return 0;
}
