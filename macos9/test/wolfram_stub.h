/*
 * wolfram_stub.h -- stand-ins for the Wolfram transport entry points the Mac
 * client calls, shared by the host-side tests. They record what the client
 * asked for and hand back a scripted response. Not Open Transport, not macTLS.
 */
#ifndef PLATINUM_TEST_WOLFRAM_STUB_H
#define PLATINUM_TEST_WOLFRAM_STUB_H

#include <stddef.h>

extern char last_url[512];
extern char last_body[512];
extern char last_auth[256];
extern char last_nsid[128];
extern size_t last_param_count;
extern char last_param_names[8][64];
extern char last_param_values[8][512];
extern int auth_set;
extern size_t last_max_response_bytes;
extern int last_method;
extern int fake_status;
extern long fake_http_status;
extern int fake_call_count;

/* If set, called before each transport call with the 1-based call number, the
 * method (1 GET, 2 POST), the URL and the request body. It sets the fake_*
 * globals for that call, which is how a refresh-and-retry sequence is driven. */
typedef void (*fake_script_fn)(int call, int method, const char *url,
                               const char *body);
extern fake_script_fn fake_script;
extern const char *fake_body;
/* If non-zero, the response body is exactly this many bytes of fake_body, which
 * may contain NULs; if zero it is the C string. */
extern size_t fake_body_len;

#endif
